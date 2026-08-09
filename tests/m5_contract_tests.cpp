#include "m4_vectors.hpp"
#include "xdna/m5.hpp"

#include <array>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using xdna::bpp9000::Byte;
using xdna::bpp9000::Trit;
using xdna::runtime::M4LogicalInput;
using xdna::runtime::M4Mode;
using xdna::runtime::M5ContractError;
using xdna::runtime::M5DeviceLayout;
using xdna::runtime::M5PackedBatch;
using xdna::runtime::M5WorkItem;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void expect_failure(const std::function<void()>& function, const std::string& message)
{
    try {
        function();
    } catch (const M5ContractError&) {
        return;
    }
    throw std::runtime_error(message);
}

void write_u32(std::vector<Byte>& bytes, std::size_t offset, std::uint32_t value)
{
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

struct OwnedInput {
    std::array<Byte, 64U> state{};
    std::vector<Byte> sequence;
    std::vector<Byte> targets;
    M4LogicalInput logical;

    explicit OwnedInput(const m4_test::M4Case& fixture)
        : state(),
          sequence(2U * xdna::runtime::kM4InputTrits),
          targets(3U),
          logical()
    {
        state.fill(2U);
        for (std::size_t index = 0U; index < sequence.size(); ++index) {
            sequence[index] = xdna::bpp9000::trit_to_byte(fixture.task.inputs[index]);
        }
        for (std::size_t index = 0U; index < targets.size(); ++index) {
            targets[index] = xdna::bpp9000::trit_to_byte(fixture.task.outputs[index]);
        }
        logical = M4LogicalInput{
            M4Mode::WindowScore,
            0U,
            2U,
            8U,
            fixture.task.topology.output_neurons[0U],
            fixture.task.topology.signal_neuron,
            state,
            fixture.lut.storage(),
            fixture.task.topology.neighbors,
            fixture.lut.updated_neurons(),
            fixture.task.topology.input_neurons,
            sequence,
            targets,
        };
    }
};

void test_schema_and_ordering()
{
    const m4_test::M4Case fixture = m4_test::make_case(0xBA7CU, 0U);
    OwnedInput owned(fixture);
    const M5DeviceLayout layout = xdna::runtime::m5_default_layout(4U);
    const std::array<M5WorkItem, 4U> items{
        M5WorkItem{7U, 100U, owned.logical},
        M5WorkItem{3U, 4U, owned.logical},
        M5WorkItem{7U, 101U, owned.logical},
        M5WorkItem{9U, 2U, owned.logical},
    };
    M5PackedBatch batch = xdna::runtime::pack_m5(items, layout);
    expect(batch.schema.batch_size == 4U, "M5 schema records the batch size");
    expect(batch.schema.input_item_stride_bytes == xdna::runtime::kM5InputItemStrideBytes,
           "M5 schema records the input stride");
    expect(batch.schema.output_item_stride_bytes == xdna::runtime::kM5OutputItemStrideBytes,
           "M5 schema records the output stride");
    expect(batch.schema.items.size() == 4U, "M5 schema preserves every item");
    for (std::size_t index = 0U; index < batch.schema.items.size(); ++index) {
        const auto& descriptor = batch.schema.items[index];
        expect(descriptor.item_index == index, "M5 item index is stable");
        expect(descriptor.input_offset == index * xdna::runtime::kM5InputItemStrideBytes,
               "M5 input offset follows the item stride");
        expect(descriptor.output_offset == index * xdna::runtime::kM5OutputItemStrideBytes,
               "M5 output offset follows the item stride");
        expect(descriptor.state_offset == descriptor.input_offset + xdna::runtime::kM4StateOffset,
               "M5 state offset is explicit");
        expect(descriptor.lut_offset == descriptor.input_offset + xdna::runtime::kM4LutOffset,
               "M5 LUT offset is explicit");
        expect(descriptor.topology_offset == descriptor.input_offset + xdna::runtime::kM4NeighborsOffset,
               "M5 topology offset is explicit");
        expect(descriptor.input_sequence_offset
                   == descriptor.input_offset + xdna::runtime::kM4InputSequenceOffset,
               "M5 input offset is explicit");
        expect(descriptor.target_offset == descriptor.input_offset + xdna::runtime::kM4TargetsOffset,
               "M5 target offset is explicit");
        expect(descriptor.score_offset == descriptor.output_offset + xdna::runtime::kM4OutputScoreOffset,
               "M5 score offset is explicit");
    }
    expect(batch.schema.items[0U].candidate_index == 7U && batch.schema.items[1U].candidate_index == 3U
               && batch.schema.items[2U].window_index == 101U,
           "M5 candidate/window metadata preserves input order");
    expect(batch.input.size() == 4U * xdna::runtime::kM5InputItemStrideBytes,
           "M5 input arena has one fixed-stride item per batch entry");
    expect(batch.output.size() == 4U * xdna::runtime::kM4OutputBufferBytes,
           "M5 output arena has one full result per batch entry");
}

void test_result_order_and_stale_output()
{
    const m4_test::M4Case fixture = m4_test::make_case(0xBEEFU, 1U);
    OwnedInput owned(fixture);
    const M5DeviceLayout layout = xdna::runtime::m5_default_layout(4U);
    const std::array<M5WorkItem, 4U> items{
        M5WorkItem{10U, 11U, owned.logical},
        M5WorkItem{20U, 22U, owned.logical},
        M5WorkItem{30U, 33U, owned.logical},
        M5WorkItem{40U, 44U, owned.logical},
    };
    M5PackedBatch batch = xdna::runtime::pack_m5(items, layout);
    for (std::size_t index = 0U; index < layout.batch_size; ++index) {
        const std::size_t offset = index * layout.output_item_stride_bytes;
        std::fill(batch.output.begin() + static_cast<std::ptrdiff_t>(offset),
                  batch.output.begin() + static_cast<std::ptrdiff_t>(offset + layout.output_item_stride_bytes),
                  static_cast<Byte>(0U));
        for (std::size_t state = 0U; state < xdna::runtime::kM4StateLogicalBytes; ++state) {
            batch.output[offset + state] = static_cast<Byte>((state + index) % 3U);
        }
        write_u32(batch.output, offset + xdna::runtime::kM4OutputScoreOffset, static_cast<std::uint32_t>(index % 2U));
        write_u32(batch.output, offset + xdna::runtime::kM4OutputStatusOffset, 0U);
        write_u32(batch.output, offset + xdna::runtime::kM4OutputTicksOffset, 3U + static_cast<std::uint32_t>(index));
        write_u32(batch.output, offset + xdna::runtime::kM4OutputFeedCountOffset, 2U);
        write_u32(batch.output, offset + xdna::runtime::kM4OutputPredictedOffset, static_cast<std::uint32_t>(index % 3U));
        write_u32(batch.output, offset + xdna::runtime::kM4OutputExpectedOffset, 2U);
        write_u32(batch.output, offset + xdna::runtime::kM4OutputMagicOffset, xdna::runtime::kM5OutputMagic);
        write_u32(batch.output, offset + xdna::runtime::kM5OutputErrorOffset, 0U);
    }
    const auto results = xdna::runtime::unpack_m5_results(batch, layout);
    expect(results.size() == 4U, "M5 returns one result per item");
    for (std::size_t index = 0U; index < results.size(); ++index) {
        expect(results[index].descriptor.candidate_index == items[index].candidate_index,
               "M5 result candidate ordering is exact");
        expect(results[index].descriptor.window_index == items[index].window_index,
               "M5 result window ordering is exact");
        expect(results[index].device.ticks == 3U + index, "M5 result fields remain item-local");
    }

    batch.output.assign(batch.output.size(), static_cast<Byte>(0x5AU));
    expect_failure(
        [&] { (void)xdna::runtime::unpack_m5_results(batch, layout); },
        "M5 stale/sentinel output cannot be accepted as a result");
}

void test_fail_closed_batch_shape()
{
    const m4_test::M4Case fixture = m4_test::make_case(0x1234U, 2U);
    OwnedInput owned(fixture);
    const std::array<M5WorkItem, 2U> items{
        M5WorkItem{1U, 1U, owned.logical},
        M5WorkItem{2U, 2U, owned.logical},
    };
    expect_failure(
        [&] { (void)xdna::runtime::pack_m5(items, xdna::runtime::m5_default_layout(4U)); },
        "M5 rejects a batch whose item count differs from the artifact contract");

    M5PackedBatch malformed = xdna::runtime::pack_m5(
        std::span<const M5WorkItem>(items),
        xdna::runtime::m5_default_layout(2U));
    malformed.input[0U] = 0U;
    expect_failure(
        [&] { xdna::runtime::validate_m5_packed_input(malformed, xdna::runtime::m5_default_layout(2U)); },
        "M5 rejects a corrupted item input before dispatch");
}

} // namespace

int main()
{
    try {
        test_schema_and_ordering();
        test_result_order_and_stale_output();
        test_fail_closed_batch_shape();
        std::cout << "PASS m5_contracts\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL m5_contracts: " << error.what() << '\n';
        return 1;
    }
}
