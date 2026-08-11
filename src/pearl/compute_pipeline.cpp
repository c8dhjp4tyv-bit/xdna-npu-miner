#include "pearl/compute_pipeline.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <vector>

namespace xdna::pearl {
namespace {

using Clock = std::chrono::steady_clock;

[[nodiscard]] std::uint64_t elapsed_ns(const Clock::time_point begin,
                                       const Clock::time_point end)
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count());
}

void require_matrix_shape(const Int8Matrix& matrix,
                          std::size_t rows,
                          std::size_t cols,
                          const char* label)
{
    if (matrix.rows() != rows || matrix.cols() != cols) {
        throw Error(ErrorCode::InvalidShape, std::string(label) + " has an unexpected shape");
    }
}

} // namespace

ComputePipelineResult ComputePipeline::run(const Int8Matrix& a,
                                           const Int8Matrix& b,
                                           const NoiseMatrices& noise,
                                           std::size_t rank,
                                           const Digest& commitment_hash,
                                           const Digest& target) const
{
    constexpr std::size_t kRows = 2U;
    constexpr std::size_t kColumns = 64U;
    constexpr std::size_t kCommon = 64U;
    constexpr std::size_t kTileColumns = kP2Columns;

    const auto total_begin = Clock::now();
    const auto preprocessing_begin = Clock::now();
    require_matrix_shape(a, kRows, kCommon, "A");
    require_matrix_shape(b, kCommon, kColumns, "B");
    const NoisedOperands noised = make_noised_operands(a, b, noise);
    noised.a.require_signal_range();
    noised.b.require_signal_range();
    const auto preprocessing_end = Clock::now();

    const auto packing_begin = Clock::now();
    std::vector<std::int8_t> padded_left(kP2LeftBytes, 0);
    for (std::size_t row = 0U; row < kRows; ++row) {
        for (std::size_t inner = 0U; inner < kCommon; ++inner) {
            padded_left[row * kCommon + inner] = noised.a.at(row, inner);
        }
    }
    const Int8Matrix padded_a(kP2Rows, kP2Common, std::move(padded_left));
    const auto packing_end = Clock::now();

    const auto npu_begin = Clock::now();
    std::vector<std::int32_t> noised_values(kRows * kColumns, 0);
    for (std::size_t column_base = 0U; column_base < kColumns; column_base += kTileColumns) {
        std::vector<std::int8_t> right_values(kCommon * kTileColumns, 0);
        for (std::size_t inner = 0U; inner < kCommon; ++inner) {
            for (std::size_t column = 0U; column < kTileColumns; ++column) {
                right_values[inner * kTileColumns + column] =
                    noised.b.at(inner, column_base + column);
            }
        }
        const Int8Matrix right_tile(kCommon, kTileColumns, std::move(right_values));
        const Int32Matrix actual = executor_.dispatch(padded_a, right_tile);
        for (std::size_t row = 0U; row < kRows; ++row) {
            for (std::size_t column = 0U; column < kTileColumns; ++column) {
                noised_values[row * kColumns + column_base + column] =
                    actual.at(row, column);
            }
        }
    }
    const auto npu_end = Clock::now();
    const Int32Matrix noised_product(kRows, kColumns, std::move(noised_values));

    const auto verification_begin = Clock::now();
    const Int32Matrix cpu_noised_product = gemm_checked(noised.a, noised.b);
    if (cpu_noised_product.values() != noised_product.values()) {
        throw Error(ErrorCode::InvalidValue, "XDNA noised product differs from CPU oracle");
    }
    const Int32Matrix denoised_product = denoise_product_checked(
        a, b, noise, noised_product);
    const auto verification_end = Clock::now();

    const auto transcript_begin = Clock::now();
    const TranscriptResult transcript = selected_transcript(
        noised.a, noised.b, noised_product, rank);
    const auto transcript_end = Clock::now();

    const auto hash_begin = Clock::now();
    const Digest jackpot = jackpot_hash(transcript.words, commitment_hash);
    const bool meets_target = jackpot_meets_target(jackpot, target);
    const auto hash_end = Clock::now();

    const auto total_end = Clock::now();
    ComputePipelineResult result;
    result.noised_product = noised_product;
    result.denoised_product = denoised_product;
    result.transcript = transcript;
    result.jackpot = jackpot;
    result.target = target;
    result.meets_target = meets_target;
    result.physical_dispatches = kColumns / kTileColumns;
    result.timings.cpu_preprocessing_ns = elapsed_ns(preprocessing_begin, preprocessing_end);
    result.timings.packing_ns = elapsed_ns(packing_begin, packing_end);
    result.timings.npu_execution_ns = elapsed_ns(npu_begin, npu_end);
    result.timings.cpu_verification_ns = elapsed_ns(verification_begin, verification_end);
    result.timings.transcript_ns = elapsed_ns(transcript_begin, transcript_end);
    result.timings.blake3_ns = elapsed_ns(hash_begin, hash_end);
    result.timings.total_ns = elapsed_ns(total_begin, total_end);
    return result;
}

} // namespace xdna::pearl
