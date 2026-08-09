#include "xdna/buffers.hpp"

namespace xdna::runtime {

BufferValidation validate_smoke_buffer(std::size_t element_count,
                                       std::size_t byte_count,
                                       std::uintptr_t address) noexcept
{
    if (element_count == 0U || byte_count == 0U) {
        return BufferValidation::ZeroLength;
    }
    if (element_count != kSmokeElementCount) {
        return BufferValidation::WrongElementCount;
    }
    if (byte_count != kSmokeBufferBytes) {
        return BufferValidation::WrongByteCount;
    }
    if ((address % kSmokeAlignmentBytes) != 0U) {
        return BufferValidation::Misaligned;
    }
    return BufferValidation::Valid;
}

std::string_view buffer_validation_name(BufferValidation validation) noexcept
{
    switch (validation) {
    case BufferValidation::Valid:
        return "VALID";
    case BufferValidation::ZeroLength:
        return "ZERO_LENGTH";
    case BufferValidation::WrongElementCount:
        return "WRONG_ELEMENT_COUNT";
    case BufferValidation::WrongByteCount:
        return "WRONG_BYTE_COUNT";
    case BufferValidation::Misaligned:
        return "MISALIGNED";
    }
    return "UNKNOWN";
}

} // namespace xdna::runtime
