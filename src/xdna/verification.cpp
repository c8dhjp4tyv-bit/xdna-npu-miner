#include "xdna/verification.hpp"

namespace xdna::runtime {

ScoreVerification verify_score_exact(const bpp9000::ScoreResult& cpu,
                                     const bpp9000::ScoreResult& npu) noexcept
{
    const bool equal = cpu.score == npu.score && cpu.status == npu.status
        && cpu.windows_evaluated == npu.windows_evaluated && cpu.ticks == npu.ticks;
    return ScoreVerification{cpu, npu, equal ? VerificationStatus::Verified : VerificationStatus::RejectedMismatch};
}

} // namespace xdna::runtime
