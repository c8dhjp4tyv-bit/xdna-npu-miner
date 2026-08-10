#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace xdna::pearl {

constexpr std::size_t kDigestBytes = 32U;
constexpr std::size_t kHeaderBytes = 76U;
constexpr std::size_t kMiningConfigBytes = 52U;
constexpr std::size_t kMerkleChunkBytes = 1024U;
constexpr std::size_t kTranscriptWords = 16U;
constexpr std::size_t kSelectedRows = 2U;
constexpr std::size_t kSelectedColumns = 64U;
constexpr std::size_t kSelectedValues = kSelectedRows * kSelectedColumns;
constexpr std::uint32_t kMaxMatrixDimension = 1U << 24U;
constexpr std::uint32_t kMaxCommonDimension = 1U << 16U;
constexpr std::uint32_t kNoiseRange = 128U;
constexpr std::uint32_t kNoiseRank = 128U;
constexpr std::uint32_t kNoiseIndicesPerColumn = 2U;
constexpr std::uint32_t kTranscriptRotation = 13U;

using Digest = std::array<std::uint8_t, kDigestBytes>;

enum class ErrorCode : std::uint8_t {
    InvalidLength,
    InvalidValue,
    InvalidShape,
    NonCanonical,
    OutOfBounds,
    ArithmeticOverflow,
};

class Error final : public std::runtime_error {
public:
    Error(ErrorCode code, const std::string& message);

    [[nodiscard]] ErrorCode code() const noexcept
    {
        return code_;
    }

private:
    ErrorCode code_;
};

struct IncompleteBlockHeader {
    std::uint32_t version = 0U;
    Digest prev_block{};
    Digest merkle_root{};
    std::uint32_t timestamp = 0U;
    std::uint32_t nbits = 0U;
};

[[nodiscard]] std::array<std::uint8_t, kHeaderBytes> serialize_header(const IncompleteBlockHeader& header);
IncompleteBlockHeader deserialize_header(std::span<const std::uint8_t> bytes);

class PeriodicPattern {
public:
    struct Shape {
        std::uint32_t stride = 1U;
        std::uint32_t length = 1U;

        friend constexpr bool operator==(const Shape&, const Shape&) = default;
    };

    PeriodicPattern() = default;
    explicit PeriodicPattern(std::array<Shape, 3U> shape)
        : shape_(shape)
    {
    }

    [[nodiscard]] const std::array<Shape, 3U>& shape() const noexcept
    {
        return shape_;
    }

    [[nodiscard]] std::vector<std::uint32_t> indices() const;
    [[nodiscard]] std::vector<std::uint32_t> indices_with_offset(std::uint32_t offset) const;
    [[nodiscard]] std::array<std::uint8_t, 6U> to_bytes() const;
    [[nodiscard]] bool offset_is_valid(std::uint32_t offset) const;
    [[nodiscard]] std::uint32_t max_index() const;

    static PeriodicPattern from_bytes(std::span<const std::uint8_t> bytes);
    static PeriodicPattern from_indices(std::span<const std::uint32_t> indices);

private:
    std::array<Shape, 3U> shape_{};
};

enum class ValidationProfile : std::uint8_t {
    Structural,
    PinnedCurrent,
};

struct MiningConfiguration {
    std::uint32_t common_dim = 1024U;
    std::uint16_t rank = static_cast<std::uint16_t>(kNoiseRank);
    std::uint16_t mma_type = 0U; // Int7xInt7ToInt32
    PeriodicPattern rows_pattern{};
    PeriodicPattern cols_pattern{};
    std::array<std::uint8_t, 32U> dense_trailer{};

    [[nodiscard]] std::array<std::uint8_t, kMiningConfigBytes> to_bytes() const;
    static MiningConfiguration from_bytes(std::span<const std::uint8_t> bytes);
    [[nodiscard]] std::size_t dot_product_length() const;
};

void validate_configuration(const MiningConfiguration& config,
                            std::uint32_t m,
                            std::uint32_t n,
                            std::uint32_t t_rows,
                            std::uint32_t t_cols,
                            ValidationProfile profile = ValidationProfile::PinnedCurrent);

class Int8Matrix {
public:
    Int8Matrix() = default;
    Int8Matrix(std::size_t rows, std::size_t cols, std::vector<std::int8_t> values);

    [[nodiscard]] std::size_t rows() const noexcept
    {
        return rows_;
    }

    [[nodiscard]] std::size_t cols() const noexcept
    {
        return cols_;
    }

    [[nodiscard]] std::int8_t at(std::size_t row, std::size_t col) const;
    [[nodiscard]] std::int8_t& at(std::size_t row, std::size_t col);
    [[nodiscard]] const std::vector<std::int8_t>& values() const noexcept
    {
        return values_;
    }

    [[nodiscard]] std::vector<std::uint8_t> raw_bytes() const;
    void require_signal_range() const;

private:
    std::size_t rows_ = 0U;
    std::size_t cols_ = 0U;
    std::vector<std::int8_t> values_;
};

struct QuantizedMatrix {
    Int8Matrix values;
    std::vector<float> row_scales;
};

QuantizedMatrix quantize_fp32(std::span<const float> values,
                              std::size_t rows,
                              std::size_t cols);

struct NoiseMatrices {
    Int8Matrix e_al; // selected A rows × r
    Int8Matrix e_ar; // r × k
    Int8Matrix e_bl; // k × r
    Int8Matrix e_br; // r × selected B columns
    Int8Matrix noise_a;
    Int8Matrix noise_b_transposed;
};

struct NoisedOperands {
    Int8Matrix a;
    Int8Matrix b;
};

