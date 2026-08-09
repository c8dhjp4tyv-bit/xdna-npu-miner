#include <stdint.h>

namespace {

constexpr uint32_t kPopulation = 64U;
constexpr uint32_t kInputTrits = 18U;
constexpr uint32_t kUpdatedRows = 46U;
constexpr uint32_t kLutStride = 32U;
constexpr uint32_t kUnknown = 2U;
constexpr uint32_t kTimeoutScore = 0xFFFFFFFFU;

constexpr uint32_t kModeSingleTick = 0U;
constexpr uint32_t kModeRepeatedTicks = 1U;
constexpr uint32_t kModeWindowScore = 2U;
constexpr uint32_t kStatusSettled = 0U;
constexpr uint32_t kStatusTimeout = 1U;

constexpr uint32_t kStateOffset = 64U;
constexpr uint32_t kLutOffset = 160U;
constexpr uint32_t kNeighborsOffset = 1632U;
constexpr uint32_t kUpdatedOffset = 2400U;
constexpr uint32_t kInputRolesOffset = 2592U;
constexpr uint32_t kInputSequenceOffset = 2688U;
constexpr uint32_t kTargetsOffset = 14784U;

constexpr uint32_t kOutputScoreOffset = 96U;
constexpr uint32_t kOutputStatusOffset = 100U;
constexpr uint32_t kOutputTicksOffset = 104U;
constexpr uint32_t kOutputFeedCountOffset = 108U;
constexpr uint32_t kOutputPredictedOffset = 112U;
constexpr uint32_t kOutputExpectedOffset = 116U;
constexpr uint32_t kOutputMagicOffset = 120U;
constexpr uint32_t kOutputMagic = 0x3152344DU;

void write_u32(uint8_t* output, uint32_t offset, uint32_t value)
{
    *reinterpret_cast<uint32_t*>(output + offset) = value;
}

void tick_once(const uint8_t* previous,
               uint8_t* next,
               const uint8_t* lut,
               const uint32_t* neighbors,
               const uint32_t* updated_neurons)
{
    for (uint32_t neuron = 0U; neuron < kPopulation; ++neuron) {
        next[neuron] = previous[neuron];
    }
    for (uint32_t row = 0U; row < kUpdatedRows; ++row) {
        const uint32_t neuron = updated_neurons[row];
        const uint32_t neighbor_base = neuron * 3U;
        const uint32_t first = previous[neighbors[neighbor_base]];
        const uint32_t second = previous[neighbors[neighbor_base + 1U]];
        const uint32_t third = previous[neighbors[neighbor_base + 2U]];
        const uint32_t index = first + 3U * second + 9U * third;
        next[neuron] = lut[row * kLutStride + index];
    }
}

void copy_state(const uint8_t* source, uint8_t* destination)
{
    for (uint32_t neuron = 0U; neuron < kPopulation; ++neuron) {
        destination[neuron] = source[neuron];
    }
}

void write_result(uint8_t* output,
                  const uint8_t* state,
                  uint32_t score,
                  uint32_t status,
                  uint32_t ticks,
                  uint32_t feed_count,
                  uint32_t predicted,
                  uint32_t expected)
{
    for (uint32_t neuron = 0U; neuron < kPopulation; ++neuron) {
        output[neuron] = state[neuron];
    }
    for (uint32_t index = kPopulation; index < 96U; ++index) {
        output[index] = 0U;
    }
    write_u32(output, kOutputScoreOffset, score);
    write_u32(output, kOutputStatusOffset, status);
    write_u32(output, kOutputTicksOffset, ticks);
    write_u32(output, kOutputFeedCountOffset, feed_count);
    write_u32(output, kOutputPredictedOffset, predicted);
    write_u32(output, kOutputExpectedOffset, expected);
    write_u32(output, kOutputMagicOffset, kOutputMagic);
    write_u32(output, 124U, 0U);
}

} // namespace

extern "C" void bpp9000_m4_dispatch(const uint8_t* device_input, uint8_t* output)
{
    const uint32_t* control = reinterpret_cast<const uint32_t*>(device_input);
    const uint32_t mode = control[1U];
    const uint32_t tick_count = control[2U];
    const uint32_t window_width = control[3U];
    const uint32_t max_ticks = control[4U];
    const uint32_t input_rows = control[5U];
    const uint32_t output_neuron = control[6U];
    const uint32_t signal_neuron = control[7U];

    const uint8_t* initial_state = device_input + kStateOffset;
    const uint8_t* lut = device_input + kLutOffset;
    const uint32_t* neighbors = reinterpret_cast<const uint32_t*>(device_input + kNeighborsOffset);
    const uint32_t* updated_neurons = reinterpret_cast<const uint32_t*>(device_input + kUpdatedOffset);
    const uint32_t* input_roles = reinterpret_cast<const uint32_t*>(device_input + kInputRolesOffset);
    const uint8_t* input_sequence = device_input + kInputSequenceOffset;
    const uint8_t* targets = device_input + kTargetsOffset;

    uint8_t current[kPopulation];
    uint8_t next[kPopulation];
    for (uint32_t neuron = 0U; neuron < kPopulation; ++neuron) {
        current[neuron] = initial_state[neuron];
    }

    uint32_t status = kStatusSettled;
    uint32_t score = 0U;
    uint32_t ticks = 0U;
    uint32_t feed_count = 0U;
    uint32_t predicted = kUnknown;
    uint32_t expected = kUnknown;

    if (mode == kModeSingleTick) {
        tick_once(current, next, lut, neighbors, updated_neurons);
        copy_state(next, current);
        ticks = 1U;
    } else if (mode == kModeRepeatedTicks) {
        for (uint32_t tick = 0U; tick < tick_count; ++tick) {
            if (tick < input_rows) {
                const uint8_t* row = input_sequence + tick * kInputTrits;
                for (uint32_t input = 0U; input < kInputTrits; ++input) {
                    current[input_roles[input]] = row[input];
                }
                ++feed_count;
            } else {
                for (uint32_t input = 0U; input < kInputTrits; ++input) {
                    current[input_roles[input]] = kUnknown;
                }
            }
            tick_once(current, next, lut, neighbors, updated_neurons);
            copy_state(next, current);
            ++ticks;
        }
    } else if (mode == kModeWindowScore) {
        for (; ticks < max_ticks; ++ticks) {
            if (current[signal_neuron] == kUnknown) {
                if (feed_count >= window_width) {
                    status = kStatusSettled;
                    break;
                }
                const uint8_t* row = input_sequence + feed_count * kInputTrits;
                for (uint32_t input = 0U; input < kInputTrits; ++input) {
                    current[input_roles[input]] = row[input];
                }
                ++feed_count;
            } else {
                for (uint32_t input = 0U; input < kInputTrits; ++input) {
                    current[input_roles[input]] = kUnknown;
                }
            }
            tick_once(current, next, lut, neighbors, updated_neurons);
            copy_state(next, current);
        }
        if (ticks >= max_ticks && feed_count < window_width) {
            status = kStatusTimeout;
            score = kTimeoutScore;
            predicted = kUnknown;
            expected = kUnknown;
        } else {
            predicted = current[output_neuron];
            expected = targets[feed_count];
            score = predicted == expected ? 0U : 1U;
        }
    } else {
        status = kStatusTimeout;
        score = kTimeoutScore;
    }

    write_result(output, current, score, status, ticks, feed_count, predicted, expected);
}
