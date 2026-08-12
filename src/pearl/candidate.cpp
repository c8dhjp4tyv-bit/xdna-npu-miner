#include "pearl/candidate.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace xdna::pearl {
namespace {

[[nodiscard]] std::uint32_t canonical_dimension(std::size_t dimension, const char* label)
{
    if (dimension > std::numeric_limits<std::uint32_t>::max()) {
        throw Error(ErrorCode::InvalidValue,
                    std::string(label) + " does not fit canonical u32");
    }
    return static_cast<std::uint32_t>(dimension);
}

void validate_binding(const CandidateBinding& binding, const PlainProof& proof)
{
    if (!is_supported_certificate_version(
            certificate_version_number(binding.job.certificate_version))) {
        throw Error(ErrorCode::InvalidValue, "candidate has unsupported certificate version");
    }
    if (binding.job.incomplete_header_bytes.size() != kHeaderBytes) {
        throw Error(ErrorCode::InvalidLength, "candidate job has noncanonical incomplete header size");
    }
    const std::array<std::uint8_t, kHeaderBytes> header_bytes = serialize_header(proof.header);
    if (!std::equal(header_bytes.begin(), header_bytes.end(),
                    binding.job.incomplete_header_bytes.begin(),
                    binding.job.incomplete_header_bytes.end())) {
        throw Error(ErrorCode::InvalidValue, "candidate header does not match immutable job identity");
    }
    if (proof.target != binding.job.target || proof.m != binding.m || proof.n != binding.n) {
        throw Error(ErrorCode::InvalidValue,
                    "candidate target or matrix dimensions do not match immutable job identity");
    }
    validate_plain_proof_for_certificate(proof, binding.job.certificate_version);
}

void append_u64(std::vector<std::uint8_t>& output, std::uint64_t value)
{
    for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
        output.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
    }
}

void append_official_merkle_proof(std::vector<std::uint8_t>& output,
                                  const MerkleProof& proof)
{
    append_u64(output, static_cast<std::uint64_t>(proof.leaf_data.size()));
    for (const auto& leaf : proof.leaf_data) {
        append_u64(output, static_cast<std::uint64_t>(leaf.size()));
        output.insert(output.end(), leaf.begin(), leaf.end());
    }
    append_u64(output, static_cast<std::uint64_t>(proof.leaf_indices.size()));
    for (const std::size_t index : proof.leaf_indices) {
        append_u64(output, static_cast<std::uint64_t>(index));
    }
    append_u64(output, static_cast<std::uint64_t>(proof.total_leaves));
    output.insert(output.end(), proof.root.begin(), proof.root.end());
    append_u64(output, static_cast<std::uint64_t>(proof.siblings.size()));
    for (const Digest& sibling : proof.siblings) {
        output.insert(output.end(), sibling.begin(), sibling.end());
    }
}

void append_official_matrix_merkle_proof(std::vector<std::uint8_t>& output,
                                         const MatrixOpening& opening)
{
    // Rust's MatrixMerkleProof fields are ordered as proof, then row_indices.
    append_official_merkle_proof(output, opening.proof);
    append_u64(output, static_cast<std::uint64_t>(opening.row_indices.size()));
    for (const std::size_t index : opening.row_indices) {
        append_u64(output, static_cast<std::uint64_t>(index));
    }
}

[[nodiscard]] Int8Matrix transpose(const Int8Matrix& matrix)
{
    std::vector<std::int8_t> values(matrix.rows() * matrix.cols(), 0);
    for (std::size_t row = 0U; row < matrix.rows(); ++row) {
        for (std::size_t column = 0U; column < matrix.cols(); ++column) {
            values[column * matrix.rows() + row] = matrix.at(row, column);
        }
    }
    return Int8Matrix(matrix.cols(), matrix.rows(), std::move(values));
}

[[nodiscard]] Int8Matrix select_rows(const Int8Matrix& matrix,
                                      std::span<const std::uint32_t> rows)
{
    std::vector<std::int8_t> values;
    values.reserve(rows.size() * matrix.cols());
    for (const std::uint32_t row : rows) {
        if (row >= matrix.rows()) {
            throw Error(ErrorCode::OutOfBounds, "selected A row is outside the committed matrix");
        }
        for (std::size_t column = 0U; column < matrix.cols(); ++column) {
            values.push_back(matrix.at(row, column));
        }
    }
    return Int8Matrix(rows.size(), matrix.cols(), std::move(values));
}

[[nodiscard]] Int8Matrix select_columns(const Int8Matrix& matrix,
                                         std::span<const std::uint32_t> columns)
{
    std::vector<std::int8_t> values;
    values.reserve(matrix.rows() * columns.size());
    for (std::size_t row = 0U; row < matrix.rows(); ++row) {
        for (const std::uint32_t column : columns) {
            if (column >= matrix.cols()) {
                throw Error(ErrorCode::OutOfBounds, "selected B column is outside the committed matrix");
            }
            values.push_back(matrix.at(row, column));
        }
    }
    return Int8Matrix(matrix.rows(), columns.size(), std::move(values));
}

} // namespace

