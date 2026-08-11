#include "pearl/reference.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace xdna::pearl;

class Failure final : public std::runtime_error {
public:
    explicit Failure(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

std::size_t assertions = 0U;

void expect(bool condition, const std::string& message)
{
    ++assertions;
    if (!condition) {
        throw Failure(message);
    }
}

template <typename Function>
void expect_error(ErrorCode code, Function&& function, const std::string& message)
{
    ++assertions;
    try {
        function();
    } catch (const Error& error) {
        if (error.code() != code) {
            throw Failure(message + " (wrong error code)");
        }
        return;
    }
    throw Failure(message + " (no Pearl error)");
}

[[nodiscard]] std::string hex(std::span<const std::uint8_t> bytes)
{
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const std::uint8_t value : bytes) {
        output << std::setw(2) << static_cast<unsigned int>(value);
    }
    return output.str();
}

[[nodiscard]] std::string hex(const Digest& digest)
{
    return hex(std::span<const std::uint8_t>(digest));
}

[[nodiscard]] std::uint64_t next_word(std::uint64_t& state)
{
    state += 0x9E3779B97F4A7C15ULL;
    std::uint64_t value = state;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
}

[[nodiscard]] PeriodicPattern rows_pattern()
{
    const std::array<std::uint32_t, 2U> rows = {0U, 8U};
    return PeriodicPattern::from_indices(rows);
}

[[nodiscard]] PeriodicPattern cols_pattern()
{
    std::array<std::uint32_t, kSelectedColumns> columns{};
    for (std::size_t index = 0U; index < columns.size(); ++index) {
        columns[index] = static_cast<std::uint32_t>((index / 2U) * 8U + (index % 2U));
    }
    return PeriodicPattern::from_indices(columns);
}

[[nodiscard]] MiningConfiguration current_config()
{
    MiningConfiguration config;
    config.common_dim = 2048U;
    config.rank = 128U;
    config.mma_type = 0U;
    config.rows_pattern = rows_pattern();
    config.cols_pattern = cols_pattern();
    return config;
}

[[nodiscard]] Int8Matrix fixture_matrix(std::size_t rows,
                                        std::size_t cols,
                                        std::uint64_t seed)
{
    std::vector<std::int8_t> values;
    values.reserve(rows * cols);
    std::uint64_t state = seed;
    for (std::size_t index = 0U; index < rows * cols; ++index) {
        const std::uint8_t bucket = static_cast<std::uint8_t>(next_word(state) & 127U);
        values.push_back(static_cast<std::int8_t>(static_cast<std::int32_t>(bucket) - 64));
    }
    return Int8Matrix(rows, cols, std::move(values));
}

[[nodiscard]] Int8Matrix select_rows(const Int8Matrix& matrix,
                                      std::span<const std::size_t> rows)
{
    std::vector<std::int8_t> values;
    values.reserve(rows.size() * matrix.cols());
    for (const std::size_t row : rows) {
        for (std::size_t col = 0U; col < matrix.cols(); ++col) {
            values.push_back(matrix.at(row, col));
        }
    }
    return Int8Matrix(rows.size(), matrix.cols(), std::move(values));
}

[[nodiscard]] Int8Matrix select_columns(const Int8Matrix& matrix,
                                         std::span<const std::size_t> columns)
{
    std::vector<std::int8_t> values;
    values.reserve(matrix.rows() * columns.size());
    for (std::size_t row = 0U; row < matrix.rows(); ++row) {
        for (const std::size_t col : columns) {
            values.push_back(matrix.at(row, col));
        }
    }
    return Int8Matrix(matrix.rows(), columns.size(), std::move(values));
}

[[nodiscard]] Int8Matrix transpose(const Int8Matrix& matrix)
{
    std::vector<std::int8_t> values(matrix.rows() * matrix.cols(), 0);
    for (std::size_t row = 0U; row < matrix.rows(); ++row) {
        for (std::size_t col = 0U; col < matrix.cols(); ++col) {
            values[col * matrix.rows() + row] = matrix.at(row, col);
        }
    }
    return Int8Matrix(matrix.cols(), matrix.rows(), std::move(values));
}

[[nodiscard]] Digest repeated_digest(std::uint8_t value)
{
    Digest digest{};
    digest.fill(value);
    return digest;
}

void test_serialization_and_validation()
{
    IncompleteBlockHeader header;
    header.version = 0x20000001U;
    for (std::size_t index = 0U; index < header.prev_block.size(); ++index) {
        header.prev_block[index] = static_cast<std::uint8_t>(index);
        header.merkle_root[index] = static_cast<std::uint8_t>(0xA0U + index);
    }
    header.timestamp = 0x12345678U;
    header.nbits = 0x207FFFFFU;
    const auto header_bytes = serialize_header(header);
    expect(header_bytes.size() == kHeaderBytes, "header size");
    expect(header_bytes[0] == 1U && header_bytes[1] == 0U
               && header_bytes[2] == 0U && header_bytes[3] == 0x20U,
           "header version is little endian");
    expect(header_bytes[4] == 31U && header_bytes[35] == 0U,
           "previous hash wire order is reversed");
    expect(deserialize_header(header_bytes).prev_block == header.prev_block,
           "header round trip");
    std::vector<std::uint8_t> truncated(header_bytes.begin(), header_bytes.end() - 1);
    expect_error(ErrorCode::InvalidLength,
                 [&] { (void)deserialize_header(truncated); },
                 "truncated header rejected");

    const MiningConfiguration config = current_config();
    const auto config_bytes = config.to_bytes();
    expect(config_bytes.size() == kMiningConfigBytes, "config size");
    expect(hex(std::span<const std::uint8_t>(config_bytes))
               == "00080000800000000701000000000001031f00000000000000000000000000000000000000000000000000000000000000000000",
           "config canonical vector");
    expect(config_bytes[0] == 0U && config_bytes[1] == 8U
               && config_bytes[8] == 7U && config_bytes[9] == 1U,
           "config common dimension and rows pattern encoding");
    expect(config_bytes[14] == 0U && config_bytes[15] == 1U
               && config_bytes[16] == 3U && config_bytes[17] == 31U,
           "config columns pattern encoding");
    expect(MiningConfiguration::from_bytes(config_bytes).to_bytes() == config_bytes,
           "config round trip");
    const auto public_data = serialize_public_data(config,
                                                   repeated_digest(0x11U),
                                                   repeated_digest(0x22U),
                                                   repeated_digest(0x33U),
                                                   9U,
                                                   250U,
                                                   0U,
                                                   0U);
    expect(public_data.size() == 164U, "dense public data size");
    const PublicData public_round_trip = deserialize_public_data(public_data);
    expect(public_round_trip.m == 9U && public_round_trip.n == 250U
               && public_round_trip.hash_b == repeated_digest(0x22U),
           "dense public data round trip");
    auto public_truncated = public_data;
    public_truncated.pop_back();
    expect_error(ErrorCode::InvalidLength,
                 [&] { (void)deserialize_public_data(public_truncated); },
                 "truncated dense public data rejected");
    auto public_oversized = public_data;
    public_oversized.push_back(0U);
    expect_error(ErrorCode::InvalidLength,
                 [&] { (void)deserialize_public_data(public_oversized); },
                 "oversized dense public data rejected");
    auto invalid_public_dimensions = public_data;
    invalid_public_dimensions[148] = 0U;
    expect_error(ErrorCode::InvalidShape,
                 [&] { (void)deserialize_public_data(invalid_public_dimensions); },
                 "invalid dense public dimensions rejected");
    auto malformed_config = config_bytes;
    malformed_config[51] = 1U;
    expect_error(ErrorCode::InvalidValue,
                 [&] { (void)MiningConfiguration::from_bytes(malformed_config); },
                 "nonzero reserved config byte rejected");
    const std::array<std::uint8_t, 6U> noncanonical_pattern = {0U, 1U, 1U, 0U, 0U, 0U};
    expect_error(ErrorCode::NonCanonical,
                 [&] { (void)PeriodicPattern::from_bytes(noncanonical_pattern); },
                 "noncanonical periodic pattern rejected");

    validate_configuration(config, 9U, 250U, 0U, 0U);
    expect(config.rows_pattern.indices() == std::vector<std::uint32_t>{0U, 8U},
           "rows pattern indices");
    expect(config.cols_pattern.indices().front() == 0U
               && config.cols_pattern.indices().back() == 249U
               && config.cols_pattern.indices().size() == kSelectedColumns,
           "columns pattern indices");
    auto structural = config;
    structural.rank = 64U;
    structural.common_dim = 1024U;
    validate_configuration(structural, 9U, 250U, 0U, 0U, ValidationProfile::Structural);
    expect_error(ErrorCode::InvalidShape,
                 [&] { validate_configuration(structural, 9U, 250U, 0U, 0U); },
                 "current rank floor enforced");
    auto too_small = config;
    too_small.common_dim = 960U;
    expect_error(ErrorCode::InvalidShape,
                 [&] { validate_configuration(too_small, 9U, 250U, 0U, 0U); },
                 "small common dimension rejected");
    expect_error(ErrorCode::OutOfBounds,
                 [&] { validate_configuration(config, 8U, 250U, 0U, 0U); },
                 "selected row outside matrix rejected");
}

void test_quantization()
{
    const float scale = 2.0F / 63.0F;
    const std::vector<float> input = {
        -65.0F, -64.0F, -63.0F, 0.0F, 63.0F, 64.0F,
        2.0F, scale * 0.5F, scale * 1.5F, -scale * 1.5F, 0.0F, 0.0F,
    };
    const QuantizedMatrix result = quantize_fp32(input, 2U, 6U);
    expect(result.values.at(0U, 0U) == -63 && result.values.at(0U, 5U) == 62,
           "quantization clamps and scales with fp32 values");
    expect(result.values.at(1U, 2U) == 2 && result.values.at(1U, 3U) == -2,
           "quantization uses ties-to-even rounding");
    expect(result.values.at(1U, 0U) == 63, "quantization row maximum reaches 63");
    expect(result.row_scales.size() == 2U && result.row_scales[0] == 65.0F / 63.0F,
           "quantization stores fp32 row scale");
    const std::vector<float> zeros(6U, 0.0F);
    const QuantizedMatrix zero_result = quantize_fp32(zeros, 1U, 6U);
    expect(zero_result.row_scales[0] == 0.0F
               && std::all_of(zero_result.values.values().begin(),
                              zero_result.values.values().end(),
                              [](std::int8_t value) { return value == 0; }),
           "zero quantization row is deterministic");
    expect_error(ErrorCode::InvalidLength,
                 [&] { (void)quantize_fp32(std::vector<float>{1.0F, 2.0F}, 1U, 3U); },
                 "quantization length mismatch rejected");
    expect_error(ErrorCode::InvalidValue,
                 [&] { (void)quantize_fp32(std::vector<float>{1.0F, std::numeric_limits<float>::infinity()}, 1U, 2U); },
                 "nonfinite quantization input rejected");
}

void test_gemm_and_overflow()
{
    const Int8Matrix left(2U, 3U, {-64, -1, 63, 2, 7, -8});
    const Int8Matrix right(3U, 2U, {-64, 3, 5, -7, 63, -2});
    const Int32Matrix product = gemm_checked(left, right);
    expect(product.at(0U, 0U) == 8060 && product.at(0U, 1U) == -311,
           "signed int8 GEMM first row");
    expect(product.at(1U, 0U) == -597 && product.at(1U, 1U) == -27,
           "signed int8 GEMM second row");

    const std::size_t large_inner = 524288U;
    std::vector<std::int8_t> large_left(large_inner, 127);
    std::vector<std::int8_t> large_right(large_inner, 127);
    const Int8Matrix overflow_left(1U, large_inner, std::move(large_left));
    const Int8Matrix overflow_right(large_inner, 1U, std::move(large_right));
    expect_error(ErrorCode::ArithmeticOverflow,
                 [&] { (void)gemm_checked(overflow_left, overflow_right); },
                 "GEMM overflow is rejected rather than wrapped");
    const Int8Matrix valid_signal_boundaries(1U, 2U, {-64, 64});
    valid_signal_boundaries.require_signal_range();
    const Int8Matrix invalid_signal(1U, 2U, {-65, 65});
    expect_error(ErrorCode::InvalidValue,
                 [&] { invalid_signal.require_signal_range(); },
                 "raw matrix signal boundary rejects -65 and +64");
}

void test_noise_and_merkle(bool dump)
{
    const MiningConfiguration config = current_config();
    const std::array<std::size_t, 2U> a_rows = {0U, 8U};
    const std::vector<std::uint32_t> column_indices = config.cols_pattern.indices();
    std::vector<std::size_t> b_columns(column_indices.begin(), column_indices.end());
    const Digest key = repeated_digest(0x11U);
    const Digest hash_a = repeated_digest(0x22U);
    const Digest hash_b = repeated_digest(0x33U);
    const CommitmentSeeds seeds = commitment_seeds(key, hash_a, hash_b);
    const NoiseMatrices noise = generate_noise(1024U, 128U, seeds, a_rows, b_columns);
    const NoiseMatrices noise_repeat = generate_noise(1024U, 128U, seeds, a_rows, b_columns);
    expect(noise.noise_a.values() == noise_repeat.noise_a.values()
               && noise.noise_b_transposed.values() == noise_repeat.noise_b_transposed.values(),
           "noise generation is deterministic for a seed");
    for (const std::int8_t value : noise.noise_a.values()) {
        expect(value >= -64 && value <= 64, "A noise range");
    }
    for (const std::int8_t value : noise.noise_b_transposed.values()) {
        expect(value >= -64 && value <= 64, "B noise range");
    }
    auto changed_seed = seeds;
    changed_seed.a_noise_seed[0] ^= 1U;
    const NoiseMatrices different_noise = generate_noise(1024U, 128U, changed_seed, a_rows, b_columns);
    expect(noise.noise_a.values() != different_noise.noise_a.values(), "noise seed changes output");

    const Int8Matrix matrix = fixture_matrix(9U, 2048U, 0x12345678U);
    const MatrixOpening opening = open_matrix_rows(matrix, key, a_rows);
    expect(verify_merkle_proof(opening.proof, key), "Merkle opening verifies");
    const auto extracted = extract_opening_bytes(opening.proof, 8U * 2048U, 2048U);
    const auto raw = matrix.raw_bytes();
    expect(std::equal(extracted.begin(), extracted.end(), raw.begin() + 8U * 2048U),
           "Merkle opening extracts selected row");
    auto tampered = opening.proof;
    tampered.leaf_data[0][0] ^= 1U;
    expect(!verify_merkle_proof(tampered, key), "Merkle leaf tampering is detected");
    expect_error(ErrorCode::OutOfBounds,
                 [&] { (void)extract_opening_bytes(opening.proof, 0U, 1024U * 20U); },
                 "opening extraction fails closed when a leaf is absent");
    std::vector<std::uint8_t> pattern_data(2048U, 0U);
    for (std::size_t index = 0U; index < pattern_data.size(); ++index) {
        pattern_data[index] = static_cast<std::uint8_t>(index & 0xFFU);
    }
    expect(hex(merkle_root(pattern_data, key))
               == "aa17a0831b07bb7ed899783326e09ee7f4cfde523218c14c7eaedeeb069f7531",
           "upstream pearl-blake3 Merkle root comparison");
    if (dump) {
        std::cout << "noise_a=" << hex(blake3_keyed(repeated_digest(0U), noise.noise_a.raw_bytes())) << '\n';
        std::cout << "noise_b=" << hex(blake3_keyed(repeated_digest(0U), noise.noise_b_transposed.raw_bytes())) << '\n';
        std::cout << "merkle_fixture=" << hex(opening.proof.root) << '\n';
    }
}

void test_seeded_randomized()
{
    std::uint64_t state = 0xD1CEB00C12345678ULL;
    const std::array<std::size_t, 3U> ranks = {32U, 64U, 128U};
    for (std::size_t case_index = 0U; case_index < 24U; ++case_index) {
        const std::size_t rank = ranks[case_index % ranks.size()];
        const std::size_t k = rank == 32U ? 1024U : 2048U;
        Digest key{};
        Digest hash_a{};
        Digest hash_b{};
        for (std::size_t byte = 0U; byte < kDigestBytes; ++byte) {
            key[byte] = static_cast<std::uint8_t>(next_word(state) & 0xFFU);
            hash_a[byte] = static_cast<std::uint8_t>(next_word(state) & 0xFFU);
            hash_b[byte] = static_cast<std::uint8_t>(next_word(state) & 0xFFU);
        }
        const std::array<std::size_t, 2U> rows = {0U, 8U};
        std::vector<std::size_t> columns(kSelectedColumns, 0U);
        for (std::size_t column = 0U; column < columns.size(); ++column) {
            columns[column] = case_index % 2U == 0U
                ? column
                : (column / 2U) * 8U + (column % 2U);
        }
        const CommitmentSeeds seeds = commitment_seeds(key, hash_a, hash_b);
        const NoiseMatrices noise = generate_noise(k, rank, seeds, rows, columns);
        const NoiseMatrices repeat = generate_noise(k, rank, seeds, rows, columns);
        expect(noise.noise_a.values() == repeat.noise_a.values()
                   && noise.noise_b_transposed.values() == repeat.noise_b_transposed.values(),
               "seeded random noise repeatability");
        const Int8Matrix a = fixture_matrix(2U, k, next_word(state));
        const Int8Matrix b = fixture_matrix(k, kSelectedColumns, next_word(state));
        const NoisedOperands operands = make_noised_operands(a, b, noise);
        const Int32Matrix noised = gemm_checked(operands.a, operands.b);
        const Int32Matrix original = gemm_checked(a, b);
        const Int32Matrix recovered = denoise_product_checked(a, b, noise, noised);
        expect(recovered.values() == original.values(), "seeded random denoising exactness");
        const TranscriptResult transcript = selected_transcript(operands.a, operands.b, noised, rank);
        expect(transcript.trace.size() == k / rank, "seeded random transcript length");
        const Digest digest = jackpot_hash(transcript.words, seeds.a_noise_seed);
        expect(jackpot_meets_target(digest, digest), "seeded random target equality");
        expect(!jackpot_meets_target(digest, Digest{}) || digest == Digest{},
               "seeded random zero target ordering");
    }
}

void test_transcript_and_plain_proof(bool dump)
{
    const MiningConfiguration config = current_config();
    const std::uint32_t m = 9U;
    const std::uint32_t n = 250U;
    const std::uint32_t k = 2048U;
    const std::uint32_t rank = 128U;
    const IncompleteBlockHeader header{0x20000001U,
                                      repeated_digest(0x01U),
                                      repeated_digest(0x02U),
                                      0x66666666U,
                                      0x207FFFFFU};
    const Digest key = job_key(header, config);
    const Int8Matrix a = fixture_matrix(m, k, 0xA11CEU);
    const Int8Matrix b = fixture_matrix(k, n, 0xB0B0U);
    const Int8Matrix bt = transpose(b);
    const std::vector<std::uint32_t> col_u32 = config.cols_pattern.indices();
    const std::vector<std::size_t> columns(col_u32.begin(), col_u32.end());
    const std::array<std::size_t, 2U> rows = {0U, 8U};
    const Int8Matrix a_selected = select_rows(a, rows);
    const Int8Matrix b_selected = select_columns(b, columns);
    const CommitmentSeeds seeds = commitment_seeds(key,
                                                   merkle_root(a.raw_bytes(), key),
                                                   merkle_root(bt.raw_bytes(), key));
    const NoiseMatrices noise = generate_noise(k, rank, seeds, rows, columns);
    const NoisedOperands operands = make_noised_operands(a_selected, b_selected, noise);
    const Int32Matrix noised_product = gemm_checked(operands.a, operands.b);
    const TranscriptResult transcript = selected_transcript(
        operands.a, operands.b, noised_product, rank);
    expect(transcript.trace.size() == 16U, "one transcript step per full rank chunk");
    expect(transcript.trace.back().state == transcript.words, "transcript trace ends at final state");
    const Digest hash_a = merkle_root(a.raw_bytes(), key);
    const Digest hash_b = merkle_root(bt.raw_bytes(), key);
    expect(hex(key) == "13038bff01365936baf6f890b92cbdc3fc1bc4d5f9ae9cd13dc33ce1bdbb6fb5",
           "job key canonical vector");
    expect(hex(hash_a) == "c9e8ab596522cc18141fc06b85f5dc6f7a630ea1924b68b85e125c75909d5fdf",
           "A commitment canonical vector");
    expect(hex(hash_b) == "5c275f3d65b38abee77b3d023f682a7b4583c7543306ac305c768e4e5ace0a54",
           "B commitment canonical vector");
    const std::array<std::uint32_t, kTranscriptWords> expected_words = {
        0x0000CA65U, 0xFFFFBA7EU, 0xFFFF01BDU, 0x00001D4DU,
        0xFFFEE55BU, 0xFFFEBA6AU, 0x0001D39EU, 0x00027572U,
        0xFFFCF4DAU, 0x00027C34U, 0x00020A79U, 0xFFFFA4F1U,
        0x0001EF93U, 0x000017BBU, 0xFFFD706EU, 0xFFFD3DFAU,
    };
    expect(transcript.words == expected_words, "transcript canonical vector");
    // The public API exposes the exact chained commitment through the seed
    // helper; derive the same values independently for the proof envelope.
    const CommitmentSeeds derived = commitment_seeds(key, hash_a, hash_b);
    const Digest actual_commitment_b = [&] {
        // H(key || hash_b) and H(commitment_b || hash_a) are intentionally
        // obtained through the same clean-room BLAKE3 boundary used by the
        // implementation. The zero key is not part of the protocol.
        return derived.b_noise_seed;
    }();
    const Digest actual_commitment_a = derived.a_noise_seed;
    expect(hex(actual_commitment_b) == "9140b2d61669f1c596abf40f936acde2ae8ac0d1f81205fd0cd8981fd7e66124",
           "B noise seed canonical vector");
    expect(hex(actual_commitment_a) == "7d9ec642653bdc89a826c93153753d9244d8df3fa8e79d6dc39fe3852ba8064e",
           "A noise seed canonical vector");
    const Digest jackpot = jackpot_hash(transcript.words, actual_commitment_a);
    expect(hex(jackpot) == "98e6852e2dc76e954d18d7008f6433c12ed3a56169e50e00b45e5b9e5014462b",
           "jackpot canonical vector");
    expect(jackpot_meets_target(jackpot, repeated_digest(0xFFU)), "maximum target accepts jackpot");
    expect(jackpot_meets_target(jackpot, jackpot), "target equality is accepted");
    auto below = jackpot;
    bool decremented = false;
    for (std::size_t index = 0U; index < below.size(); ++index) {
        if (below[index] != 0U) {
            --below[index];
            decremented = true;
            break;
        }
        below[index] = 0xFFU;
    }
    if (decremented) {
        expect(!jackpot_meets_target(jackpot, below), "target below jackpot rejects");
    }
    expect(jackpot_meets_target(Digest{}, Digest{}), "zero equals zero target");
    expect(!jackpot_meets_target(repeated_digest(1U), Digest{}), "nonzero hash misses zero target");
    expect(target_from_bytes(std::vector<std::uint8_t>(32U, 0xAAU))
               == repeated_digest(0xAAU),
           "target fixed-width parser");
    expect_error(ErrorCode::InvalidLength,
                 [&] { (void)target_from_bytes(std::vector<std::uint8_t>(31U, 0U)); },
                 "truncated target rejected");
    expect_error(ErrorCode::InvalidLength,
                 [&] { (void)target_from_bytes(std::vector<std::uint8_t>(33U, 0U)); },
                 "oversized target rejected");

    PlainProof proof;
    proof.header = header;
    proof.config = config;
    proof.header_config_key = key;
    proof.hash_a = hash_a;
    proof.hash_b = hash_b;
    proof.commitment_b = actual_commitment_b;
    proof.commitment_a = actual_commitment_a;
    proof.jackpot = jackpot;
    proof.target = repeated_digest(0xFFU);
    proof.m = m;
    proof.n = n;
    proof.k = k;
    proof.rank = rank;
    proof.t_rows = 0U;
    proof.t_cols = 0U;
    proof.a_opening = open_matrix_rows(a, key, rows);
    proof.bt_opening = open_matrix_rows(bt, key, columns);
    proof.transcript = transcript;
    const std::vector<std::uint8_t> serialized = proof.serialize();
    expect(PlainProof::deserialize(serialized).serialize() == serialized,
           "PlainProof canonical round trip");
    auto truncated = serialized;
    truncated.pop_back();
    expect_error(ErrorCode::InvalidLength,
                 [&] { (void)PlainProof::deserialize(truncated); },
                 "truncated PlainProof rejected");
    auto bad_version = serialized;
    bad_version[0] = 2U;
    expect_error(ErrorCode::InvalidValue,
                 [&] { (void)PlainProof::deserialize(bad_version); },
                 "unknown PlainProof version rejected");
    auto oversized = serialized;
    oversized.push_back(0U);
    expect_error(ErrorCode::InvalidLength,
                 [&] { (void)PlainProof::deserialize(oversized); },
                 "oversized PlainProof with trailing bytes rejected");

    if (dump) {
        std::cout << "header=" << hex(std::span<const std::uint8_t>(serialize_header(header))) << '\n';
        std::cout << "config=" << hex(std::span<const std::uint8_t>(config.to_bytes())) << '\n';
        std::cout << "job_key=" << hex(key) << '\n';
        std::cout << "hash_a=" << hex(hash_a) << '\n';
        std::cout << "hash_b=" << hex(hash_b) << '\n';
        std::cout << "commitment_b=" << hex(actual_commitment_b) << '\n';
        std::cout << "commitment_a=" << hex(actual_commitment_a) << '\n';
        std::cout << "jackpot=" << hex(jackpot) << '\n';
        std::cout << "transcript_words=";
        for (const std::uint32_t word : transcript.words) {
            std::cout << std::hex << std::setfill('0') << std::setw(8) << word;
        }
        std::cout << '\n';
        for (const TranscriptStep& step : transcript.trace) {
            std::cout << "trace=" << std::dec << step.reduction_index << ':'
                      << std::hex << std::setfill('0') << std::setw(8) << step.combined_xor << ':';
            for (const std::uint32_t word : step.state) {
                std::cout << std::setw(8) << word;
            }
            std::cout << '\n';
        }
        std::cout << "plain_proof_bytes=" << std::dec << serialized.size() << '\n';
    }
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const bool dump = argc > 1 && std::string(argv[1]) == "--dump";
        test_serialization_and_validation();
        test_quantization();
        test_gemm_and_overflow();
        test_noise_and_merkle(dump);
        test_seeded_randomized();
        test_transcript_and_plain_proof(dump);
        std::cout << "pearl CPU golden tests passed (" << assertions << " assertions)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "pearl CPU golden test failure: " << error.what() << '\n';
        return 1;
    }
}
