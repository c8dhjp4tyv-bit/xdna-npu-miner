#include "pearl/compute_pipeline.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <string>
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
    constexpr std::size_t kTileCommon = 64U;
    constexpr std::size_t kTileColumns = kP2Columns;

    const auto total_begin = Clock::now();
    const auto preprocessing_begin = Clock::now();
    require_matrix_shape(a, kRows, b.rows(), "A");
    if (b.cols() != kColumns || b.rows() == 0U || b.rows() % kTileCommon != 0U) {
        throw Error(ErrorCode::InvalidShape,
                    "P3/P5 B must have 64 columns and a common dimension divisible by 64");
    }
    if (a.cols() != b.rows()) {
        throw Error(ErrorCode::InvalidShape, "A and B common dimensions differ");
    }
    const NoisedOperands noised = make_noised_operands(a, b, noise);
    const auto preprocessing_end = Clock::now();

    std::vector<std::int8_t> padded_left(kP2LeftBytes, 0);
    std::vector<std::int32_t> noised_values(kRows * kColumns, 0);
    std::uint64_t packing_ns = 0U;
    std::uint64_t npu_execution_ns = 0U;
    for (std::size_t inner_base = 0U;
         inner_base < b.rows();
         inner_base += kTileCommon) {
        const auto left_packing_begin = Clock::now();
        std::fill(padded_left.begin(), padded_left.end(), 0);
        for (std::size_t row = 0U; row < kRows; ++row) {
            for (std::size_t inner = 0U; inner < kTileCommon; ++inner) {
                padded_left[row * kTileCommon + inner] = noised.a.at(row, inner_base + inner);
            }
        }
        const Int8Matrix padded_a(kP2Rows, kP2Common, padded_left);
        packing_ns += elapsed_ns(left_packing_begin, Clock::now());
        for (std::size_t column_base = 0U;
             column_base < kColumns;
             column_base += kTileColumns) {
            const auto right_packing_begin = Clock::now();
            std::vector<std::int8_t> right_values(kTileCommon * kTileColumns, 0);
            for (std::size_t inner = 0U; inner < kTileCommon; ++inner) {
                for (std::size_t column = 0U; column < kTileColumns; ++column) {
                    right_values[inner * kTileColumns + column] =
                        noised.b.at(inner_base + inner, column_base + column);
                }
            }
            const Int8Matrix right_tile(kTileCommon, kTileColumns, std::move(right_values));
            packing_ns += elapsed_ns(right_packing_begin, Clock::now());
            const auto dispatch_begin = Clock::now();
            const Int32Matrix actual = executor_.dispatch(padded_a, right_tile);
            npu_execution_ns += elapsed_ns(dispatch_begin, Clock::now());
            for (std::size_t column = 0U; column < kTileColumns; ++column) {
                for (std::size_t row = 0U; row < kRows; ++row) {
                    noised_values[row * kColumns + column_base + column] +=
                        actual.at(row, column);
                }
            }
        }
    }
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
    result.physical_dispatches = (b.rows() / kTileCommon) * (kColumns / kTileColumns);
    result.timings.cpu_preprocessing_ns = elapsed_ns(preprocessing_begin, preprocessing_end);
    result.timings.packing_ns = packing_ns;
    result.timings.npu_execution_ns = npu_execution_ns;
    result.timings.cpu_verification_ns = elapsed_ns(verification_begin, verification_end);
    result.timings.transcript_ns = elapsed_ns(transcript_begin, transcript_end);
    result.timings.blake3_ns = elapsed_ns(hash_begin, hash_end);
    result.timings.total_ns = elapsed_ns(total_begin, total_end);
    return result;
}

} // namespace xdna::pearl
