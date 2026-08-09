#include "xdna/runtime.hpp"

#include "xdna/buffers.hpp"
#include "xdna/errors.hpp"

#include <algorithm>
#include <chrono>
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

XdnaRuntime::XdnaRuntime(const SmokeArtifact& artifact,
                         const std::string& selector,
                         WorkloadKind workload,
                         std::size_t m5_batch_size,
                         std::size_t m5_columns)
    : artifact_(artifact),
      workload_(workload),
      capability_(probe_device(selector)),
      device_(),
      xclbin_(),
      hardware_context_(),
      kernel_(),
      instructions_(),
      instruction_bo_(),
      input_bo_(),
      output_bo_(),
      k1_input_bo_(),
      k1_next_state_bo_(),
      k1_device_input_(),
      m4_input_bo_(),
      m4_output_bo_(),
      m4_device_input_(),
      m5_input_bo_(),
      m5_output_bo_(),
      m5_device_input_(),
      m5_batch_size_(m5_batch_size),
      m5_columns_(m5_columns),
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
    if (workload_ == WorkloadKind::M5
        && (m5_columns_ == 0U || m5_columns_ > 4U || m5_columns_ == 3U
            || m5_batch_size_ == 0U || m5_batch_size_ > kM5MaximumBatchSize
            || (m5_batch_size_ % m5_columns_) != 0U)) {
        throw RuntimeError(
            ErrorCode::InvalidArgument,
            "M5 runtime requires a batch size in 1..16 divisible by one, two, or four columns");
    }
    if (workload_ == WorkloadKind::K1 || workload_ == WorkloadKind::M4
        || workload_ == WorkloadKind::M5) {
        if (!std::filesystem::is_regular_file(artifact_.manifest)) {
            throw RuntimeError(
                ErrorCode::ArtifactMissing,
                (workload_ == WorkloadKind::K1
                     ? "K1 artifact manifest is missing: "
                     : workload_ == WorkloadKind::M4 ? "M4 artifact manifest is missing: "
                                                    : "M5 artifact manifest is missing: ")
                    + artifact_path_message(artifact_.manifest));
        }
        std::ifstream manifest(artifact_.manifest);
        std::string marker;
        std::string expected_marker;
        if (workload_ == WorkloadKind::K1) {
            expected_marker = "artifact_kind=xdna-npu-miner-m3-k1-v1";
        } else if (workload_ == WorkloadKind::M4) {
            expected_marker = "artifact_kind=xdna-npu-miner-m4-score-v1";
        } else {
            expected_marker = "artifact_kind=xdna-npu-miner-m5-batch-v1 batch_size="
                + std::to_string(m5_batch_size_) + " columns=" + std::to_string(m5_columns_);
        }
        if (!manifest || !std::getline(manifest, marker) || marker != expected_marker) {
            throw RuntimeError(
                ErrorCode::ArtifactInvalid,
                workload_ == WorkloadKind::K1
                    ? "K1 artifact manifest does not identify the project K1 artifact"
                    : workload_ == WorkloadKind::M4
                        ? "M4 artifact manifest does not identify the project M4 artifact"
                        : "M5 artifact manifest does not identify the requested batch/column artifact");
        }
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
        const std::size_t required_kernel_args = 5U;
        if (found->get_num_args() < required_kernel_args) {
            throw RuntimeError(
                ErrorCode::ArtifactInvalid,
                workload_ == WorkloadKind::K1
                    ? "K1 MLIR_AIE kernel has fewer than the required opcode/instruction/input/output arguments"
                    : "MLIR_AIE kernel has fewer than the required opcode/instruction/input/output arguments");
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
        if (workload_ == WorkloadKind::Smoke) {
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
                throw RuntimeError(
                    ErrorCode::ArtifactInvalid,
                    "XRT allocated a smoke buffer smaller than its logical contract");
            }
        } else if (workload_ == WorkloadKind::K1) {
            const K1DeviceLayout layout = k1_default_layout();
            k1_input_bo_ = xrt::bo(
                device_,
                layout.input_buffer_bytes,
                XRT_BO_FLAGS_HOST_ONLY,
                kernel_.group_id(3));
            k1_next_state_bo_ = xrt::bo(
                device_,
                layout.state_stride_bytes,
                XRT_BO_FLAGS_HOST_ONLY,
                kernel_.group_id(4));
            k1_device_input_.assign(layout.input_buffer_bytes, 0xA5U);

            if (instruction_bo_.size() < instructions_.size() * sizeof(std::uint32_t)
                || k1_input_bo_.size() < layout.input_buffer_bytes
                || k1_next_state_bo_.size() < layout.state_stride_bytes) {
                throw RuntimeError(
                    ErrorCode::ArtifactInvalid,
                    "XRT allocated a K1 buffer smaller than its logical/device contract");
            }
        } else if (workload_ == WorkloadKind::M4) {
            const M4DeviceLayout layout = m4_default_layout();
            m4_input_bo_ = xrt::bo(
                device_,
                layout.input_buffer_bytes,
                XRT_BO_FLAGS_HOST_ONLY,
                kernel_.group_id(3));
            m4_output_bo_ = xrt::bo(
                device_,
                layout.output_buffer_bytes,
                XRT_BO_FLAGS_HOST_ONLY,
                kernel_.group_id(4));
            m4_device_input_.assign(layout.input_buffer_bytes, 0xA5U);

            if (instruction_bo_.size() < instructions_.size() * sizeof(std::uint32_t)
                || m4_input_bo_.size() < layout.input_buffer_bytes
                || m4_output_bo_.size() < layout.output_buffer_bytes) {
                throw RuntimeError(
                    ErrorCode::ArtifactInvalid,
                    "XRT allocated an M4 buffer smaller than its logical/device contract");
            }
        } else {
            const M5DeviceLayout layout = m5_default_layout(m5_batch_size_);
            m5_input_bo_ = xrt::bo(
                device_,
                layout.input_buffer_bytes,
                XRT_BO_FLAGS_HOST_ONLY,
                kernel_.group_id(3));
            m5_output_bo_ = xrt::bo(
                device_,
                layout.output_buffer_bytes,
                XRT_BO_FLAGS_HOST_ONLY,
                kernel_.group_id(4));
            m5_device_input_.assign(layout.input_buffer_bytes, 0xA5U);

            if (instruction_bo_.size() < instructions_.size() * sizeof(std::uint32_t)
                || m5_input_bo_.size() < layout.input_buffer_bytes
                || m5_output_bo_.size() < layout.output_buffer_bytes) {
                throw RuntimeError(
                    ErrorCode::ArtifactInvalid,
                    "XRT allocated an M5 buffer smaller than its logical/device contract");
            }
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
        counters_.h2d_bytes += 2U * kSmokeBufferBytes;
        ++counters_.dispatches;

        const std::uint32_t opcode = 3U;
        ert_cmd_state status = ERT_CMD_STATE_ERROR;
        try {
            const auto dispatch_started = std::chrono::steady_clock::now();
            xrt::run run = kernel_(opcode, instruction_bo_, instructions_.size(), input_bo_, output_bo_);
            status = run.wait();
            counters_.dispatch_wait_ns += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - dispatch_started)
                    .count());
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
        counters_.d2h_bytes += kSmokeBufferBytes;
        std::memcpy(output.data(), output_map, kSmokeBufferBytes);
    } catch (const RuntimeError&) {
        throw;
    } catch (const std::exception& error) {
        throw RuntimeError(
            ErrorCode::SynchronizationFailed,
            "XRT buffer synchronization or dispatch failed: " + exception_message(error));
    }
}

