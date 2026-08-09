#pragma once

#include "bpp9000/types.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace xdna::runtime {

constexpr std::size_t kK1Population = 64U;
constexpr std::size_t kK1NeighborsPerNeuron = 3U;
constexpr std::size_t kK1UpdatedNeurons = 46U;
constexpr std::size_t kK1LutEntries = 27U;
constexpr std::size_t kK1LutRowStride = 32U;
constexpr std::size_t kK1StateLogicalBytes = kK1Population;
constexpr std::size_t kK1LutLogicalBytes = kK1UpdatedNeurons * kK1LutRowStride;
constexpr std::size_t kK1NeighborsLogicalCount = kK1Population * kK1NeighborsPerNeuron;
constexpr std::size_t kK1UpdatedLogicalCount = kK1UpdatedNeurons;
constexpr std::size_t kK1StateDeviceOffset = 0U;
constexpr std::size_t kK1LutDeviceOffset = 96U;
constexpr std::size_t kK1NeighborsDeviceOffset = kK1LutDeviceOffset + kK1LutLogicalBytes;
constexpr std::size_t kK1UpdatedDeviceOffset
    = kK1NeighborsDeviceOffset + kK1NeighborsLogicalCount * sizeof(std::uint32_t);
constexpr std::size_t kK1InputDeviceBytes = kK1UpdatedDeviceOffset + 48U * sizeof(std::uint32_t);

// Device buffers are explicit and stable even when the logical contract is
// smaller. The AIE kernel only reads/writes the logical prefixes described
// here; padding is never algorithm input.
struct K1DeviceLayout {
    std::size_t input_buffer_bytes = kK1InputDeviceBytes;
    std::size_t state_device_offset = kK1StateDeviceOffset;
    std::size_t lut_device_offset = kK1LutDeviceOffset;
    std::size_t neighbors_device_offset = kK1NeighborsDeviceOffset;
    std::size_t updated_device_offset = kK1UpdatedDeviceOffset;
    std::size_t state_logical_bytes = kK1StateLogicalBytes;
    std::size_t state_stride_bytes = 96U;
    std::size_t lut_rows = kK1UpdatedNeurons;
    std::size_t lut_logical_entries = kK1LutEntries;
    std::size_t lut_row_stride_bytes = kK1LutRowStride;
    std::size_t neighbors_rows = kK1Population;
    std::size_t neighbors_per_row = kK1NeighborsPerNeuron;
    std::size_t neighbors_row_stride_bytes = kK1NeighborsPerNeuron * sizeof(std::uint32_t);
    std::size_t updated_logical_count = kK1UpdatedLogicalCount;
    std::size_t updated_device_count = 48U;
    std::size_t updated_stride_bytes = sizeof(std::uint32_t);
};

[[nodiscard]] constexpr K1DeviceLayout k1_default_layout() noexcept
{
    return {};
}

[[nodiscard]] constexpr std::size_t k1_neighbors_device_bytes() noexcept
{
    return kK1NeighborsLogicalCount * sizeof(std::uint32_t);
}

[[nodiscard]] constexpr std::size_t k1_updated_device_bytes() noexcept
{
    return 48U * sizeof(std::uint32_t);
}

class K1ContractError final : public std::runtime_error {
public:
    explicit K1ContractError(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

struct K1LogicalInput {
    std::span<const bpp9000::Byte> previous_state;
    std::span<const bpp9000::Byte> lut;
    std::span<const std::uint32_t> neighbors;
    std::span<const std::uint32_t> updated_neurons;
};

struct K1PackedBuffers {
    std::vector<bpp9000::Byte> previous_state;
    std::vector<bpp9000::Byte> lut;
    std::vector<std::uint32_t> neighbors;
    std::vector<std::uint32_t> updated_neurons;
    std::vector<bpp9000::Byte> next_state;
};

struct K1Comparison {
    std::vector<std::size_t> differing_indices;

    [[nodiscard]] bool matches() const noexcept
    {
        return differing_indices.empty();
    }
};

void validate_k1_device_layout(const K1DeviceLayout& layout);
void validate_k1_logical_input(const K1LogicalInput& input);
void validate_k1_packed_input(const K1PackedBuffers& buffers, const K1DeviceLayout& layout = {});
void validate_k1_output(std::span<const bpp9000::Byte> device_output,
                        const K1DeviceLayout& layout = {});

[[nodiscard]] K1PackedBuffers pack_k1(const K1LogicalInput& input,
                                      const K1DeviceLayout& layout = {});
[[nodiscard]] std::vector<bpp9000::Byte> unpack_k1(
    std::span<const bpp9000::Byte> device_output,
    const K1DeviceLayout& layout = {});
[[nodiscard]] K1Comparison compare_k1_output(std::span<const bpp9000::Byte> expected,
                                              std::span<const bpp9000::Byte> actual);

} // namespace xdna::runtime
