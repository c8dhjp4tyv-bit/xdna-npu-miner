#pragma once

#include "pearl/compute_pipeline.hpp"

#include <span>

namespace xdna::pearl {

// Constructs the repository-owned PlainProof envelope only after the physical
// pipeline has produced an exact selected product/transcript.  The caller
// supplies full committed matrices because openings are over the original
// matrices, not over the selected working tiles.
PlainProof build_plain_proof(const IncompleteBlockHeader& header,
                             const MiningConfiguration& config,
                             const Int8Matrix& a,
                             const Int8Matrix& b,
                             std::uint32_t t_rows,
                             std::uint32_t t_cols,
                             const ComputePipelineResult& compute_result,
                             const Digest& target);

void verify_plain_proof_candidate(const PlainProof& proof);

} // namespace xdna::pearl
