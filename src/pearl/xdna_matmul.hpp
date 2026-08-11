#pragma once

#include "pearl/reference.hpp"

#include "xdna/device.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace xdna::pearl {

// P2 is deliberately a small fixed AIE2 tile. The inner dimension is a
// multiple of 64, matching the Pearl configuration contract, while the 4x8
// output is small enough to bring up and inspect independently.
constexpr std::size_t kP2Rows = 4U;
constexpr std::size_t kP2Common = 64U;
constexpr std::size_t kP2Columns = 8U;
constexpr std::size_t kP2LeftBytes = kP2Rows * kP2Common;
constexpr std::size_t kP2RightBytes = kP2Common * kP2Columns;
constexpr std::size_t kP2OutputElements = kP2Rows * kP2Columns;
constexpr std::size_t kP2OutputBytes = kP2OutputElements * sizeof(std::int32_t);

struct XdnaMatmulArtifact {
    std::filesystem::path xclbin;
    std::filesystem::path instructions;
    std::filesystem::path manifest;
};

struct XdnaMatmulCounters {
    std::uint64_t dispatches = 0U;
    std::uint64_t completed_dispatches = 0U;
    std::uint64_t h2d_syncs = 0U;
    std::uint64_t d2h_syncs = 0U;
    std::uint64_t h2d_bytes = 0U;
    std::uint64_t d2h_bytes = 0U;
    std::uint64_t dispatch_wait_ns = 0U;
};

class XdnaMatmulExecutor final {
public:
    XdnaMatmulExecutor(const XdnaMatmulArtifact& artifact,
                       const std::string& selector = "0");
    ~XdnaMatmulExecutor();

    XdnaMatmulExecutor(const XdnaMatmulExecutor&) = delete;
    XdnaMatmulExecutor& operator=(const XdnaMatmulExecutor&) = delete;
    XdnaMatmulExecutor(XdnaMatmulExecutor&&) = delete;
    XdnaMatmulExecutor& operator=(XdnaMatmulExecutor&&) = delete;

    // The host uploads only the two int8 operands. The output BO is poisoned
    // before every dispatch and is never seeded with the CPU answer.
    [[nodiscard]] Int32Matrix dispatch(const Int8Matrix& left,
                                       const Int8Matrix& right);

    [[nodiscard]] const runtime::CapabilityReport& capability() const noexcept;
    [[nodiscard]] const XdnaMatmulCounters& counters() const noexcept;
    [[nodiscard]] const std::string& kernel_name() const noexcept;
    [[nodiscard]] const std::string& artifact_uuid() const noexcept;

private:
    struct Impl;
    Impl* impl_;
};

} // namespace xdna::pearl
