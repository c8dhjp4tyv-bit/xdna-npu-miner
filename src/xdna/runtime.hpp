#pragma once

#include "xdna/device.hpp"

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
};

struct RuntimeCounters {
    std::uint64_t dispatches = 0U;
    std::uint64_t completed_dispatches = 0U;
    std::uint64_t h2d_syncs = 0U;
    std::uint64_t d2h_syncs = 0U;
};

class XdnaRuntime final {
public:
    XdnaRuntime(const SmokeArtifact& artifact, const std::string& selector = "0");

    XdnaRuntime(const XdnaRuntime&) = delete;
    XdnaRuntime& operator=(const XdnaRuntime&) = delete;
    XdnaRuntime(XdnaRuntime&&) = delete;
    XdnaRuntime& operator=(XdnaRuntime&&) = delete;

    void dispatch(std::span<const std::int32_t> input,
                  std::span<std::int32_t> output);

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
    CapabilityReport capability_;
    xrt::device device_;
    xrt::xclbin xclbin_;
    xrt::hw_context hardware_context_;
    xrt::kernel kernel_;
    std::vector<std::uint32_t> instructions_;
    xrt::bo instruction_bo_;
    xrt::bo input_bo_;
    xrt::bo output_bo_;
    std::string kernel_name_;
    std::string artifact_uuid_;
    RuntimeCounters counters_;
};

} // namespace xdna::runtime