CandidateBinding make_candidate_binding(const MiningJob& job,
                                        const IncompleteBlockHeader& header,
                                        std::size_t m,
                                        std::size_t n)
{
    if (!is_supported_certificate_version(certificate_version_number(job.certificate_version))) {
        throw Error(ErrorCode::InvalidValue, "candidate job uses an unsupported certificate version");
    }
    const std::array<std::uint8_t, kHeaderBytes> encoded_header = serialize_header(header);
    if (job.incomplete_header_bytes.size() != encoded_header.size()
        || !std::equal(encoded_header.begin(), encoded_header.end(),
                       job.incomplete_header_bytes.begin(), job.incomplete_header_bytes.end())) {
        throw Error(ErrorCode::InvalidValue,
                    "candidate header does not match the gateway job header");
    }
    return CandidateBinding{MiningJobIdentity{job.job_id,
                                               job.incomplete_header_bytes,
                                               job.target,
                                               job.certificate_version},
                            canonical_dimension(m, "A matrix dimension"),
                            canonical_dimension(n, "B matrix dimension")};
}

PlainProof build_plain_proof(const CandidateBinding& binding,
                             const IncompleteBlockHeader& header,
                             const MiningConfiguration& config,
                             const Int8Matrix& a,
                             const Int8Matrix& b,
                             std::uint32_t t_rows,
                             std::uint32_t t_cols,
                             const ComputePipelineResult& compute_result)
{
    if (a.cols() != b.rows()) {
        throw Error(ErrorCode::InvalidShape, "candidate matrices have different common dimensions");
    }
    if (a.cols() != config.common_dim) {
        throw Error(ErrorCode::InvalidShape, "candidate dimensions do not match configuration");
    }
    if (binding.m != canonical_dimension(a.rows(), "A matrix dimension")
        || binding.n != canonical_dimension(b.cols(), "B matrix dimension")) {
        throw Error(ErrorCode::InvalidValue,
                    "candidate matrices do not match immutable job dimensions");
    }
    validate_configuration(config,
                           static_cast<std::uint32_t>(a.rows()),
                           static_cast<std::uint32_t>(b.cols()),
                           t_rows,
                           t_cols);

    const Digest key = job_key(header, config);
    const Int8Matrix bt = transpose(b);
    const Digest hash_a = merkle_root(a.raw_bytes(), key);
    const Digest hash_b = merkle_root(bt.raw_bytes(), key);
    const CommitmentSeeds seeds = commitment_seeds(binding.job.certificate_version,
                                                   key,
                                                   hash_a,
                                                   hash_b,
                                                   a.rows(),
                                                   b.cols());
    const std::vector<std::uint32_t> a_rows = config.rows_pattern.indices_with_offset(t_rows);
    const std::vector<std::uint32_t> b_columns = config.cols_pattern.indices_with_offset(t_cols);
    const Int8Matrix selected_a = select_rows(a, a_rows);
    const Int8Matrix selected_b = select_columns(b, b_columns);
    const Int32Matrix expected_original = gemm_checked(selected_a, selected_b);
    if (compute_result.denoised_product.values() != expected_original.values()) {
        throw Error(ErrorCode::InvalidValue, "candidate denoised product differs from CPU reference");
    }
    if (compute_result.jackpot != jackpot_hash(compute_result.transcript.words, seeds.a_noise_seed)) {
        throw Error(ErrorCode::InvalidValue, "candidate jackpot is not tied to the current commitment");
    }

    PlainProof proof;
    proof.header = header;
    proof.config = config;
    proof.header_config_key = key;
    proof.hash_a = hash_a;
    proof.hash_b = hash_b;
    proof.commitment_b = seeds.b_noise_seed;
    proof.commitment_a = seeds.a_noise_seed;
    proof.jackpot = compute_result.jackpot;
    proof.target = binding.job.target;
    proof.m = static_cast<std::uint32_t>(a.rows());
    proof.n = static_cast<std::uint32_t>(b.cols());
    proof.k = config.common_dim;
    proof.rank = config.rank;
    proof.t_rows = t_rows;
    proof.t_cols = t_cols;
    std::vector<std::size_t> a_opening_rows(a_rows.begin(), a_rows.end());
    std::vector<std::size_t> b_opening_rows(b_columns.begin(), b_columns.end());
    proof.a_opening = open_matrix_rows(a, key, a_opening_rows);
    proof.bt_opening = open_matrix_rows(bt, key, b_opening_rows);
    proof.transcript = compute_result.transcript;
    validate_binding(binding, proof);
    return proof;
}

void verify_plain_proof_candidate(const PlainProof& proof,
                                  const CandidateBinding& binding)
{
    validate_binding(binding, proof);
    if (binding.job.certificate_version != CertificateVersion::V3) {
        const std::vector<std::uint8_t> bytes = proof.serialize();
        const PlainProof decoded = PlainProof::deserialize(bytes);
        if (decoded.serialize() != bytes) {
            throw Error(ErrorCode::NonCanonical, "PlainProof candidate is not canonical");
        }
    }
}

std::vector<std::uint8_t> serialize_official_plain_proof(const PlainProof& proof,
                                                         const CandidateBinding& binding)
{
    // Validate every repository-owned field before projecting to the smaller
    // official wire object.  The official bincode object contains only these
    // dimensions and the two MatrixMerkleProof values plus Option::None.
    validate_binding(binding, proof);

    std::vector<std::uint8_t> output;
    output.reserve(16U * 1024U);
    append_u64(output, static_cast<std::uint64_t>(proof.m));
    append_u64(output, static_cast<std::uint64_t>(proof.n));
    append_u64(output, static_cast<std::uint64_t>(proof.k));
    append_u64(output, static_cast<std::uint64_t>(proof.rank));
    append_official_matrix_merkle_proof(output, proof.a_opening);
    append_official_matrix_merkle_proof(output, proof.bt_opening);
    // bincode encodes Option::None as a single zero tag.  The dense proof is
    // intentionally used here; MoE requires a separate routing wire object.
    output.push_back(0U);
    return output;
}

} // namespace xdna::pearl
