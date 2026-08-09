#include "xdna/m5.hpp"

#include <array>
#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

namespace xdna::runtime {
namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        throw M5ContractError(message);
    }
}

[[nodiscard]] std::uint32_t read_u32(std::span<const bpp9000::Byte> bytes, std::size_t offset)
{
    require(offset <= bytes.size() && bytes.size() - offset >= sizeof(std::uint32_t),
            "M5 uint32 field is truncated");
    std::uint32_t value = 0U;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

[[nodiscard]] bool checked_product(std::size_t left, std::size_t right, std::size_t& result) noexcept
{
    if (right != 0U && left > std::numeric_limits<std::size_t>::max() / right) {
        return false;
    }
    result = left * right;
    return true;
}

void validate_item_descriptor(const M5ItemDescriptor& descriptor,
                             std::size_t item_index,
                             const M5DeviceLayout& layout)
{
    const std::size_t input_offset = item_index * layout.input_item_stride_bytes;
    const std::size_t output_offset = item_index * layout.output_item_stride_bytes;
    require(descriptor.item_index == item_index, "M5 item ordering metadata is not contiguous");
    require(descriptor.input_offset == input_offset, "M5 input offset does not match item stride");
    require(descriptor.output_offset == output_offset, "M5 output offset does not match item stride");
    require(descriptor.state_offset == input_offset + kM4StateOffset,
            "M5 state offset does not match the M4 item layout");
    require(descriptor.lut_offset == input_offset + kM4LutOffset,
            "M5 LUT offset does not match the M4 item layout");
    require(descriptor.topology_offset == input_offset + kM4NeighborsOffset,
            "M5 topology offset does not match the M4 item layout");
    require(descriptor.input_sequence_offset == input_offset + kM4InputSequenceOffset,
            "M5 input-sequence offset does not match the M4 item layout");
    require(descriptor.target_offset == input_offset + kM4TargetsOffset,
            "M5 target offset does not match the M4 item layout");
    require(descriptor.score_offset == output_offset + kM4OutputScoreOffset,
            "M5 score offset does not match the output stride");
    require(descriptor.status_offset == output_offset + kM4OutputStatusOffset,
            "M5 status offset does not match the output stride");
    require(descriptor.error_offset == output_offset + kM5OutputErrorOffset,
            "M5 error offset does not match the output stride");
}

void validate_m5_item_input(std::span<const bpp9000::Byte> input)
{
    require(input.size() == kM5InputItemStrideBytes, "M5 item input size is invalid");
    require(read_u32(input, kM4ControlOffset) == kM4InputMagic, "M5 item input magic is invalid");

    const std::uint32_t mode = read_u32(input, 4U);
    const std::uint32_t tick_count = read_u32(input, 8U);
    const std::uint32_t window_width = read_u32(input, 12U);
    const std::uint32_t max_ticks = read_u32(input, 16U);
    const std::uint32_t input_rows = read_u32(input, 20U);
    const std::uint32_t output_neuron = read_u32(input, 24U);
    const std::uint32_t signal_neuron = read_u32(input, 28U);
    const std::uint32_t target_count = read_u32(input, 32U);
    require(mode == static_cast<std::uint32_t>(M4Mode::WindowScore),
            "M5 batch items must use the M4 window-score mode");
    require(tick_count == 0U && window_width >= 2U && window_width <= kM4MaxWindowWidth
                && max_ticks > window_width && max_ticks <= bpp9000::kProductionMaxTicks
                && input_rows == window_width && target_count == window_width + 1U,
            "M5 item window controls are invalid");
    require(output_neuron < kM4Population && signal_neuron < kM4Population
                && output_neuron != signal_neuron,
            "M5 item output/signal roles are invalid");

    for (std::size_t index = 0U; index < kM4StateLogicalBytes; ++index) {
        require(bpp9000::is_valid_trit_byte(input[kM4StateOffset + index]),
                "M5 item initial state contains an invalid trit");
    }
    for (std::size_t row = 0U; row < kM4UpdatedNeurons; ++row) {
        for (std::size_t entry = 0U; entry < kM4LutEntries; ++entry) {
            require(bpp9000::is_valid_trit_byte(input[kM4LutOffset + row * kM4LutRowStride + entry]),
                    "M5 item LUT contains an invalid logical trit");
        }
    }

    std::array<bool, kM4Population> updated{};
    std::uint32_t previous_updated = 0U;
    for (std::size_t row = 0U; row < kM4UpdatedNeurons; ++row) {
        const std::uint32_t neuron = read_u32(input, kM4UpdatedOffset + row * sizeof(std::uint32_t));
        require(neuron < kM4Population && !updated[neuron], "M5 item updated-neuron role is invalid");
        if (row != 0U) {
            require(neuron > previous_updated, "M5 item updated-neuron rows are not ascending");
        }
        updated[neuron] = true;
        previous_updated = neuron;
    }
    std::array<bool, kM4Population> input_roles{};
    for (std::size_t index = 0U; index < kM4InputRoleCount; ++index) {
        const std::uint32_t neuron = read_u32(input, kM4InputRolesOffset + index * sizeof(std::uint32_t));
        require(neuron < kM4Population && !input_roles[neuron] && !updated[neuron],
                "M5 item input role is invalid");
        input_roles[neuron] = true;
    }
    for (std::size_t neuron = 0U; neuron < kM4Population; ++neuron) {
        require(updated[neuron] != input_roles[neuron], "M5 item roles are not an exact partition");
    }
    require(updated[output_neuron] && updated[signal_neuron], "M5 item result roles are not recurrent");

    for (std::size_t index = 0U; index < kM4NeighborsCount; ++index) {
        const std::uint32_t neighbor = read_u32(input, kM4NeighborsOffset + index * sizeof(std::uint32_t));
        require(neighbor < kM4Population, "M5 item neighbor index is invalid");
    }
    const std::size_t sequence_bytes = static_cast<std::size_t>(input_rows) * kM4InputTrits;
    for (std::size_t index = 0U; index < sequence_bytes; ++index) {
        require(bpp9000::is_valid_trit_byte(input[kM4InputSequenceOffset + index]),
                "M5 item input sequence contains an invalid trit");
    }
    for (std::size_t index = 0U; index < target_count; ++index) {
        require(bpp9000::is_valid_trit_byte(input[kM4TargetsOffset + index]),
                "M5 item target sequence contains an invalid trit");
    }
}

} // namespace