struct CommitmentSeeds {
    Digest b_noise_seed{};
    Digest a_noise_seed{};
};

Digest blake3_keyed(const Digest& key, std::span<const std::uint8_t> data);
Digest blake3_chunk_cv(const Digest& key,
                       std::span<const std::uint8_t> data,
                       std::uint64_t chunk_index);
Digest blake3_parent_cv(const Digest& key,
                        const Digest& left,
                        const Digest& right,
                        bool root);

Digest job_key(const IncompleteBlockHeader& header, const MiningConfiguration& config);
CommitmentSeeds commitment_seeds(const Digest& job_key,
                                 const Digest& hash_a,
                                 const Digest& hash_b);
NoiseMatrices generate_noise(std::size_t k,
                             std::size_t rank,
                             const CommitmentSeeds& seeds,
                             std::span<const std::size_t> a_rows,
                             std::span<const std::size_t> b_columns);
NoisedOperands make_noised_operands(const Int8Matrix& a,
                                    const Int8Matrix& b,
                                    const NoiseMatrices& noise);

class Int32Matrix {
public:
    Int32Matrix() = default;
    Int32Matrix(std::size_t rows, std::size_t cols, std::vector<std::int32_t> values);

    [[nodiscard]] std::size_t rows() const noexcept
    {
        return rows_;
    }

    [[nodiscard]] std::size_t cols() const noexcept
    {
        return cols_;
    }

    [[nodiscard]] std::int32_t at(std::size_t row, std::size_t col) const;
    [[nodiscard]] std::int32_t& at(std::size_t row, std::size_t col);
    [[nodiscard]] const std::vector<std::int32_t>& values() const noexcept
    {
        return values_;
    }

private:
    std::size_t rows_ = 0U;
    std::size_t cols_ = 0U;
    std::vector<std::int32_t> values_;
};

Int32Matrix gemm_checked(const Int8Matrix& left, const Int8Matrix& right);
Int32Matrix noised_gemm(const Int8Matrix& a,
                        const Int8Matrix& b,
                        const NoiseMatrices& noise);
Int32Matrix denoise_product_checked(const Int8Matrix& a,
                                    const Int8Matrix& b,
                                    const NoiseMatrices& noise,
                                    const Int32Matrix& noised_product);

struct TranscriptStep {
    std::uint32_t reduction_index = 0U;
    std::uint32_t combined_xor = 0U;
    std::array<std::uint32_t, kTranscriptWords> state{};
};

struct TranscriptResult {
    std::array<std::uint32_t, kTranscriptWords> words{};
    std::vector<TranscriptStep> trace;
};

TranscriptResult selected_transcript(const Int8Matrix& noised_a,
                                     const Int8Matrix& noised_b,
                                     const Int32Matrix& noised_product,
                                     std::size_t rank);

Digest jackpot_hash(const std::array<std::uint32_t, kTranscriptWords>& transcript,
                    const Digest& commitment_hash);
Digest target_from_bytes(std::span<const std::uint8_t> bytes);
bool jackpot_meets_target(const Digest& jackpot, const Digest& target);

struct MerkleProof {
    std::vector<std::array<std::uint8_t, kMerkleChunkBytes>> leaf_data;
    std::vector<std::size_t> leaf_indices;
    std::size_t total_leaves = 0U;
    Digest root{};
    std::vector<Digest> siblings;
};

struct MatrixOpening {
    MerkleProof proof;
    std::vector<std::size_t> row_indices;
};

Digest merkle_root(std::span<const std::uint8_t> data, const Digest& key);
MatrixOpening open_matrix_rows(const Int8Matrix& matrix,
                               const Digest& key,
                               std::span<const std::size_t> rows);
bool verify_merkle_proof(const MerkleProof& proof, const Digest& key);
std::vector<std::uint8_t> extract_opening_bytes(const MerkleProof& proof,
                                                std::size_t offset,
                                                std::size_t length);

struct PlainProof {
    // P1's explicit fixed-width envelope. It is the candidate immediately
    // before the CPU/Rust proving boundary; it is not a ZK certificate.
    std::uint32_t version = 1U;
    IncompleteBlockHeader header{};
    MiningConfiguration config{};
    Digest header_config_key{};
    Digest hash_a{};
    Digest hash_b{};
    Digest commitment_b{};
    Digest commitment_a{};
    Digest jackpot{};
    Digest target{};
    std::uint32_t m = 0U;
    std::uint32_t n = 0U;
    std::uint32_t k = 0U;
    std::uint32_t rank = 0U;
    std::uint32_t t_rows = 0U;
    std::uint32_t t_cols = 0U;
    MatrixOpening a_opening{};
    MatrixOpening bt_opening{};
    TranscriptResult transcript{};

    [[nodiscard]] std::vector<std::uint8_t> serialize() const;
    static PlainProof deserialize(std::span<const std::uint8_t> bytes);
};

struct PublicData {
    MiningConfiguration config{};
    Digest hash_a{};
    Digest hash_b{};
    Digest jackpot{};
    std::uint32_t m = 0U;
    std::uint32_t n = 0U;
    std::uint32_t t_rows = 0U;
    std::uint32_t t_cols = 0U;
};

[[nodiscard]] std::vector<std::uint8_t> serialize_public_data(const MiningConfiguration& config,
                                                               const Digest& hash_a,
                                                               const Digest& hash_b,
                                                               const Digest& jackpot,
                                                               std::uint32_t m,
                                                               std::uint32_t n,
                                                               std::uint32_t t_rows,
                                                               std::uint32_t t_cols);
PublicData deserialize_public_data(std::span<const std::uint8_t> bytes);

} // namespace xdna::pearl
