#include "xdna/buffers.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

std::size_t assertions = 0U;

void expect(bool condition, const std::string& message)
{
    ++assertions;
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main()
{
    try {
        const auto valid = xdna::runtime::validate_smoke_buffer(
            xdna::runtime::kSmokeElementCount,
            xdna::runtime::kSmokeBufferBytes,
            0x1000U);
        expect(valid == xdna::runtime::BufferValidation::Valid, "valid contract rejected");
        expect(
            xdna::runtime::validate_smoke_buffer(0U, 0U, 0x1000U)
                == xdna::runtime::BufferValidation::ZeroLength,
            "zero length accepted");
        expect(
            xdna::runtime::validate_smoke_buffer(31U, 124U, 0x1000U)
                == xdna::runtime::BufferValidation::WrongElementCount,
            "wrong element count accepted");
        expect(
            xdna::runtime::validate_smoke_buffer(32U, 124U, 0x1000U)
                == xdna::runtime::BufferValidation::WrongByteCount,
            "wrong byte count accepted");
        expect(
            xdna::runtime::validate_smoke_buffer(32U, 128U, 0x1002U)
                == xdna::runtime::BufferValidation::Misaligned,
            "misaligned buffer accepted");
        std::cout << "PASS assertions=" << assertions << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
