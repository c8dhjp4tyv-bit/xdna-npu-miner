#pragma once

#include "xdna/runtime.hpp"

#include <cstdint>
#include <filesystem>

namespace xdna::runtime {

constexpr std::int32_t smoke_cpu_transform(std::int32_t value) noexcept
{
    return value * 3 + 7;
}

struct SmokeSummary {
    std::uint64_t requested_dispatches = 0U;
    std::uint64_t exact_matches = 0U;
    std::uint64_t output_mismatches = 0U;
    std::uint64_t runtime_failures = 0U;
};

[[nodiscard]] SmokeSummary run_smoke(XdnaRuntime& runtime, std::uint64_t iterations);

void write_smoke_evidence(const std::filesystem::path& path,
                          const XdnaRuntime& runtime,
                          const SmokeSummary& summary,
                          std::uint64_t iterations);

} // namespace xdna::runtime