void XdnaRuntime::dispatch_k1(K1PackedBuffers& buffers, const K1DeviceLayout& layout)
{
    if (workload_ != WorkloadKind::K1) {
        throw RuntimeError(ErrorCode::InvalidArgument, "K1 dispatch requested from a non-K1 runtime");
    }
    try {
        validate_k1_packed_input(buffers, layout);
    } catch (const K1ContractError& error) {
        throw RuntimeError(ErrorCode::InvalidBuffer, std::string("K1 buffer contract rejected: ") + error.what());
    }

    try {
        if (k1_device_input_.size() != layout.input_buffer_bytes) {
            throw RuntimeError(ErrorCode::InvalidBuffer, "K1 runtime input arena does not match its layout");
        }
        auto* input_map = k1_input_bo_.map<bpp9000::Byte*>();
        auto* next_state_map = k1_next_state_bo_.map<bpp9000::Byte*>();

        std::fill(k1_device_input_.begin(), k1_device_input_.end(), static_cast<bpp9000::Byte>(0xA5U));
        std::memcpy(k1_device_input_.data() + layout.state_device_offset,
                    buffers.previous_state.data(),
                    buffers.previous_state.size() * sizeof(bpp9000::Byte));
        std::memcpy(k1_device_input_.data() + layout.lut_device_offset,
                    buffers.lut.data(),
                    buffers.lut.size() * sizeof(bpp9000::Byte));
        std::memcpy(k1_device_input_.data() + layout.neighbors_device_offset,
                    buffers.neighbors.data(),
                    buffers.neighbors.size() * sizeof(std::uint32_t));
        std::memcpy(k1_device_input_.data() + layout.updated_device_offset,
                    buffers.updated_neurons.data(),
                    buffers.updated_neurons.size() * sizeof(std::uint32_t));
        std::fill(next_state_map,
                  next_state_map + buffers.next_state.size(),
                  static_cast<bpp9000::Byte>(0x5AU));

        std::memcpy(input_map, k1_device_input_.data(), k1_device_input_.size());
        k1_input_bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE, k1_device_input_.size(), 0U);
        k1_next_state_bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE, buffers.next_state.size(), 0U);
        counters_.h2d_syncs += 2U;
        counters_.h2d_bytes += k1_device_input_.size() + buffers.next_state.size();
        ++counters_.dispatches;

        const std::uint32_t opcode = 3U;
        ert_cmd_state status = ERT_CMD_STATE_ERROR;
        try {
            const auto dispatch_started = std::chrono::steady_clock::now();
            xrt::run run = kernel_(
                opcode,
                instruction_bo_,
                instructions_.size(),
                k1_input_bo_,
                k1_next_state_bo_);
            status = run.wait();
            counters_.dispatch_wait_ns += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - dispatch_started)
                    .count());
        } catch (const std::exception& error) {
            throw RuntimeError(
                ErrorCode::DeviceExecutionFailed,
                "XRT K1 kernel dispatch failed: " + exception_message(error));
        }
        if (status != ERT_CMD_STATE_COMPLETED) {
            throw RuntimeError(
                ErrorCode::DeviceExecutionFailed,
                "XRT K1 run did not complete; status="
                    + std::to_string(static_cast<unsigned int>(status)));
        }
        ++counters_.completed_dispatches;

        k1_next_state_bo_.sync(XCL_BO_SYNC_BO_FROM_DEVICE, buffers.next_state.size(), 0U);
        ++counters_.d2h_syncs;
        counters_.d2h_bytes += buffers.next_state.size();
        std::memcpy(
            buffers.next_state.data(),
            next_state_map,
            buffers.next_state.size() * sizeof(bpp9000::Byte));
        try {
            validate_k1_output(buffers.next_state, layout);
        } catch (const K1ContractError& error) {
            throw RuntimeError(
                ErrorCode::DeviceExecutionFailed,
                std::string("K1 device returned an invalid output: ") + error.what());
        }
    } catch (const RuntimeError&) {
        throw;
    } catch (const std::exception& error) {
        throw RuntimeError(
            ErrorCode::SynchronizationFailed,
            "XRT K1 buffer synchronization or dispatch failed: " + exception_message(error));
    }
}

