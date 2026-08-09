#include "xdna/k1.hpp"

#include <algorithm>
#include <limits>

namespace xdna::runtime {
namespace {

[[nodiscard]] std::size_t expected_state_bytes(const K1DeviceLayout& layout)
{
    return layout.state_stride_bytes;
}

[[nodiscard]] std::size_t expected_lut_bytes(const K1DeviceLayout& layout)
{
    return layout.lut_rows * layout.lut_row_stride_bytes;
}

[[nodiscard]] std::size_t expected_neighbors_count(const K1DeviceLayout& layout)
{
    return layout.neighbors_rows * layout.neighbors_per_row;
}

[[nodiscard]] std::size_t expected_updated_count(const K1DeviceLayout& layout)
{
    return layout.updated_device_count;
}

void require(bool condition, const char* message)
{
    if (!condition) {
        throw K1ContractError(message);
    }
}

} // namespace

void validate_k1_device_layout(const K1DeviceLayout& layout)
{
    require(layout.input_buffer_bytes == kK1InputDeviceBytes, "K1 combined input buffer size is invalid");
    require(layout.state_device_offset == kK1StateDeviceOffset, "K1 state sub-buffer offset is invalid");
    require(layout.lut_device_offset == kK1LutDeviceOffset, "K1 LUT sub-buffer offset is invalid");
    require(layout.neighbors_device_offset == kK1NeighborsDeviceOffset,
            "K1 neighbor sub-buffer offset is invalid");
    require(layout.updated_device_offset == kK1UpdatedDeviceOffset,
            "K1 updated-neuron sub-buffer offset is invalid");
    require(layout.state_device_offset + layout.state_stride_bytes <= layout.input_buffer_bytes,
            "K1 state sub-buffer exceeds the combined input buffer");
    require(layout.lut_device_offset + layout.lut_rows * layout.lut_row_stride_bytes <= layout.input_buffer_bytes,
            "K1 LUT sub-buffer exceeds the combined input buffer");
    require(layout.neighbors_device_offset + k1_neighbors_device_bytes() <= layout.input_buffer_bytes,
            "K1 neighbor sub-buffer exceeds the combined input buffer");
    require(layout.updated_device_offset + k1_updated_device_bytes() <= layout.input_buffer_bytes,
            "K1 updated-neuron sub-buffer exceeds the combined input buffer");
    require(layout.state_logical_bytes == kK1StateLogicalBytes, "K1 state logical length is not 64 bytes");
    require(layout.state_stride_bytes == 96U, "K1 state stride must be 96 bytes");
    require(layout.lut_rows == kK1UpdatedNeurons, "K1 LUT row count is not 46");
    require(layout.lut_logical_entries == kK1LutEntries, "K1 LUT logical entry count is not 27");
    require(layout.lut_row_stride_bytes == kK1LutRowStride, "K1 LUT row stride must be 32 bytes");
    require(layout.neighbors_rows == kK1Population, "K1 neighbor topology must have 64 rows");
    require(layout.neighbors_per_row == kK1NeighborsPerNeuron, "K1 topology must have K=3");
    require(layout.neighbors_row_stride_bytes == 3U * sizeof(std::uint32_t),
            "K1 neighbor row stride must be 12 bytes");
    require(layout.updated_logical_count == kK1UpdatedLogicalCount,
            "K1 updated-neuron logical count is not 46");
    require(layout.updated_device_count == 48U, "K1 updated-neuron device padding must provide 48 words");
    require(layout.updated_stride_bytes == sizeof(std::uint32_t),
            "K1 updated-neuron stride must be four bytes");
    require(expected_state_bytes(layout) % 32U == 0U, "K1 state stride must be AIE-aligned");
    require(expected_lut_bytes(layout) % 32U == 0U, "K1 LUT storage must be AIE-aligned");
    require((expected_neighbors_count(layout) * sizeof(std::uint32_t)) % 32U == 0U,
            "K1 neighbor storage must be AIE-aligned");
    require((expected_updated_count(layout) * layout.updated_stride_bytes) % 32U == 0U,
            "K1 updated-neuron storage must be AIE-aligned");
}

void validate_k1_logical_input(const K1LogicalInput& input)
{
    require(input.previous_state.size() == kK1StateLogicalBytes, "K1 state length must be exactly 64 bytes");
    require(input.lut.size() == kK1LutLogicalBytes, "K1 LUT length must be exactly 46*32 bytes");
    require(input.neighbors.size() == kK1NeighborsLogicalCount,
            "K1 neighbor topology length must be exactly 64*3 uint32 values");
    require(input.updated_neurons.size() == kK1UpdatedLogicalCount,
            "K1 updated-neuron list length must be exactly 46 uint32 values");

    for (const bpp9000::Byte value : input.previous_state) {
        require(bpp9000::is_valid_trit_byte(value), "K1 state contains a trit outside 0, 1, 2");
    }
    for (std::size_t row = 0U; row < kK1UpdatedNeurons; ++row) {
        for (std::size_t entry = 0U; entry < kK1LutEntries; ++entry) {
            require(bpp9000::is_valid_trit_byte(input.lut[row * kK1LutRowStride + entry]),
                    "K1 LUT contains a trit outside 0, 1, 2");
        }
    }
    for (const std::uint32_t neighbor : input.neighbors) {
        require(neighbor < kK1Population, "K1 neighbor index is outside the 64-neuron state");
    }
    std::uint32_t previous = 0U;
    for (std::size_t row = 0U; row < input.updated_neurons.size(); ++row) {
        const std::uint32_t neuron = input.updated_neurons[row];
        require(neuron < kK1Population, "K1 updated-neuron index is outside the 64-neuron state");
        if (row != 0U) {
            require(neuron > previous, "K1 updated-neuron rows must be strictly ascending M1 row order");
        }
        previous = neuron;
    }
}

void validate_k1_packed_input(const K1PackedBuffers& buffers, const K1DeviceLayout& layout)
{
    validate_k1_device_layout(layout);
    require(buffers.previous_state.size() == expected_state_bytes(layout),
            "K1 packed state size does not match the device stride");
    require(buffers.lut.size() == expected_lut_bytes(layout), "K1 packed LUT size does not match the device layout");
    require(buffers.neighbors.size() == expected_neighbors_count(layout),
            "K1 packed neighbor size does not match the device layout");
    require(buffers.updated_neurons.size() == expected_updated_count(layout),
            "K1 packed updated-neuron size does not match the device layout");
    require(buffers.next_state.size() == expected_state_bytes(layout),
            "K1 packed output size does not match the device stride");

    K1LogicalInput logical{
        std::span<const bpp9000::Byte>(buffers.previous_state).first(kK1StateLogicalBytes),
        std::span<const bpp9000::Byte>(buffers.lut),
        std::span<const std::uint32_t>(buffers.neighbors),
        std::span<const std::uint32_t>(buffers.updated_neurons).first(kK1UpdatedLogicalCount),
    };
    validate_k1_logical_input(logical);
}

void validate_k1_output(std::span<const bpp9000::Byte> device_output, const K1DeviceLayout& layout)
{
    validate_k1_device_layout(layout);
    require(device_output.size() == expected_state_bytes(layout),
            "K1 device output size does not match the device stride");
    for (std::size_t index = 0U; index < kK1StateLogicalBytes; ++index) {
        require(bpp9000::is_valid_trit_byte(device_output[index]),
                "K1 device output contains a trit outside 0, 1, 2");
    }
}

K1PackedBuffers pack_k1(const K1LogicalInput& input, const K1DeviceLayout& layout)
{
    validate_k1_device_layout(layout);
    validate_k1_logical_input(input);

    K1PackedBuffers buffers;
    buffers.previous_state.assign(expected_state_bytes(layout), 0xA5U);
    std::copy(input.previous_state.begin(), input.previous_state.end(), buffers.previous_state.begin());
    buffers.lut.assign(input.lut.begin(), input.lut.end());
    buffers.neighbors.assign(input.neighbors.begin(), input.neighbors.end());
    buffers.updated_neurons.assign(expected_updated_count(layout), 0U);
    std::copy(input.updated_neurons.begin(), input.updated_neurons.end(), buffers.updated_neurons.begin());
    buffers.next_state.assign(expected_state_bytes(layout), 0x5AU);
    return buffers;
}

std::vector<bpp9000::Byte> unpack_k1(std::span<const bpp9000::Byte> device_output,
                                     const K1DeviceLayout& layout)
{
    validate_k1_output(device_output, layout);
    return std::vector<bpp9000::Byte>(device_output.begin(), device_output.begin() + kK1StateLogicalBytes);
}

K1Comparison compare_k1_output(std::span<const bpp9000::Byte> expected,
                               std::span<const bpp9000::Byte> actual)
{
    require(expected.size() == kK1StateLogicalBytes, "K1 expected output length must be 64 bytes");
    require(actual.size() == kK1StateLogicalBytes, "K1 actual output length must be 64 bytes");
    K1Comparison comparison;
    for (std::size_t index = 0U; index < expected.size(); ++index) {
        if (expected[index] != actual[index]) {
            comparison.differing_indices.push_back(index);
        }
    }
    return comparison;
}

} // namespace xdna::runtime
