#include "xdna/m4.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

namespace xdna::runtime {
namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        throw M4ContractError(message);
    }
}

[[nodiscard]] std::uint32_t read_u32(std::span<const bpp9000::Byte> bytes, std::size_t offset)
{
    require(offset <= bytes.size() && bytes.size() - offset >= sizeof(std::uint32_t),
            "M4 output uint32 field is truncated");
    std::uint32_t value = 0U;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

void write_u32(std::span<bpp9000::Byte> bytes, std::size_t offset, std::uint32_t value)
{
    require(offset <= bytes.size() && bytes.size() - offset >= sizeof(std::uint32_t),
            "M4 control uint32 field is truncated");
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

[[nodiscard]] bool is_known_mode(M4Mode mode) noexcept
{
    return mode == M4Mode::SingleTick || mode == M4Mode::RepeatedTicks || mode == M4Mode::WindowScore;
}

[[nodiscard]] std::size_t checked_sequence_bytes(std::uint32_t rows)
{
    require(rows <= kM4MaxWindowWidth, "M4 input row count exceeds the device arena");
    return static_cast<std::size_t>(rows) * kM4InputTrits;
}

} // namespace

void validate_m4_device_layout(const M4DeviceLayout& layout)
{
    require(layout.input_buffer_bytes == kM4InputBufferBytes, "M4 input buffer size is invalid");
    require(layout.output_buffer_bytes == kM4OutputBufferBytes, "M4 output buffer size is invalid");
    require(layout.state_offset == kM4StateOffset, "M4 state offset is invalid");
    require(layout.lut_offset == kM4LutOffset, "M4 LUT offset is invalid");
    require(layout.neighbors_offset == kM4NeighborsOffset, "M4 neighbor offset is invalid");
    require(layout.updated_offset == kM4UpdatedOffset, "M4 updated-neuron offset is invalid");
    require(layout.input_roles_offset == kM4InputRolesOffset, "M4 input-role offset is invalid");
    require(layout.input_sequence_offset == kM4InputSequenceOffset, "M4 input-sequence offset is invalid");
    require(layout.targets_offset == kM4TargetsOffset, "M4 target offset is invalid");
    require(kM4ControlOffset + kM4ControlWords * sizeof(std::uint32_t) <= layout.state_offset,
            "M4 control arena overlaps the state arena");
    require(layout.state_offset + kM4StateStrideBytes <= layout.lut_offset,
            "M4 state arena overlaps the LUT arena");
    require(layout.lut_offset + kM4LutBytes <= layout.neighbors_offset,
            "M4 LUT arena overlaps the neighbor arena");
    require(layout.neighbors_offset + kM4NeighborsCount * sizeof(std::uint32_t) <= layout.updated_offset,
            "M4 neighbor arena overlaps the updated-neuron arena");
    require(layout.updated_offset + kM4UpdatedDeviceCount * sizeof(std::uint32_t) <= layout.input_roles_offset,
            "M4 updated-neuron arena overlaps the input-role arena");
    require(layout.input_roles_offset + kM4InputRoleCount * sizeof(std::uint32_t) <= layout.input_sequence_offset,
            "M4 input-role arena overlaps the input-sequence arena");
    require(layout.input_sequence_offset + kM4MaxInputSequenceBytes <= layout.targets_offset,
            "M4 input-sequence arena overlaps the target arena");
    require(layout.targets_offset + kM4MaxTargetBytes <= layout.input_buffer_bytes,
            "M4 target arena exceeds the input buffer");
}

void validate_m4_logical_input(const M4LogicalInput& input, const M4DeviceLayout& layout)
{
    validate_m4_device_layout(layout);
    require(is_known_mode(input.mode), "M4 mode is invalid");
    require(input.initial_state.size() == kM4StateLogicalBytes, "M4 initial state must contain 64 trits");
    require(input.lut.size() == kM4LutBytes, "M4 LUT must contain 46 rows with a 32-byte stride");
    require(input.neighbors.size() == kM4NeighborsCount, "M4 topology must contain 64*3 neighbors");
    require(input.updated_neurons.size() == kM4UpdatedNeurons, "M4 topology must contain 46 updated neurons");
    require(input.input_neurons.size() == kM4InputRoleCount, "M4 topology must contain 18 input neurons");
    require(input.output_neuron < kM4Population && input.signal_neuron < kM4Population,
            "M4 output and signal roles must be in range");
    require(input.output_neuron != input.signal_neuron, "M4 output and signal roles must be distinct");

    std::array<bool, kM4Population> updated{};
    std::uint32_t previous_updated = 0U;
    for (std::size_t row = 0U; row < input.updated_neurons.size(); ++row) {
        const std::uint32_t neuron = input.updated_neurons[row];
        require(neuron < kM4Population, "M4 updated-neuron index is out of range");
        if (row != 0U) {
            require(neuron > previous_updated, "M4 updated-neuron rows must be strictly ascending");
        }
        require(!updated[neuron], "M4 updated-neuron rows must be unique");
        updated[neuron] = true;
        previous_updated = neuron;
    }

    std::array<bool, kM4Population> input_role{};
    for (const std::uint32_t neuron : input.input_neurons) {
        require(neuron < kM4Population, "M4 input-neuron index is out of range");
        require(!input_role[neuron], "M4 input-neuron roles must be unique");
        require(!updated[neuron], "M4 input and updated roles must be disjoint");
        input_role[neuron] = true;
    }
    for (std::size_t neuron = 0U; neuron < kM4Population; ++neuron) {
        require(updated[neuron] != input_role[neuron], "M4 input and updated roles must be an exact partition");
    }
    require(updated[input.output_neuron] && updated[input.signal_neuron],
            "M4 output and signal roles must be recurrent roles");

    for (const bpp9000::Byte value : input.initial_state) {
        require(bpp9000::is_valid_trit_byte(value), "M4 initial state contains an invalid trit");
    }
    for (std::size_t row = 0U; row < kM4UpdatedNeurons; ++row) {
        for (std::size_t entry = 0U; entry < kM4LutEntries; ++entry) {
            require(bpp9000::is_valid_trit_byte(input.lut[row * kM4LutRowStride + entry]),
                    "M4 LUT contains an invalid logical trit");
        }
    }
    for (const std::uint32_t neighbor : input.neighbors) {
        require(neighbor < kM4Population, "M4 neighbor index is out of range");
    }

    switch (input.mode) {
    case M4Mode::SingleTick:
        require(input.tick_count == 1U && input.window_width == 0U && input.max_ticks == 0U,
                "M4 single-tick control fields are invalid");
        require(input.input_sequence.empty() && input.targets.empty(),
                "M4 single-tick mode must not carry a feed sequence");
        break;
    case M4Mode::RepeatedTicks:
        require(input.tick_count >= 1U && input.tick_count <= kM4MaxWindowWidth,
                "M4 repeated-tick count is out of range");
        require(input.window_width == 0U && input.max_ticks == 0U,
                "M4 repeated-tick window controls are invalid");
        require(input.input_sequence.size() == checked_sequence_bytes(input.tick_count),
                "M4 repeated-tick input sequence length is invalid");
        require(input.targets.empty(), "M4 repeated-tick mode must not carry targets");
        break;
    case M4Mode::WindowScore:
        require(input.tick_count == 0U, "M4 window mode tick-count field must be zero");
        require(input.window_width >= 2U && input.window_width <= kM4MaxWindowWidth,
                "M4 window width is out of range");
        require(input.max_ticks > input.window_width && input.max_ticks <= bpp9000::kProductionMaxTicks,
                "M4 maximum ticks must exceed the window width and fit the production limit");
        require(input.input_sequence.size() == checked_sequence_bytes(input.window_width),
                "M4 window input sequence length is invalid");
        require(input.targets.size() == static_cast<std::size_t>(input.window_width) + 1U,
                "M4 window target sequence length is invalid");
        break;
    }
    for (const bpp9000::Byte value : input.input_sequence) {
        require(bpp9000::is_valid_trit_byte(value), "M4 input sequence contains an invalid trit");
    }
    for (const bpp9000::Byte value : input.targets) {
        require(bpp9000::is_valid_trit_byte(value), "M4 target sequence contains an invalid trit");
    }
}

void validate_m4_packed_input(const M4PackedBuffers& buffers, const M4DeviceLayout& layout)
{
    validate_m4_device_layout(layout);
    require(buffers.input.size() == layout.input_buffer_bytes, "M4 packed input size is invalid");
    require(buffers.output.size() == layout.output_buffer_bytes, "M4 packed output size is invalid");
    const std::span<const bpp9000::Byte> input(buffers.input);
    require(read_u32(input, kM4ControlOffset) == kM4InputMagic, "M4 input magic is invalid");
    const std::uint32_t mode = read_u32(input, 4U);
    const std::uint32_t tick_count = read_u32(input, 8U);
    const std::uint32_t window_width = read_u32(input, 12U);
    const std::uint32_t max_ticks = read_u32(input, 16U);
    const std::uint32_t input_rows = read_u32(input, 20U);
    const std::uint32_t output_neuron = read_u32(input, 24U);
    const std::uint32_t signal_neuron = read_u32(input, 28U);
    const std::uint32_t target_count = read_u32(input, 32U);
    require(mode <= static_cast<std::uint32_t>(M4Mode::WindowScore), "M4 packed mode is invalid");
    require(output_neuron < kM4Population && signal_neuron < kM4Population
                && output_neuron != signal_neuron,
            "M4 packed output/signal roles are invalid");
    if (mode == static_cast<std::uint32_t>(M4Mode::SingleTick)) {
        require(tick_count == 1U && window_width == 0U && max_ticks == 0U && input_rows == 0U
                    && target_count == 0U,
                "M4 packed single-tick controls are invalid");
    } else if (mode == static_cast<std::uint32_t>(M4Mode::RepeatedTicks)) {
        require(tick_count >= 1U && tick_count <= kM4MaxWindowWidth && window_width == 0U && max_ticks == 0U
                    && input_rows == tick_count && target_count == 0U,
                "M4 packed repeated-tick controls are invalid");
    } else {
        require(tick_count == 0U && window_width >= 2U && window_width <= kM4MaxWindowWidth
                    && max_ticks > window_width && max_ticks <= bpp9000::kProductionMaxTicks
                    && input_rows == window_width
                    && target_count == window_width + 1U,
                "M4 packed window controls are invalid");
    }
    for (std::size_t index = 0U; index < kM4StateLogicalBytes; ++index) {
        require(bpp9000::is_valid_trit_byte(buffers.input[layout.state_offset + index]),
                "M4 packed initial state contains an invalid trit");
    }
    for (std::size_t row = 0U; row < kM4UpdatedNeurons; ++row) {
        for (std::size_t entry = 0U; entry < kM4LutEntries; ++entry) {
            require(bpp9000::is_valid_trit_byte(buffers.input[layout.lut_offset + row * kM4LutRowStride + entry]),
                    "M4 packed LUT contains an invalid logical trit");
        }
    }
    std::array<bool, kM4Population> updated{};
    std::uint32_t previous_updated = 0U;
    for (std::size_t row = 0U; row < kM4UpdatedNeurons; ++row) {
        const std::uint32_t neuron = read_u32(input, layout.updated_offset + row * sizeof(std::uint32_t));
        require(neuron < kM4Population && !updated[neuron], "M4 packed updated-neuron role is invalid");
        if (row != 0U) {
            require(neuron > previous_updated, "M4 packed updated-neuron rows are not ascending");
        }
        updated[neuron] = true;
        previous_updated = neuron;
    }
    std::array<bool, kM4Population> input_roles{};
    for (std::size_t index = 0U; index < kM4InputRoleCount; ++index) {
        const std::uint32_t neuron = read_u32(input, layout.input_roles_offset + index * sizeof(std::uint32_t));
        require(neuron < kM4Population && !input_roles[neuron] && !updated[neuron],
                "M4 packed input role is invalid");
        input_roles[neuron] = true;
    }
    for (std::size_t neuron = 0U; neuron < kM4Population; ++neuron) {
        require(updated[neuron] != input_roles[neuron], "M4 packed roles do not form a partition");
    }
    require(updated[output_neuron] && updated[signal_neuron], "M4 packed output/signal role is not recurrent");
    for (std::size_t index = 0U; index < kM4NeighborsCount; ++index) {
        const std::uint32_t neighbor = read_u32(input, layout.neighbors_offset + index * sizeof(std::uint32_t));
        require(neighbor < kM4Population, "M4 packed neighbor index is invalid");
    }
    const std::size_t sequence_bytes = static_cast<std::size_t>(input_rows) * kM4InputTrits;
    for (std::size_t index = 0U; index < sequence_bytes; ++index) {
        require(bpp9000::is_valid_trit_byte(buffers.input[layout.input_sequence_offset + index]),
                "M4 packed input sequence contains an invalid trit");
    }
    for (std::size_t index = 0U; index < target_count; ++index) {
        require(bpp9000::is_valid_trit_byte(buffers.input[layout.targets_offset + index]),
                "M4 packed target sequence contains an invalid trit");
    }
}

void validate_m4_output(std::span<const bpp9000::Byte> output, const M4DeviceLayout& layout)
{
    validate_m4_device_layout(layout);
    require(output.size() == layout.output_buffer_bytes, "M4 output size is invalid");
    require(read_u32(output, kM4OutputMagicOffset) == kM4OutputMagic, "M4 output magic is invalid");
    const std::uint32_t status = read_u32(output, kM4OutputStatusOffset);
    require(status <= static_cast<std::uint32_t>(M4DeviceStatus::Timeout), "M4 output status is invalid");
    for (std::size_t index = 0U; index < kM4StateLogicalBytes; ++index) {
        require(bpp9000::is_valid_trit_byte(output[kM4OutputStateOffset + index]),
                "M4 output state contains an invalid trit");
    }
    const std::uint32_t score = read_u32(output, kM4OutputScoreOffset);
    const std::uint32_t ticks = read_u32(output, kM4OutputTicksOffset);
    const std::uint32_t feed_count = read_u32(output, kM4OutputFeedCountOffset);
    const std::uint32_t predicted = read_u32(output, kM4OutputPredictedOffset);
    const std::uint32_t expected = read_u32(output, kM4OutputExpectedOffset);
    require(ticks <= 100000U, "M4 output tick count is outside the device contract");
    require(feed_count <= kM4MaxWindowWidth, "M4 output feed count is outside the device contract");
    require(predicted <= static_cast<std::uint32_t>(bpp9000::Trit::Unknown)
                && expected <= static_cast<std::uint32_t>(bpp9000::Trit::Unknown),
            "M4 output trit field is invalid");
    if (status == static_cast<std::uint32_t>(M4DeviceStatus::Timeout)) {
        require(score == bpp9000::kTimeoutScore, "M4 timeout output does not carry the timeout sentinel");
    } else {
        require(score <= 1U, "M4 finite window score is outside the binary error domain");
    }
}

M4PackedBuffers pack_m4(const M4LogicalInput& input, const M4DeviceLayout& layout)
{
    validate_m4_logical_input(input, layout);
    M4PackedBuffers buffers;
    buffers.input.assign(layout.input_buffer_bytes, 0xA5U);
    buffers.output.assign(layout.output_buffer_bytes, 0x5AU);

    const auto control = std::span<bpp9000::Byte>(buffers.input).subspan(kM4ControlOffset);
    write_u32(control, 0U, kM4InputMagic);
    write_u32(control, 4U, static_cast<std::uint32_t>(input.mode));
    write_u32(control, 8U, input.tick_count);
    write_u32(control, 12U, input.window_width);
    write_u32(control, 16U, input.max_ticks);
    const std::uint32_t input_rows = input.mode == M4Mode::RepeatedTicks ? input.tick_count : input.window_width;
    write_u32(control, 20U, input_rows);
    write_u32(control, 24U, input.output_neuron);
    write_u32(control, 28U, input.signal_neuron);
    write_u32(control, 32U, static_cast<std::uint32_t>(input.targets.size()));

    std::copy(input.initial_state.begin(), input.initial_state.end(), buffers.input.begin() + layout.state_offset);
    std::copy(input.lut.begin(), input.lut.end(), buffers.input.begin() + layout.lut_offset);
    std::memcpy(buffers.input.data() + layout.neighbors_offset,
                input.neighbors.data(),
                input.neighbors.size() * sizeof(std::uint32_t));
    std::memcpy(buffers.input.data() + layout.updated_offset,
                input.updated_neurons.data(),
                input.updated_neurons.size() * sizeof(std::uint32_t));
    std::memcpy(buffers.input.data() + layout.input_roles_offset,
                input.input_neurons.data(),
                input.input_neurons.size() * sizeof(std::uint32_t));
    std::copy(input.input_sequence.begin(),
              input.input_sequence.end(),
              buffers.input.begin() + layout.input_sequence_offset);
    std::copy(input.targets.begin(), input.targets.end(), buffers.input.begin() + layout.targets_offset);
    return buffers;
}

M4DeviceResult unpack_m4_result(std::span<const bpp9000::Byte> output, const M4DeviceLayout& layout)
{
    validate_m4_output(output, layout);
    M4DeviceResult result;
    result.state.assign(output.begin(), output.begin() + kM4StateLogicalBytes);
    result.score = read_u32(output, kM4OutputScoreOffset);
    result.status = static_cast<M4DeviceStatus>(read_u32(output, kM4OutputStatusOffset));
    result.ticks = read_u32(output, kM4OutputTicksOffset);
    result.feed_count = read_u32(output, kM4OutputFeedCountOffset);
    result.predicted = static_cast<bpp9000::Trit>(read_u32(output, kM4OutputPredictedOffset));
    result.expected = static_cast<bpp9000::Trit>(read_u32(output, kM4OutputExpectedOffset));
    return result;
}

} // namespace xdna::runtime
