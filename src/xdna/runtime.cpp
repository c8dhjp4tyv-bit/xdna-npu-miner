#include "xdna/runtime.hpp"

#include "xdna/buffers.hpp"
#include "xdna/errors.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace xdna::runtime {
namespace {

[[nodiscard]] std::string artifact_path_message(const std::filesystem::path& path)
{
    return path.string().empty() ? "<empty>" : path.string();
}

[[nodiscard]] std::string exception_message(const std::exception& error)
{
    return error.what() == nullptr ? "unknown XRT exception" : error.what();
}

[[nodiscard]] ErrorCode capability_error_code(CapabilityStatus status) noexcept
{
    switch (status) {
    case CapabilityStatus::NoXdnaDevice:
        return ErrorCode::NoXdnaDevice;
    case CapabilityStatus::WrongXdnaGeneration:
        return ErrorCode::WrongXdnaGeneration;
    case CapabilityStatus::XrtUnavailable:
        return ErrorCode::XrtUnavailable;
    case CapabilityStatus::DriverUnavailable:
        return ErrorCode::DriverUnavailable;
    case CapabilityStatus::FirmwareUnavailableOrUnknown:
        return ErrorCode::FirmwareUnavailableOrUnknown;
    case CapabilityStatus::ToolchainUnavailable:
        return ErrorCode::ToolchainUnavailable;
    case CapabilityStatus::RuntimeVersionMismatch:
        return ErrorCode::RuntimeVersionMismatch;
    case CapabilityStatus::DeviceOpenFailed:
        return ErrorCode::DeviceOpenFailed;
    case CapabilityStatus::SupportedXdna1:
        return ErrorCode::XrtUnavailable;
    }
    return ErrorCode::XrtUnavailable;
}

} // namespace

XdnaRuntime::XdnaRuntime(const SmokeArtifact& artifact, const std::string& selector)
    : artifact_(artifact),
      capability_(probe_device(selector)),
      device_(),
      xclbin_(),
      hardware_context_(),
      kernel_(),
      instructions_(),
      instruction_bo_(),
      input_bo_(),
      output_bo_(),
      kernel_name_(),
      artifact_uuid_(),
      counters_()
{
    if (!capability_.supported()) {
        throw RuntimeError(
            capability_error_code(capability_.status),
            std::string(capability_status_name(capability_.status)) + ": " + capability_.detail);
    }
    if (!std::filesystem::is_regular_file(artifact_.xclbin)) {
        throw RuntimeError(
            ErrorCode::ArtifactMissing,
            "xclbin artifact is missing: " + artifact_path_message(artifact_.xclbin));
    }
    if (!std::filesystem::is_regular_file(artifact_.instructions)) {
        throw RuntimeError(
            ErrorCode::ArtifactMissing,
            "instruction artifact is missing: " + artifact_path_message(artifact_.instructions));
    }

    bool context_setup_started = false;
    try {
        device_ = open_device(selector);
        xclbin_ = xrt::xclbin(artifact_.xclbin.string());
        const auto kernels = xclbin_.get_kernels();
        const auto found = std::find_if(
            kernels.begin(),
            kernels.end(),
            [](const xrt::xclbin::kernel& candidate) {
                return candidate.get_name().rfind("MLIR_AIE", 0U) == 0U;
            });
        if (found == kernels.end()) {
            throw RuntimeError(ErrorCode::ArtifactInvalid, "xclbin has no MLIR_AIE kernel");
        }
        if (found->get_num_args() < 5U) {
            throw RuntimeError(
                ErrorCode::ArtifactInvalid,
                "MLIR_AIE kernel has fewer than the required opcode/instruction/input/output arguments");
        }

        kernel_name_ = found->get_name();
        artifact_uuid_ = xclbin_.get_uuid().to_string();
        instructions_ = load_instructions(artifact_.instructions);
        if (instructions_.empty()) {
            throw RuntimeError(ErrorCode::ArtifactInvalid, "instruction artifact is empty");
        }

        context_setup_started = true;
        device_.register_xclbin(xclbin_);
        hardware_context_ = xrt::hw_context(device_, xclbin_.get_uuid());
        kernel_ = xrt::kernel(hardware_context_, kernel_name_);
        instruction_bo_ = xrt::bo(
            device_,
            instructions_.size() * sizeof(std::uint32_t),
            XCL_BO_FLAGS_CACHEABLE,
            kernel_.group_id(1));
        input_bo_ = xrt::bo(
            device_,
            kSmokeBufferBytes,
            XRT_BO_FLAGS_HOST_ONLY,
            kernel_.group_id(3));
        output_bo_ = xrt::bo(
            device_,
            kSmokeBufferBytes,
            XRT_BO_FLAGS_HOST_ONLY,
            kernel_.group_id(4));

        if (instruction_bo_.size() < instructions_.size() * sizeof(std::uint32_t)
            || input_bo_.size() < kSmokeBufferBytes
            || output_bo_.size() < kSmokeBufferBytes) {
            throw RuntimeError(ErrorCode::ArtifactInvalid, "XRT allocated a buffer smaller than its logical contract");
        }

        auto* instruction_map = instruction_bo_.map<std::uint32_t*>();
        std::memcpy(
            instruction_map,
            instructions_.data(),
            instructions_.size() * sizeof(std::uint32_t));
        instruction_bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    } catch (const RuntimeError&) {
        throw;
    } catch (const std::exception& error) {
        throw RuntimeError(
            context_setup_started ? ErrorCode::RuntimeContextCreationFailed : ErrorCode::ArtifactInvalid,
            context_setup_started
                ? "XRT hardware context or buffer setup failed: " + exception_message(error)
                : "XRT artifact setup failed: " + exception_message(error));
    }
}

