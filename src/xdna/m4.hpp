#pragma once

#include "bpp9000/types.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace xdna::runtime {

// M4 deliberately keeps the M3 logical K1 contract and adds only the control
// and input arenas needed for repeated ticks and one complete window. The
// artifact is fixed to the production neuron shape; sequence/window lengths
// remain per-dispatch metadata so reduced deterministic fixtures can exercise
// the same semantics.
constexpr std::size_t kM4Population = 64U;
constexpr std::size_t kM4InputTrits = 18U;
constexpr std::size_t kM4NeighborsPerNeuron = 3U;
constexpr std::size_t kM4UpdatedNeurons = 46U;
constexpr std::size_t kM4LutEntries = 27U;
constexpr std::size_t kM4LutRowStride = 32U;
constexpr std::size_t kM4StateLogicalBytes = kM4Population;
constexpr std::size_t kM4StateStrideBytes = 96U;
constexpr std::size_t kM4LutBytes = kM4UpdatedNeurons * kM4LutRowStride;
constexpr std::size_t kM4NeighborsCount = kM4Population * kM4NeighborsPerNeuron;
constexpr std::size_t kM4UpdatedDeviceCount = 48U;
constexpr std::size_t kM4InputRoleCount = kM4InputTrits;
constexpr std::size_t kM4MaxWindowWidth = 672U;
constexpr std::size_t kM4MaxInputSequenceBytes = kM4MaxWindowWidth * kM4InputTrits;
constexpr std::size_t kM4MaxTargetBytes = kM4MaxWindowWidth + 1U;

constexpr std::size_t kM4ControlOffset = 0U;
constexpr std::size_t kM4ControlWords = 16U;
constexpr std::size_t kM4StateOffset = 64U;
constexpr std::size_t kM4LutOffset = kM4StateOffset + kM4StateStrideBytes;
constexpr std::size_t kM4NeighborsOffset = kM4LutOffset + kM4LutBytes;
constexpr std::size_t kM4UpdatedOffset = kM4NeighborsOffset + kM4NeighborsCount * sizeof(std::uint32_t);
constexpr std::size_t kM4InputRolesOffset = kM4UpdatedOffset + kM4UpdatedDeviceCount * sizeof(std::uint32_t);
constexpr std::size_t kM4InputSequenceOffset = 2688U;
constexpr std::size_t kM4TargetsOffset = kM4InputSequenceOffset + kM4MaxInputSequenceBytes;
constexpr std::size_t kM4InputBufferBytes = kM4TargetsOffset + kM4MaxTargetBytes;

constexpr std::size_t kM4OutputStateOffset = 0U;
constexpr std::size_t kM4OutputScoreOffset = 96U;
constexpr std::size_t kM4OutputStatusOffset = 100U;
constexpr std::size_t kM4OutputTicksOffset = 104U;
constexpr std::size_t kM4OutputFeedCountOffset = 108U;
constexpr std::size_t kM4OutputPredictedOffset = 112U;
constexpr std::size_t kM4OutputExpectedOffset = 116U;
constexpr std::size_t kM4OutputMagicOffset = 120U;
constexpr std::size_t kM4OutputBufferBytes = 128U;

constexpr std::uint32_t kM4InputMagic = 0x3150344DU; // little-endian "M4P1"
constexpr std::uint32_t kM4OutputMagic = 0x3152344DU; // little-endian "M4R1"

enum class M4Mode : std::uint32_t {
    SingleTick = 0U,
    RepeatedTicks = 1U,
    WindowScore = 2U,
};

enum class M4DeviceStatus : std::uint32_t {
    Settled = 0U,
    Timeout = 1U,
};

struct M4DeviceLayout {
    std::size_t input_buffer_bytes = kM4InputBufferBytes;
    std::size_t output_buffer_bytes = kM4OutputBufferBytes;
    std::size_t state_offset = kM4StateOffset;
    std::size_t lut_offset = kM4LutOffset;
    std::size_t neighbors_offset = kM4NeighborsOffset;
    std::size_t updated_offset = kM4UpdatedOffset;
    std::size_t input_roles_offset = kM4InputRolesOffset;
    std::size_t input_sequence_offset = kM4InputSequenceOffset;
    std::size_t targets_offset = kM4TargetsOffset;
};

[[nodiscard]] constexpr M4DeviceLayout m4_default_layout() noexcept
{
    return {};
}

class M4ContractError final : public std::runtime_error {
public:
    explicit M4ContractError(const char* message)
        : std::runtime_error(message)
    {
    }
};

struct M4LogicalInput {
    M4Mode mode = M4Mode::SingleTick;
    std::uint32_t tick_count = 1U;
    std::uint32_t window_width = 0U;
    std::uint32_t max_ticks = 0U;
    std::uint32_t output_neuron = 0U;
    std::uint32_t signal_neuron = 0U;
    std::span<const bpp9000::Byte> initial_state;
    std::span<const bpp9000::Byte> lut;
    std::span<const std::uint32_t> neighbors;
    std::span<const std::uint32_t> updated_neurons;
    std::span<const std::uint32_t> input_neurons;
    std::span<const bpp9000::Byte> input_sequence;
    std::span<const bpp9000::Byte> targets;
};

struct M4PackedBuffers {
    std::vector<bpp9000::Byte> input;
    std::vector<bpp9000::Byte> output;
};

struct M4DeviceResult {
    std::vector<bpp9000::Byte> state;
    std::uint32_t score = bpp9000::kTimeoutScore;
    M4DeviceStatus status = M4DeviceStatus::Timeout;
    std::uint32_t ticks = 0U;
    std::uint32_t feed_count = 0U;
    bpp9000::Trit predicted = bpp9000::Trit::Unknown;
    bpp9000::Trit expected = bpp9000::Trit::Unknown;

    [[nodiscard]] bool timed_out() const noexcept
    {
        return status == M4DeviceStatus::Timeout;
    }
};

void validate_m4_device_layout(const M4DeviceLayout& layout);
void validate_m4_logical_input(const M4LogicalInput& input,
                               const M4DeviceLayout& layout = {});
void validate_m4_packed_input(const M4PackedBuffers& buffers,
                              const M4DeviceLayout& layout = {});
void validate_m4_output(std::span<const bpp9000::Byte> output,
                        const M4DeviceLayout& layout = {});

[[nodiscard]] M4PackedBuffers pack_m4(const M4LogicalInput& input,
                                      const M4DeviceLayout& layout = {});
[[nodiscard]] M4DeviceResult unpack_m4_result(std::span<const bpp9000::Byte> output,
                                              const M4DeviceLayout& layout = {});

} // namespace xdna::runtime
