#include "pearl/xdna_matmul.hpp"

#include "xdna/errors.hpp"

#include "xrt/experimental/xrt_xclbin.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_hw_context.h"
#include "xrt/xrt_kernel.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace xdna::pearl {
namespace {

constexpr char kManifestMarker[] = "artifact_kind=pearl-xdna-gemm-p2-v1";

[[nodiscard]] std::string exception_message(const std::exception& error)
{
    return error.what() == nullptr ? "unknown XRT exception" : error.what();
}

[[nodiscard]] std::string artifact_path_message(const std::filesystem::path& path)
{
    return path.string().empty() ? "<empty>" : path.string();
}

[[nodiscard]] std::vector<std::uint32_t> load_instructions(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        throw runtime::RuntimeError(
            runtime::ErrorCode::ArtifactMissing,
            "cannot open instruction artifact: " + artifact_path_message(path));
    }
    const std::streampos end = stream.tellg();
    if (end < 0) {
        throw runtime::RuntimeError(
            runtime::ErrorCode::ArtifactInvalid,
            "cannot determine instruction artifact size");
    }
    const auto byte_count = static_cast<std::uintmax_t>(end);
    if (byte_count == 0U || (byte_count % sizeof(std::uint32_t)) != 0U
        || byte_count > std::numeric_limits<std::size_t>::max()) {
        throw runtime::RuntimeError(
            runtime::ErrorCode::ArtifactInvalid,
            "instruction artifact is not a non-empty uint32 sequence");
    }
    stream.seekg(0);
    std::vector<std::uint32_t> result(static_cast<std::size_t>(byte_count / sizeof(std::uint32_t)));
    stream.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(byte_count));
    if (!stream) {
        throw runtime::RuntimeError(
            runtime::ErrorCode::ArtifactInvalid,
            "instruction artifact read was incomplete");
    }
    return result;
}

void require_fixed_matrix_shapes(const Int8Matrix& left, const Int8Matrix& right)
{
    if (left.rows() != kP2Rows || left.cols() != kP2Common
        || right.rows() != kP2Common || right.cols() != kP2Columns) {
        std::ostringstream message;
        message << "P2 GEMM requires A=" << kP2Rows << "x" << kP2Common
                << " and B=" << kP2Common << "x" << kP2Columns
                << ", got A=" << left.rows() << "x" << left.cols()
                << " and B=" << right.rows() << "x" << right.cols();
        throw runtime::RuntimeError(runtime::ErrorCode::InvalidBuffer, message.str());
    }
    left.require_signal_range();
    right.require_signal_range();
}

} // namespace

struct XdnaMatmulExecutor::Impl {
    explicit Impl(const XdnaMatmulArtifact& selected_artifact, const std::string& selector)
        : artifact(selected_artifact),
          capability(runtime::probe_device(selector)),
          device(),
          xclbin(),
          hardware_context(),
          kernel(),
          instructions(),
          instruction_bo(),
          left_bo(),
          right_bo(),
          output_bo(),
          kernel_name(),
          artifact_uuid(),
          counters()
    {
        if (!capability.supported()) {
            throw runtime::RuntimeError(
                runtime::ErrorCode::DeviceOpenFailed,
                std::string(runtime::capability_status_name(capability.status))
                    + ": " + capability.detail);
        }
        if (!std::filesystem::is_regular_file(artifact.xclbin)) {
            throw runtime::RuntimeError(
                runtime::ErrorCode::ArtifactMissing,
                "xclbin artifact is missing: " + artifact_path_message(artifact.xclbin));
        }
        if (!std::filesystem::is_regular_file(artifact.instructions)) {
            throw runtime::RuntimeError(
                runtime::ErrorCode::ArtifactMissing,
                "instruction artifact is missing: " + artifact_path_message(artifact.instructions));
        }
        if (!std::filesystem::is_regular_file(artifact.manifest)) {
            throw runtime::RuntimeError(
                runtime::ErrorCode::ArtifactMissing,
                "P2 GEMM artifact manifest is missing: " + artifact_path_message(artifact.manifest));
        }
        std::ifstream manifest(artifact.manifest);
        std::string marker;
        if (!manifest || !std::getline(manifest, marker) || marker != kManifestMarker) {
            throw runtime::RuntimeError(
                runtime::ErrorCode::ArtifactInvalid,
                "artifact manifest does not identify the Pearl P2 GEMM artifact");
        }

        bool context_setup_started = false;
        try {
            device = selector == "0" || selector == "accel0"
                ? xrt::device(0U)
                : xrt::device(selector);
            xclbin = xrt::xclbin(artifact.xclbin.string());
            const auto kernels = xclbin.get_kernels();
            const auto found = std::find_if(
                kernels.begin(), kernels.end(), [](const xrt::xclbin::kernel& candidate) {
                    return candidate.get_name().rfind("MLIR_AIE", 0U) == 0U;
                });
            if (found == kernels.end()) {
                throw runtime::RuntimeError(
                    runtime::ErrorCode::ArtifactInvalid,
                    "P2 xclbin has no MLIR_AIE kernel");
            }
            // opcode, instruction BO, instruction count, A BO, B BO, C BO.
            if (found->get_num_args() < 6U) {
                throw runtime::RuntimeError(
                    runtime::ErrorCode::ArtifactInvalid,
                    "P2 MLIR_AIE kernel has fewer than six arguments");
            }
            kernel_name = found->get_name();
            artifact_uuid = xclbin.get_uuid().to_string();
            instructions = load_instructions(artifact.instructions);
            context_setup_started = true;
            device.register_xclbin(xclbin);
            hardware_context = xrt::hw_context(device, xclbin.get_uuid());
            kernel = xrt::kernel(hardware_context, kernel_name);
            instruction_bo = xrt::bo(
                device,
                instructions.size() * sizeof(std::uint32_t),
                XCL_BO_FLAGS_CACHEABLE,
                kernel.group_id(1));
            left_bo = xrt::bo(device, kP2LeftBytes, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(3));
            right_bo = xrt::bo(device, kP2RightBytes, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(4));
            output_bo = xrt::bo(device, kP2OutputBytes, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(5));
            if (instruction_bo.size() < instructions.size() * sizeof(std::uint32_t)
                || left_bo.size() < kP2LeftBytes || right_bo.size() < kP2RightBytes
                || output_bo.size() < kP2OutputBytes) {
                throw runtime::RuntimeError(
                    runtime::ErrorCode::ArtifactInvalid,
                    "XRT allocated a buffer smaller than the P2 GEMM contract");
            }
            auto* instruction_map = instruction_bo.map<std::uint32_t*>();
            std::memcpy(
                instruction_map,
                instructions.data(),
                instructions.size() * sizeof(std::uint32_t));
            instruction_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        } catch (const runtime::RuntimeError&) {
            throw;
        } catch (const std::exception& error) {
            throw runtime::RuntimeError(
                context_setup_started
                    ? runtime::ErrorCode::RuntimeContextCreationFailed
                    : runtime::ErrorCode::ArtifactInvalid,
                context_setup_started
                    ? "P2 XRT hardware context or buffer setup failed: " + exception_message(error)
                    : "P2 XRT artifact setup failed: " + exception_message(error));
        }
    }