xrt::device XdnaRuntime::open_device(const std::string& selector)
{
    if (selector == "0" || selector == "accel0") {
        return xrt::device(0U);
    }
    return xrt::device(selector);
}

std::vector<std::uint32_t> XdnaRuntime::load_instructions(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        throw RuntimeError(
            ErrorCode::ArtifactMissing,
            "cannot open instruction artifact: " + artifact_path_message(path));
    }
    const std::streampos end = stream.tellg();
    if (end < 0) {
        throw RuntimeError(ErrorCode::ArtifactInvalid, "cannot determine instruction artifact size");
    }
    const auto byte_count = static_cast<std::uintmax_t>(end);
    if (byte_count == 0U || (byte_count % sizeof(std::uint32_t)) != 0U) {
        throw RuntimeError(
            ErrorCode::ArtifactInvalid,
            "instruction artifact size is not a non-zero uint32 sequence");
    }
    stream.seekg(0);
    std::vector<std::uint32_t> result(static_cast<std::size_t>(byte_count / sizeof(std::uint32_t)));
    stream.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(byte_count));
    if (!stream) {
        throw RuntimeError(ErrorCode::ArtifactInvalid, "instruction artifact read was incomplete");
    }
    return result;
}

void XdnaRuntime::dispatch(std::span<const std::int32_t> input,
                           std::span<std::int32_t> output)
{
    if (input.size() != output.size()) {
        throw RuntimeError(ErrorCode::InvalidBuffer, "input and output element counts differ");
    }
    const auto input_validation = validate_smoke_buffer(
        input.size(),
        input.size_bytes(),
        reinterpret_cast<std::uintptr_t>(input.data()));
    const auto output_validation = validate_smoke_buffer(
        output.size(),
        output.size_bytes(),
        reinterpret_cast<std::uintptr_t>(output.data()));
    if (input_validation != BufferValidation::Valid
        || output_validation != BufferValidation::Valid) {
        std::ostringstream message;
        message << "buffer contract rejected input=" << buffer_validation_name(input_validation)
                << " output=" << buffer_validation_name(output_validation)
                << " expected_elements=" << kSmokeElementCount
                << " expected_bytes=" << kSmokeBufferBytes
                << " alignment=" << kSmokeAlignmentBytes;
        throw RuntimeError(ErrorCode::InvalidBuffer, message.str());
    }

    try {
        auto* input_map = input_bo_.map<std::int32_t*>();
        auto* output_map = output_bo_.map<std::int32_t*>();
        std::memcpy(input_map, input.data(), kSmokeBufferBytes);
        std::fill(output_map, output_map + kSmokeElementCount, static_cast<std::int32_t>(0x5A5A5A5A));

        input_bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE, kSmokeBufferBytes, 0U);
        output_bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE, kSmokeBufferBytes, 0U);
        counters_.h2d_syncs += 2U;
        ++counters_.dispatches;

        const std::uint32_t opcode = 3U;
        ert_cmd_state status = ERT_CMD_STATE_ERROR;
        try {
            xrt::run run = kernel_(opcode, instruction_bo_, instructions_.size(), input_bo_, output_bo_);
            status = run.wait();
        } catch (const std::exception& error) {
            throw RuntimeError(
                ErrorCode::DeviceExecutionFailed,
                "XRT kernel dispatch failed: " + exception_message(error));
        }
        if (status != ERT_CMD_STATE_COMPLETED) {
            throw RuntimeError(
                ErrorCode::DeviceExecutionFailed,
                "XRT run did not complete; status=" + std::to_string(static_cast<unsigned int>(status)));
        }
        ++counters_.completed_dispatches;

        output_bo_.sync(XCL_BO_SYNC_BO_FROM_DEVICE, kSmokeBufferBytes, 0U);
        ++counters_.d2h_syncs;
        std::memcpy(output.data(), output_map, kSmokeBufferBytes);
    } catch (const RuntimeError&) {
        throw;
    } catch (const std::exception& error) {
        throw RuntimeError(
            ErrorCode::SynchronizationFailed,
            "XRT buffer synchronization or dispatch failed: " + exception_message(error));
    }
}

} // namespace xdna::runtime