void validate_m5_device_layout(const M5DeviceLayout& layout)
{
    require(layout.batch_size >= 1U && layout.batch_size <= kM5MaximumBatchSize,
            "M5 batch size is outside the supported contract");
    require(layout.input_item_stride_bytes == kM5InputItemStrideBytes,
            "M5 input item stride is invalid");
    require(layout.output_item_stride_bytes == kM5OutputItemStrideBytes,
            "M5 output item stride is invalid");
    std::size_t expected_input_bytes = 0U;
    std::size_t expected_output_bytes = 0U;
    require(checked_product(layout.batch_size, layout.input_item_stride_bytes, expected_input_bytes),
            "M5 input buffer size overflows size_t");
    require(checked_product(layout.batch_size, layout.output_item_stride_bytes, expected_output_bytes),
            "M5 output buffer size overflows size_t");
    require(layout.input_buffer_bytes == expected_input_bytes, "M5 input buffer size is invalid");
    require(layout.output_buffer_bytes == expected_output_bytes, "M5 output buffer size is invalid");
}

void validate_m5_packed_input(const M5PackedBatch& batch, const M5DeviceLayout& layout)
{
    validate_m5_device_layout(layout);
    require(batch.schema.batch_size == layout.batch_size, "M5 schema batch size is invalid");
    require(batch.schema.input_item_stride_bytes == layout.input_item_stride_bytes,
            "M5 schema input stride is invalid");
    require(batch.schema.output_item_stride_bytes == layout.output_item_stride_bytes,
            "M5 schema output stride is invalid");
    require(batch.schema.items.size() == layout.batch_size, "M5 schema item count is invalid");
    require(batch.input.size() == layout.input_buffer_bytes, "M5 packed input size is invalid");
    require(batch.output.size() == layout.output_buffer_bytes, "M5 packed output size is invalid");

    for (std::size_t index = 0U; index < layout.batch_size; ++index) {
        validate_item_descriptor(batch.schema.items[index], index, layout);
        const std::size_t input_offset = index * layout.input_item_stride_bytes;
        validate_m5_item_input(std::span<const bpp9000::Byte>(batch.input).subspan(
            input_offset,
            layout.input_item_stride_bytes));
    }
}

