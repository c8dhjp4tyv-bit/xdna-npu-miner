#include "pearl/candidate.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace xdna::pearl {
namespace {

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

PlainProof build_plain_proof(const IncompleteBlockHeader& header,
                             const MiningConfiguration& config,
                             const Int8Matrix& a,
                             const Int8Matrix& b,
                             std::uint32_t t_rows,
                             std::uint32_t t_cols,
                             const ComputePipelineResult& compute_result,
                             const Digest& target)
{
    if (a.cols() != b.rows()) {
        throw Error(ErrorCode::InvalidShape, "candidate matrices have different common dimensions");
    }
    if (a.cols() != config.common_dim) {
        throw Error(ErrorCode::InvalidShape, "candidate dimensions do not match configuration");
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
    const CommitmentSeeds seeds = commitment_seeds(key, hash_a, hash_b);
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
    proof.target = target;
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
    // serialize() is the canonical validation boundary for every field that
    // the fixed-width P1 envelope owns.
    (void)proof.serialize();
    return proof;
}

void verify_plain_proof_candidate(const PlainProof& proof)
{
    const std::vector<std::uint8_t> bytes = proof.serialize();
    const PlainProof decoded = PlainProof::deserialize(bytes);
    if (decoded.serialize() != bytes) {
        throw Error(ErrorCode::NonCanonical, "PlainProof candidate is not canonical");
    }
}

std::vector<std::uint8_t> serialize_official_plain_proof(const PlainProof& proof)
{
    // Validate every repository-owned field before projecting to the smaller
    // official wire object.  The official bincode object contains only these
    // dimensions and the two MatrixMerkleProof values plus Option::None.
    (void)proof.serialize();

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