void XdnaRuntime::dispatch_m4(M4PackedBuffers& buffers, const M4DeviceLayout& layout)
{
    if (workload_ != WorkloadKind::M4) {
        throw RuntimeError(ErrorCode::InvalidArgument, "M4 dispatch requested from a non-M4 runtime");
    }
    try {
        validate_m4_packed_input(buffers, layout);
    } catch (const M4ContractError& error) {
        throw RuntimeError(ErrorCode::InvalidBuffer, std::string("M4 buffer contract rejected: ") + error.what());
    }

    try {
        if (m4_device_input_.size() != layout.input_buffer_bytes) {
            throw RuntimeError(ErrorCode::InvalidBuffer, "M4 runtime input arena does not match its layout");
        }
        auto* input_map = m4_input_bo_.map<bpp9000::Byte*>();
        auto* output_map = m4_output_bo_.map<bpp9000::Byte*>();

        std::fill(m4_device_input_.begin(), m4_device_input_.end(), static_cast<bpp9000::Byte>(0xA5U));
        std::memcpy(m4_device_input_.data(), buffers.input.data(), buffers.input.size());
        std::fill(output_map,
                  output_map + buffers.output.size(),
                  static_cast<bpp9000::Byte>(0x5AU));

        std::memcpy(input_map, m4_device_input_.data(), m4_device_input_.size());
        m4_input_bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE, m4_device_input_.size(), 0U);
        m4_output_bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE, buffers.output.size(), 0U);
        counters_.h2d_syncs += 2U;
        counters_.h2d_bytes += m4_device_input_.size() + buffers.output.size();
        ++counters_.dispatches;

        const std::uint32_t opcode = 3U;
        ert_cmd_state status = ERT_CMD_STATE_ERROR;
        try {
            const auto dispatch_started = std::chrono::steady_clock::now();
            xrt::run run = kernel_(
                opcode,
                instruction_bo_,
                instructions_.size(),
                m4_input_bo_,
                m4_output_bo_);
            status = run.wait();
            counters_.dispatch_wait_ns += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - dispatch_started)
                    .count());
        } catch (const std::exception& error) {
            throw RuntimeError(
                ErrorCode::DeviceExecutionFailed,
                "XRT M4 kernel dispatch failed: " + exception_message(error));
        }
        if (status != ERT_CMD_STATE_COMPLETED) {
            throw RuntimeError(
                ErrorCode::DeviceExecutionFailed,
                "XRT M4 run did not complete; status="
                    + std::to_string(static_cast<unsigned int>(status)));
        }
        ++counters_.completed_dispatches;

        m4_output_bo_.sync(XCL_BO_SYNC_BO_FROM_DEVICE, buffers.output.size(), 0U);
        ++counters_.d2h_syncs;
        counters_.d2h_bytes += buffers.output.size();
        std::memcpy(buffers.output.data(), output_map, buffers.output.size());
        try {
            validate_m4_output(buffers.output, layout);
        } catch (const M4ContractError& error) {
            throw RuntimeError(
                ErrorCode::DeviceExecutionFailed,
                std::string("M4 device returned an invalid output: ") + error.what());
        }
    } catch (const RuntimeError&) {
        throw;
    } catch (const std::exception& error) {
        throw RuntimeError(
            ErrorCode::SynchronizationFailed,
            "XRT M4 buffer synchronization or dispatch failed: " + exception_message(error));
    }
}

