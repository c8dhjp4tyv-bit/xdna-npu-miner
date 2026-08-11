#define NOCPP

#include <stdint.h>

#include <aie_api/aie.hpp>

namespace {

constexpr unsigned kRows = 4U;
constexpr unsigned kCommon = 64U;
constexpr unsigned kColumns = 8U;
constexpr unsigned kMicroCommon = 8U;
constexpr unsigned kMicroBlocks = kCommon / kMicroCommon;

} // namespace

// The host-facing buffers are row-major [4,64], [64,8], and [4,8].  The
// IRON consumer/producer views apply the AIE2 4x8x8 lane layout before this
// function runs and restore row-major order after it returns.  Each inner
// iteration is one AIE2 int8xint8->acc32 matrix multiply; the eight
// micro-accumulations cover the full K=64 reduction without scalar
// element-by-element arithmetic in the kernel.
extern "C" void pearl_gemm_i8_i32(const int8_t* left,
                                   const int8_t* right,
                                   int32_t* output)
{
    using Mmul = aie::mmul<4, 8, 8, int8, int8>;
    Mmul accumulator = aie::zeros<acc32, Mmul::size_C>();
    for (unsigned block = 0U; block < kMicroBlocks; ++block) {
        const auto left_tile = aie::load_v<Mmul::size_A>(left + block * kRows * kMicroCommon);
        const auto right_tile = aie::load_v<Mmul::size_B>(right + block * kMicroCommon * kColumns);
        accumulator.mac(left_tile, right_tile);
    }
    aie::store_v(output, accumulator.template to_vector<int32_t>());
}
