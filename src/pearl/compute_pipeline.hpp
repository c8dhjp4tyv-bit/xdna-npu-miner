#pragma once

#include "pearl/reference.hpp"
#include "pearl/xdna_matmul.hpp"

#include <cstddef>
#include <cstdint>

namespace xdna::pearl {

struct ComputePipelineTimings {
    std::uint64_t cpu_preprocessing_ns = 0U;
    std::uint64_t packing_ns = 0U;
    std::uint64_t npu_execution_ns = 0U;
    std::uint64_t cpu_verification_ns = 0U;
    std::uint64_t transcript_ns = 0U;
    std::uint64_t blake3_ns = 0U;
    std::uint64_t total_ns = 0U;
};

struct ComputePipelineResult {
    Int32Matrix noised_product;
    Int32Matrix denoised_product;
    TranscriptResult transcript;
    Digest jackpot{};
    Digest target{};
    bool meets_target = false;
    ComputePipelineTimings timings{};
    std::size_t physical_dispatches = 0U;
};

// Runs the Pearl-owned CPU preprocessing and verification around the physical
// P2 XDNA backend.  The backend computes one 4x64x8 tile per column group;
// this wrapper gathers the first two rows into the proof-shaped 2x64 tile
// consumed by the exact transcript oracle.
class ComputePipeline final {
public:
    explicit ComputePipeline(XdnaMatmulExecutor& executor) noexcept
        : executor_(executor)
    {
    }

    ComputePipelineResult run(const Int8Matrix& a,
                              const Int8Matrix& b,
                              const NoiseMatrices& noise,
                              std::size_t rank,
                              const Digest& commitment_hash,
                              const Digest& target) const;

private:
    XdnaMatmulExecutor& executor_;
};

} // namespace xdna::pearl
