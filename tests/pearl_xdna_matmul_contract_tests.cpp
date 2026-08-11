#include "pearl/xdna_matmul.hpp"

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_fixed_shape_contract()
{
    using namespace xdna::pearl;
    expect(kP2Rows == 4U && kP2Common == 64U && kP2Columns == 8U,
           "P2 fixed tile changed");
    expect(kP2LeftBytes == 256U && kP2RightBytes == 512U && kP2OutputBytes == 128U,
           "P2 byte contract changed");

    std::vector<std::int8_t> left(kP2Rows * kP2Common, 1);
    std::vector<std::int8_t> right(kP2Common * kP2Columns, 1);
    const Int8Matrix a(kP2Rows, kP2Common, std::move(left));
    const Int8Matrix b(kP2Common, kP2Columns, std::move(right));
    const Int32Matrix expected = gemm_checked(a, b);
    expect(expected.rows() == kP2Rows && expected.cols() == kP2Columns,
           "CPU P2 oracle shape");
    for (const std::int32_t value : expected.values()) {
        expect(value == static_cast<std::int32_t>(kP2Common),
               "CPU P2 oracle all-ones vector");
    }
}

} // namespace

int main()
{
    test_fixed_shape_contract();
    return 0;
}
