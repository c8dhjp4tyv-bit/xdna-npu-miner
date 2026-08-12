#pragma once

#include "pearl/compute_pipeline.hpp"
#include "pearl/gateway.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace xdna::pearl {

// Immutable candidate-to-job binding.  It is held in memory through compute,
// verification, and submit; the official PlainProof wire intentionally carries
// raw roots rather than the V3-bound roots or a duplicate certificate version.
struct CandidateBinding {
    MiningJobIdentity job;
    std::uint32_t m = 0U;
    std::uint32_t n = 0U;
};

[[nodiscard]] CandidateBinding make_candidate_binding(const MiningJob& job,
                                                       const IncompleteBlockHeader& header,
                                                       std::size_t m,
                                                       std::size_t n);

// Constructs the repository-owned PlainProof envelope only after the physical
// pipeline has produced an exact selected product/transcript.  The caller
// supplies full committed matrices because openings are over the original
// matrices, not over the selected working tiles.
PlainProof build_plain_proof(const CandidateBinding& binding,
                             const IncompleteBlockHeader& header,
                             const MiningConfiguration& config,
                             const Int8Matrix& a,
                             const Int8Matrix& b,
                             std::uint32_t t_rows,
                             std::uint32_t t_cols,
                             const ComputePipelineResult& compute_result);

void verify_plain_proof_candidate(const PlainProof& proof,
                                  const CandidateBinding& binding);

// Serialize the dense PlainProof payload expected by the pinned official
// py-pearl-mining gateway.  This is deliberately separate from the
// repository-owned P1 envelope used for local evidence.
[[nodiscard]] std::vector<std::uint8_t> serialize_official_plain_proof(
    const PlainProof& proof,
    const CandidateBinding& binding);

} // namespace xdna::pearl
