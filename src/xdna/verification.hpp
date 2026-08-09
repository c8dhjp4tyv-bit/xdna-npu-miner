#pragma once

#include "bpp9000/types.hpp"

#include <cstdint>

namespace xdna::runtime {

enum class VerificationStatus : std::uint8_t {
    Verified = 0U,
    RejectedMismatch = 1U,
};

struct ScoreVerification {
    bpp9000::ScoreResult cpu{};
    bpp9000::ScoreResult npu{};
    VerificationStatus status = VerificationStatus::RejectedMismatch;

    [[nodiscard]] bool verified() const noexcept
    {
        return status == VerificationStatus::Verified;
    }
};

[[nodiscard]] ScoreVerification verify_score_exact(const bpp9000::ScoreResult& cpu,
                                                    const bpp9000::ScoreResult& npu) noexcept;

} // namespace xdna::runtime