void validate_m5_output(std::span<const bpp9000::Byte> output)
{
    require(output.size() == kM5OutputItemStrideBytes, "M5 output item size is invalid");
    require(read_u32(output, kM4OutputMagicOffset) == kM5OutputMagic, "M5 output magic is invalid");
    const std::uint32_t status = read_u32(output, kM4OutputStatusOffset);
    require(status <= static_cast<std::uint32_t>(M4DeviceStatus::Timeout), "M5 output status is invalid");
    for (std::size_t index = 0U; index < kM4StateLogicalBytes; ++index) {
        require(bpp9000::is_valid_trit_byte(output[kM4OutputStateOffset + index]),
                "M5 output state contains an invalid trit");
    }
    const std::uint32_t score = read_u32(output, kM4OutputScoreOffset);
    const std::uint32_t ticks = read_u32(output, kM4OutputTicksOffset);
    const std::uint32_t feed_count = read_u32(output, kM4OutputFeedCountOffset);
    const std::uint32_t predicted = read_u32(output, kM4OutputPredictedOffset);
    const std::uint32_t expected = read_u32(output, kM4OutputExpectedOffset);
    const std::uint32_t error = read_u32(output, kM5OutputErrorOffset);
    require(ticks <= bpp9000::kProductionMaxTicks, "M5 output tick count is invalid");
    require(feed_count <= kM4MaxWindowWidth, "M5 output feed count is invalid");
    require(predicted <= static_cast<std::uint32_t>(bpp9000::Trit::Unknown)
                && expected <= static_cast<std::uint32_t>(bpp9000::Trit::Unknown),
            "M5 output trit field is invalid");
    if (status == static_cast<std::uint32_t>(M4DeviceStatus::Timeout)) {
        require(score == bpp9000::kTimeoutScore, "M5 timeout output lacks the timeout sentinel");
    } else {
        require(score <= 1U, "M5 finite window score is outside the binary error domain");
        require(error == static_cast<std::uint32_t>(M5ItemError::None),
                "M5 finite output carries an item error");
    }
}

M5PackedBatch pack_m5(std::span<const M5WorkItem> items, const M5DeviceLayout& layout)
{
    validate_m5_device_layout(layout);
    require(items.size() == layout.batch_size, "M5 item count must equal the fixed artifact batch size");

    M5PackedBatch batch;
    batch.schema.batch_size = layout.batch_size;
    batch.schema.input_item_stride_bytes = layout.input_item_stride_bytes;
    batch.schema.output_item_stride_bytes = layout.output_item_stride_bytes;
    batch.schema.items.reserve(layout.batch_size);
    batch.input.assign(layout.input_buffer_bytes, 0xA5U);
    batch.output.assign(layout.output_buffer_bytes, 0x5AU);

    for (std::size_t index = 0U; index < layout.batch_size; ++index) {
        M4PackedBuffers item;
        try {
            item = pack_m4(items[index].input);
        } catch (const M4ContractError& error) {
            throw M5ContractError(std::string("M5 item reuses an invalid M4 input: ") + error.what());
        }
        const std::size_t input_offset = index * layout.input_item_stride_bytes;
        const std::size_t output_offset = index * layout.output_item_stride_bytes;
        std::copy(item.input.begin(),
                  item.input.end(),
                  batch.input.begin() + static_cast<std::ptrdiff_t>(input_offset));
        const M5ItemDescriptor descriptor{
            index,
            items[index].candidate_index,
            items[index].window_index,
            input_offset,
            output_offset,
            input_offset + kM4StateOffset,
            input_offset + kM4LutOffset,
            input_offset + kM4NeighborsOffset,
            input_offset + kM4InputSequenceOffset,
            input_offset + kM4TargetsOffset,
            output_offset + kM4OutputScoreOffset,
            output_offset + kM4OutputStatusOffset,
            output_offset + kM5OutputErrorOffset,
        };
        batch.schema.items.push_back(descriptor);
    }
    validate_m5_packed_input(batch, layout);
    return batch;
}

std::vector<M5ItemResult> unpack_m5_results(const M5PackedBatch& batch, const M5DeviceLayout& layout)
{
    validate_m5_packed_input(batch, layout);

    std::vector<M5ItemResult> results;
    results.reserve(layout.batch_size);
    for (std::size_t index = 0U; index < layout.batch_size; ++index) {
        const std::size_t output_offset = index * layout.output_item_stride_bytes;
        const std::span<const bpp9000::Byte> output = std::span<const bpp9000::Byte>(batch.output).subspan(
            output_offset,
            layout.output_item_stride_bytes);
        validate_m5_output(output);
        M5ItemResult item;
        item.descriptor = batch.schema.items[index];
        item.device.state.assign(output.begin(), output.begin() + kM4StateLogicalBytes);
        item.device.score = read_u32(output, kM4OutputScoreOffset);
        item.device.status = static_cast<M4DeviceStatus>(read_u32(output, kM4OutputStatusOffset));
        item.device.ticks = read_u32(output, kM4OutputTicksOffset);
        item.device.feed_count = read_u32(output, kM4OutputFeedCountOffset);
        item.device.predicted = static_cast<bpp9000::Trit>(read_u32(output, kM4OutputPredictedOffset));
        item.device.expected = static_cast<bpp9000::Trit>(read_u32(output, kM4OutputExpectedOffset));
        item.error = static_cast<M5ItemError>(read_u32(output, kM5OutputErrorOffset));
        results.push_back(std::move(item));
    }
    return results;
}

} // namespace xdna::runtime