    XdnaMatmulArtifact artifact;
    runtime::CapabilityReport capability;
    xrt::device device;
    xrt::xclbin xclbin;
    xrt::hw_context hardware_context;
    xrt::kernel kernel;
    std::vector<std::uint32_t> instructions;
    xrt::bo instruction_bo;
    xrt::bo left_bo;
    xrt::bo right_bo;
    xrt::bo output_bo;
    std::string kernel_name;
    std::string artifact_uuid;
    XdnaMatmulCounters counters;
};

XdnaMatmulExecutor::XdnaMatmulExecutor(const XdnaMatmulArtifact& artifact,
                                       const std::string& selector)
    : impl_(new Impl(artifact, selector))
{
}

XdnaMatmulExecutor::~XdnaMatmulExecutor()
{
    delete impl_;
}

Int32Matrix XdnaMatmulExecutor::dispatch(const Int8Matrix& left, const Int8Matrix& right)
{
    require_fixed_matrix_shapes(left, right);
    auto* left_map = impl_->left_bo.map<std::int8_t*>();
    auto* right_map = impl_->right_bo.map<std::int8_t*>();
    auto* output_map = impl_->output_bo.map<std::int32_t*>();
    std::memcpy(left_map, left.values().data(), kP2LeftBytes);
    std::memcpy(right_map, right.values().data(), kP2RightBytes);
    // Poison is written and synchronized before every dispatch. The CPU
    // oracle result is never uploaded to the device output buffer.
    std::fill(output_map,
              output_map + kP2OutputElements,
              static_cast<std::int32_t>(0x5A5A5A5A));

    try {
        impl_->left_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE, kP2LeftBytes, 0U);
        impl_->right_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE, kP2RightBytes, 0U);
        impl_->output_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE, kP2OutputBytes, 0U);
        impl_->counters.h2d_syncs += 3U;
        impl_->counters.h2d_bytes += kP2LeftBytes + kP2RightBytes + kP2OutputBytes;
        ++impl_->counters.dispatches;

        ert_cmd_state status = ERT_CMD_STATE_ERROR;
        try {
            const auto started = std::chrono::steady_clock::now();
            xrt::run run = impl_->kernel(
                3U,
                impl_->instruction_bo,
                impl_->instructions.size(),
                impl_->left_bo,
                impl_->right_bo,
                impl_->output_bo);
            status = run.wait();
            impl_->counters.dispatch_wait_ns += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - started)
                    .count());
        } catch (const std::exception& error) {
            throw runtime::RuntimeError(
                runtime::ErrorCode::DeviceExecutionFailed,
                "P2 XRT GEMM dispatch failed: " + exception_message(error));
        }
        if (status != ERT_CMD_STATE_COMPLETED) {
            throw runtime::RuntimeError(
                runtime::ErrorCode::DeviceExecutionFailed,
                "P2 XRT GEMM run did not complete; status="
                    + std::to_string(static_cast<unsigned int>(status)));
        }
        ++impl_->counters.completed_dispatches;
        impl_->output_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE, kP2OutputBytes, 0U);
        ++impl_->counters.d2h_syncs;
        impl_->counters.d2h_bytes += kP2OutputBytes;
        return Int32Matrix(
            kP2Rows,
            kP2Columns,
            std::vector<std::int32_t>(output_map, output_map + kP2OutputElements));
    } catch (const runtime::RuntimeError&) {
        throw;
    } catch (const std::exception& error) {
        throw runtime::RuntimeError(
            runtime::ErrorCode::SynchronizationFailed,
            "P2 XRT GEMM synchronization failed: " + exception_message(error));
    }
}

const runtime::CapabilityReport& XdnaMatmulExecutor::capability() const noexcept
{
    return impl_->capability;
}

const XdnaMatmulCounters& XdnaMatmulExecutor::counters() const noexcept
{
    return impl_->counters;
}

const std::string& XdnaMatmulExecutor::kernel_name() const noexcept
{
    return impl_->kernel_name;
}

const std::string& XdnaMatmulExecutor::artifact_uuid() const noexcept
{
    return impl_->artifact_uuid;
}

} // namespace xdna::pearl
