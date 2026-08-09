#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace xdna::runtime {

constexpr std::size_t kSmokeElementCount = 32U;
constexpr std::size_t kSmokeElementBytes = sizeof(std::int32_t);
constexpr std::size_t kSmokeBufferBytes = kSmokeElementCount * kSmokeElementBytes;
constexpr std::size_t kSmokeAlignmentBytes = alignof(std::int32_t);

enum class BufferValidation {
    Valid,
    ZeroLength,
    WrongElementCount,
    WrongByteCount,
    Misaligned,
};

[[nodiscard]] BufferValidation validate_smoke_buffer(std::size_t element_count,
                                                     std::size_t byte_count,
                                                     std::uintptr_t address) noexcept;

[[nodiscard]] std::string_view buffer_validation_name(BufferValidation validation) noexcept;

} // namespace xdna::runtime
