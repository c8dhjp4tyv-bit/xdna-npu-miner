#pragma once

#include "bpp9000/reference.hpp"
#include "xdna/m4.hpp"
#include "xdna/runtime.hpp"
#include "xdna/verification.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace xdna::runtime {

struct WindowVerification {
    bpp9000::WindowResult cpu{};
    bpp9000::WindowResult npu{};
    VerificationStatus status = VerificationStatus::RejectedMismatch;

    [[nodiscard]] bool verified() const noexcept
    {
        return status == VerificationStatus::Verified;
    }
};

struct M4ScoreRun {
    bpp9000::ScoreResult cpu{};
    bpp9000::ScoreResult npu{};
    VerificationStatus status = VerificationStatus::RejectedMismatch;
    std::uint64_t windows_compared = 0U;
    std::optional<std::uint64_t> first_mismatch_window;
    std::optional<std::uint32_t> candidate_score_call;
    std::optional<std::uint32_t> candidate_mutation_step;
    std::optional<WindowVerification> first_mismatch;

    [[nodiscard]] bool verified() const noexcept
    {
        return status == VerificationStatus::Verified;
    }
};

struct M4CandidateResult {
    VerificationStatus status = VerificationStatus::RejectedMismatch;
    std::optional<bpp9000::CandidateResult> cpu_result;
    std::uint32_t score_calls = 0U;
    std::uint64_t npu_score_calls = 0U;
    std::uint64_t windows_compared = 0U;
    std::uint64_t timeout_score_calls = 0U;
    std::uint64_t finite_score_calls = 0U;
    std::optional<M4ScoreRun> first_mismatch;

    [[nodiscard]] bool verified() const noexcept
    {
        return status == VerificationStatus::Verified;
    }
};

class M4NpuScorer final {
public:
    explicit M4NpuScorer(XdnaRuntime& runtime)
        : runtime_(runtime)
    {
    }

    // These are raw NPU results. They do not authorize or verify a score.
    [[nodiscard]] M4DeviceResult dispatch_single_tick(const bpp9000::Task& task,
                                                      std::span<const bpp9000::Byte> initial_state,
                                                      const bpp9000::Lut& lut);
    [[nodiscard]] M4DeviceResult dispatch_repeated_ticks(const bpp9000::Task& task,
                                                         std::span<const bpp9000::Byte> initial_state,
                                                         const bpp9000::Lut& lut,
                                                         std::span<const bpp9000::Byte> input_sequence);
    [[nodiscard]] bpp9000::WindowResult score_window_npu(const bpp9000::Task& task,
                                                         const bpp9000::Lut& lut,
                                                         std::uint64_t window_start,
                                                         const bpp9000::ReferenceConfig& config = {});
    [[nodiscard]] bpp9000::ScoreResult score_lut_npu(const bpp9000::Task& task,
                                                     const bpp9000::Lut& lut,
                                                     const bpp9000::ReferenceConfig& config = {});

    // The verification methods always recompute through the M1 scalar path.
    // A rejected result carries both CPU and NPU observations and never
    // returns a verified candidate.
    [[nodiscard]] M4ScoreRun score_lut_verified(const bpp9000::Task& task,
                                                 const bpp9000::Lut& lut,
                                                 const bpp9000::ReferenceConfig& config = {});
    [[nodiscard]] M4CandidateResult score_candidate_verified(
        const bpp9000::Task& task,
        const bpp9000::PublicKey& public_key,
        const bpp9000::MiningSeed& mining_seed,
        const bpp9000::Nonce& nonce,
        const bpp9000::CandidateRandomSource& random_source,
        const bpp9000::ReferenceConfig& config = {});

private:
    [[nodiscard]] M4DeviceResult dispatch(const M4LogicalInput& input);
    [[nodiscard]] M4LogicalInput task_input(const bpp9000::Task& task,
                                             const bpp9000::Lut& lut,
                                             M4Mode mode,
                                             std::span<const bpp9000::Byte> initial_state,
                                             std::span<const bpp9000::Byte> input_sequence,
                                             std::span<const bpp9000::Byte> targets,
                                             std::uint32_t tick_count,
                                             std::uint32_t window_width,
                                             std::uint32_t max_ticks) const;

    XdnaRuntime& runtime_;
};

} // namespace xdna::runtime
