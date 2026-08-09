#pragma once

#include "xdna/device.hpp"
#include "xdna/k1.hpp"
#include "xdna/m4.hpp"
#include "xdna/m5.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_hw_context.h"
#include "xrt/xrt_kernel.h"
#include "xrt/experimental/xrt_xclbin.h"

namespace xdna::runtime {

struct SmokeArtifact {
    std::filesystem::path xclbin;
    std::filesystem::path instructions;
    std::filesystem::path manifest;
};

using K1Artifact = SmokeArtifact;
using M4Artifact = SmokeArtifact;
using M5Artifact = SmokeArtifact;

enum class WorkloadKind {
    Smoke,
    K1,
    M4,
    M5,
};

struct RuntimeCounters {
    std::uint64_t dispatches = 0U;
    std::uint64_t completed_dispatches = 0U;
    std::uint64_t h2d_syncs = 0U;
    std::uint64_t d2h_syncs = 0U;
    std::uint64_t h2d_bytes = 0U;
    std::uint64_t d2h_bytes = 0U;
    std::uint64_t dispatch_wait_ns = 0U;
};

class XdnaRuntime final {
public:
    XdnaRuntime(const SmokeArtifact& artifact,
                const std::string& selector = "0",
                WorkloadKind workload = WorkloadKind::Smoke,
                std::size_t m5_batch_size = kM5DefaultBatchSize,
                std::size_t m5_columns = 1U);

    XdnaRuntime(const XdnaRuntime&) = delete;
    XdnaRuntime& operator=(const XdnaRuntime&) = delete;
    XdnaRuntime(XdnaRuntime&&) = delete;
    XdnaRuntime& operator=(XdnaRuntime&&) = delete;

    void dispatch(std::span<const std::int32_t> input,
                  std::span<std::int32_t> output);

    // Dispatches one already validated/packed K1 input. The method performs
    // only host validation, H2D, physical XRT execution, D2H, and output
    // validation; it never computes the CPU expected result.
    void dispatch_k1(K1PackedBuffers& buffers, const K1DeviceLayout& layout = {});

    // Dispatches one M4 operation. Scoring and CPU comparison remain above
    // this runtime boundary; this method performs only physical XRT work and
    // packed-buffer validation.
    void dispatch_m4(M4PackedBuffers& buffers, const M4DeviceLayout& layout = {});

    // Dispatches one fixed-size M5 batch. The runtime owns and reuses the
    // XRT BOs, but the complete input arena and output sentinel are rewritten
    // before every physical dispatch. It never computes CPU expected values.
    void dispatch_m5(M5PackedBatch& batch,
                     const M5DeviceLayout& layout = m5_default_layout());

    [[nodiscard]] const CapabilityReport& capability() const noexcept
    {
        return capability_;
    }

    [[nodiscard]] const RuntimeCounters& counters() const noexcept
    {
        return counters_;
    }

    [[nodiscard]] const std::string& kernel_name() const noexcept
    {
        return kernel_name_;
    }

    [[nodiscard]] const std::string& artifact_uuid() const noexcept
    {
        return artifact_uuid_;
    }

    [[nodiscard]] const SmokeArtifact& artifact() const noexcept
    {
        return artifact_;
    }

private:
    static xrt::device open_device(const std::string& selector);
    static std::vector<std::uint32_t> load_instructions(const std::filesystem::path& path);

    SmokeArtifact artifact_;
    WorkloadKind workload_;
    CapabilityReport capability_;
    xrt::device device_;
    xrt::xclbin xclbin_;
    xrt::hw_context hardware_context_;
    xrt::kernel kernel_;
    std::vector<std::uint32_t> instructions_;
    xrt::bo instruction_bo_;
    xrt::bo input_bo_;
    xrt::bo output_bo_;
    xrt::bo k1_input_bo_;
    xrt::bo k1_next_state_bo_;
    std::vector<bpp9000::Byte> k1_device_input_;
    xrt::bo m4_input_bo_;
    xrt::bo m4_output_bo_;
    std::vector<bpp9000::Byte> m4_device_input_;
    xrt::bo m5_input_bo_;
    xrt::bo m5_output_bo_;
    std::vector<bpp9000::Byte> m5_device_input_;
    std::size_t m5_batch_size_;
    std::size_t m5_columns_;
    std::string kernel_name_;
    std::string artifact_uuid_;
    RuntimeCounters counters_;
};

} // namespace xdna::runtime