void XdnaRuntime::dispatch_m5(M5PackedBatch& batch, const M5DeviceLayout& layout)
{
    if (workload_ != WorkloadKind::M5) {
        throw RuntimeError(ErrorCode::InvalidArgument, "M5 dispatch requested from a non-M5 runtime");
    }
    try {
        validate_m5_packed_input(batch, layout);
    } catch (const M5ContractError& error) {
        throw RuntimeError(ErrorCode::InvalidBuffer, std::string("M5 buffer contract rejected: ") + error.what());
    }

    try {
        if (layout.batch_size != m5_batch_size_
            || m5_device_input_.size() != layout.input_buffer_bytes) {
            throw RuntimeError(ErrorCode::InvalidBuffer, "M5 runtime arena does not match its fixed artifact batch");
        }
        auto* input_map = m5_input_bo_.map<bpp9000::Byte*>();
        auto* output_map = m5_output_bo_.map<bpp9000::Byte*>();

        std::fill(m5_device_input_.begin(), m5_device_input_.end(), static_cast<bpp9000::Byte>(0xA5U));
        std::memcpy(m5_device_input_.data(), batch.input.data(), batch.input.size());
        std::fill(output_map,
                  output_map + batch.output.size(),
                  static_cast<bpp9000::Byte>(0x5AU));

        std::memcpy(input_map, m5_device_input_.data(), m5_device_input_.size());
        m5_input_bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE, m5_device_input_.size(), 0U);
        m5_output_bo_.sync(XCL_BO_SYNC_BO_TO_DEVICE, batch.output.size(), 0U);
        counters_.h2d_syncs += 2U;
        counters_.h2d_bytes += m5_device_input_.size() + batch.output.size();
        ++counters_.dispatches;

        const std::uint32_t opcode = 3U;
        ert_cmd_state status = ERT_CMD_STATE_ERROR;
        try {
            const auto dispatch_started = std::chrono::steady_clock::now();
            xrt::run run = kernel_(
                opcode,
                instruction_bo_,
                instructions_.size(),
                m5_input_bo_,
                m5_output_bo_);
            status = run.wait();
            counters_.dispatch_wait_ns += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - dispatch_started)
                    .count());
        } catch (const std::exception& error) {
            throw RuntimeError(
                ErrorCode::DeviceExecutionFailed,
                "XRT M5 kernel dispatch failed: " + exception_message(error));
        }
        if (status != ERT_CMD_STATE_COMPLETED) {
            throw RuntimeError(
                ErrorCode::DeviceExecutionFailed,
                "XRT M5 run did not complete; status="
                    + std::to_string(static_cast<unsigned int>(status)));
        }
        ++counters_.completed_dispatches;

        m5_output_bo_.sync(XCL_BO_SYNC_BO_FROM_DEVICE, batch.output.size(), 0U);
        ++counters_.d2h_syncs;
        counters_.d2h_bytes += batch.output.size();
        std::memcpy(batch.output.data(), output_map, batch.output.size());
        for (std::size_t index = 0U; index < layout.batch_size; ++index) {
            const std::size_t output_offset = index * layout.output_item_stride_bytes;
            try {
                validate_m5_output(std::span<const bpp9000::Byte>(batch.output).subspan(
                    output_offset,
                    layout.output_item_stride_bytes));
            } catch (const M5ContractError& error) {
                throw RuntimeError(
                    ErrorCode::DeviceExecutionFailed,
                    std::string("M5 device returned an invalid item output: ") + error.what());
            }
        }
    } catch (const RuntimeError&) {
        throw;
    } catch (const std::exception& error) {
        throw RuntimeError(
            ErrorCode::SynchronizationFailed,
            "XRT M5 buffer synchronization or dispatch failed: " + exception_message(error));
    }
}

} // namespace xdna::runtime
