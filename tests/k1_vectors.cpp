#include "k1_vectors.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace k1_test {
namespace {

using xdna::bpp9000::Byte;
using xdna::bpp9000::Task;
using xdna::bpp9000::TaskShape;
using xdna::bpp9000::Trit;
using xdna::runtime::kK1LutEntries;
using xdna::runtime::kK1LutLogicalBytes;
using xdna::runtime::kK1LutRowStride;
using xdna::runtime::kK1NeighborsLogicalCount;
using xdna::runtime::kK1NeighborsPerNeuron;
using xdna::runtime::kK1Population;
using xdna::runtime::kK1StateLogicalBytes;
using xdna::runtime::kK1UpdatedNeurons;

constexpr std::array<std::uint32_t, 18U> kNoncontiguousInputs{
    1U, 4U, 7U, 11U, 14U, 18U, 21U, 25U, 28U,
    32U, 35U, 39U, 42U, 46U, 49U, 53U, 58U, 62U,
};

class DeterministicRng {
public:
    explicit DeterministicRng(std::uint64_t seed)
        : state_(seed)
    {
    }

    [[nodiscard]] std::uint64_t next()
    {
        state_ += 0x9E3779B97F4A7C15ULL;
        std::uint64_t value = state_;
        value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
        return value ^ (value >> 31U);
    }

    [[nodiscard]] Byte trit()
    {
        return static_cast<Byte>(next() % 3U);
    }

private:
    std::uint64_t state_;
};

[[nodiscard]] std::array<std::uint32_t, kK1UpdatedNeurons> complement_updated(
    const std::array<bool, kK1Population>& is_input)
{
    std::array<std::uint32_t, kK1UpdatedNeurons> updated{};
    std::size_t row = 0U;
    for (std::size_t neuron = 0U; neuron < kK1Population; ++neuron) {
        if (!is_input[neuron]) {
            updated[row++] = static_cast<std::uint32_t>(neuron);
        }
    }
    return updated;
}

[[nodiscard]] K1Vector make_base(std::string id,
                                 std::uint64_t seed,
                                 std::uint64_t case_index)
{
    K1Vector vector;
    vector.id = std::move(id);
    vector.generator_seed = seed;
    vector.case_index = case_index;

    std::array<bool, kK1Population> is_input{};
    for (const std::uint32_t neuron : kNoncontiguousInputs) {
        is_input[neuron] = true;
    }
    vector.updated_neurons = complement_updated(is_input);
    for (std::size_t neuron = 0U; neuron < kK1Population; ++neuron) {
        vector.neighbors[neuron * kK1NeighborsPerNeuron] = static_cast<std::uint32_t>((neuron + 1U) % kK1Population);
        vector.neighbors[neuron * kK1NeighborsPerNeuron + 1U]
            = static_cast<std::uint32_t>((neuron + 7U) % kK1Population);
        vector.neighbors[neuron * kK1NeighborsPerNeuron + 2U]
            = static_cast<std::uint32_t>((neuron + 19U) % kK1Population);
    }
    for (std::size_t index = 0U; index < kK1StateLogicalBytes; ++index) {
        vector.state[index] = static_cast<Byte>(index % 3U);
    }
    for (std::size_t row = 0U; row < kK1UpdatedNeurons; ++row) {
        for (std::size_t entry = 0U; entry < kK1LutEntries; ++entry) {
            vector.lut[row * kK1LutRowStride + entry] = static_cast<Byte>((row + 2U * entry) % 3U);
        }
        for (std::size_t entry = kK1LutEntries; entry < kK1LutRowStride; ++entry) {
            vector.lut[row * kK1LutRowStride + entry] = 0xC3U;
        }
    }
    return vector;
}

[[nodiscard]] K1Vector k3_case(std::uint32_t combination)
{
    K1Vector vector = make_base("k3-combination-" + std::to_string(combination),
                                0xB3000000ULL + combination,
                                combination);
    const std::uint32_t target = vector.updated_neurons[0U];
    const std::uint32_t first_input = kNoncontiguousInputs[0U];
    const std::uint32_t second_input = kNoncontiguousInputs[1U];
    const std::uint32_t third_input = kNoncontiguousInputs[2U];
    vector.neighbors[target * kK1NeighborsPerNeuron] = first_input;
    vector.neighbors[target * kK1NeighborsPerNeuron + 1U] = second_input;
    vector.neighbors[target * kK1NeighborsPerNeuron + 2U] = third_input;
    vector.state[first_input] = static_cast<Byte>(combination % 3U);
    vector.state[second_input] = static_cast<Byte>((combination / 3U) % 3U);
    vector.state[third_input] = static_cast<Byte>((combination / 9U) % 3U);
    const std::size_t row = 0U;
    for (std::size_t entry = 0U; entry < kK1LutEntries; ++entry) {
        vector.lut[row * kK1LutRowStride + entry] = 0U;
    }
    vector.lut[row * kK1LutRowStride + combination] = 1U;
    return vector;
}

} // namespace

K1Vector make_random_vector(std::uint64_t seed, std::uint64_t case_index, std::string id)
{
    DeterministicRng rng(seed ^ (case_index * 0xD1342543DE82EF95ULL));
    K1Vector vector;
    vector.id = std::move(id);
    vector.generator_seed = seed;
    vector.case_index = case_index;

    std::array<std::uint32_t, kK1Population> permutation{};
    std::array<bool, kK1Population> is_input{};
    for (std::size_t neuron = 0U; neuron < kK1Population; ++neuron) {
        permutation[neuron] = static_cast<std::uint32_t>(neuron);
    }
    for (std::size_t position = kK1Population - 1U; position > 0U; --position) {
        const std::size_t swap_position = static_cast<std::size_t>(rng.next() % (position + 1U));
        std::swap(permutation[position], permutation[swap_position]);
    }
    for (std::size_t index = 0U; index < 18U; ++index) {
        is_input[permutation[index]] = true;
    }
    vector.updated_neurons = complement_updated(is_input);

    for (Byte& value : vector.state) {
        value = rng.trit();
    }
    for (std::size_t row = 0U; row < kK1UpdatedNeurons; ++row) {
        for (std::size_t entry = 0U; entry < kK1LutEntries; ++entry) {
            vector.lut[row * kK1LutRowStride + entry] = rng.trit();
        }
        for (std::size_t entry = kK1LutEntries; entry < kK1LutRowStride; ++entry) {
            vector.lut[row * kK1LutRowStride + entry] = static_cast<Byte>(0x80U + (rng.next() & 0x7FU));
        }
    }
    for (std::uint32_t& neighbor : vector.neighbors) {
        neighbor = static_cast<std::uint32_t>(rng.next() % kK1Population);
    }
    return vector;
}

std::vector<K1Vector> make_edge_vectors()
{
    std::vector<K1Vector> vectors;
    vectors.reserve(40U);

    K1Vector all_zero = make_base("all-zero-state-lut", 0xA001U, 0U);
    all_zero.state.fill(0U);
    all_zero.lut.fill(0U);
    vectors.push_back(all_zero);

    K1Vector all_one = make_base("all-one-state-lut", 0xA002U, 1U);
    all_one.state.fill(1U);
    all_one.lut.fill(1U);
    vectors.push_back(all_one);

    K1Vector all_unknown = make_base("all-unknown-state-lut", 0xA003U, 2U);
    all_unknown.state.fill(2U);
    all_unknown.lut.fill(2U);
    vectors.push_back(all_unknown);

    vectors.push_back(make_base("mixed-repeating-0-1-2", 0xA004U, 3U));

    for (std::uint32_t combination = 0U; combination < 27U; ++combination) {
        vectors.push_back(k3_case(combination));
    }

    K1Vector repeated = make_base("same-neighbor-used-three-times", 0xA005U, 31U);
    const std::uint32_t repeated_target = repeated.updated_neurons[0U];
    repeated.neighbors[repeated_target * kK1NeighborsPerNeuron] = kNoncontiguousInputs[0U];
    repeated.neighbors[repeated_target * kK1NeighborsPerNeuron + 1U] = kNoncontiguousInputs[0U];
    repeated.neighbors[repeated_target * kK1NeighborsPerNeuron + 2U] = kNoncontiguousInputs[0U];
    repeated.state[kNoncontiguousInputs[0U]] = 2U;
    repeated.lut[26U] = 1U;
    vectors.push_back(repeated);

    K1Vector distinct = make_base("distinct-neighbor-indices", 0xA006U, 32U);
    const std::uint32_t distinct_target = distinct.updated_neurons[0U];
    distinct.neighbors[distinct_target * kK1NeighborsPerNeuron] = kNoncontiguousInputs[0U];
    distinct.neighbors[distinct_target * kK1NeighborsPerNeuron + 1U] = kNoncontiguousInputs[1U];
    distinct.neighbors[distinct_target * kK1NeighborsPerNeuron + 2U] = kNoncontiguousInputs[2U];
    distinct.state[kNoncontiguousInputs[0U]] = 0U;
    distinct.state[kNoncontiguousInputs[1U]] = 1U;
    distinct.state[kNoncontiguousInputs[2U]] = 2U;
    distinct.lut[21U] = 2U;
    vectors.push_back(distinct);

    K1Vector role_case = make_base("noncontiguous-external-input-roles", 0xA007U, 33U);
    role_case.state[kNoncontiguousInputs[0U]] = 1U;
    role_case.state[kNoncontiguousInputs[1U]] = 2U;
    role_case.state[kNoncontiguousInputs[2U]] = 0U;
    vectors.push_back(role_case);

    K1Vector output_signal = make_base("output-and-signal-are-recurrent-roles", 0xA008U, 34U);
    output_signal.lut[0U] = 2U;
    output_signal.lut[kK1LutRowStride + 13U] = 1U;
    vectors.push_back(output_signal);

    K1Vector isolation = make_base("previous-state-isolation", 0xA009U, 35U);
    const std::uint32_t first = isolation.updated_neurons[0U];
    const std::uint32_t later = isolation.updated_neurons[1U];
    const std::uint32_t input0 = kNoncontiguousInputs[0U];
    const std::uint32_t input1 = kNoncontiguousInputs[1U];
    const std::uint32_t input2 = kNoncontiguousInputs[2U];
    isolation.state[input0] = 0U;
    isolation.state[input1] = 0U;
    isolation.state[input2] = 0U;
    isolation.neighbors[first * kK1NeighborsPerNeuron] = input0;
    isolation.neighbors[first * kK1NeighborsPerNeuron + 1U] = input1;
    isolation.neighbors[first * kK1NeighborsPerNeuron + 2U] = input2;
    isolation.neighbors[later * kK1NeighborsPerNeuron] = first;
    isolation.neighbors[later * kK1NeighborsPerNeuron + 1U] = first;
    isolation.neighbors[later * kK1NeighborsPerNeuron + 2U] = first;
    isolation.lut[0U] = 1U;
    isolation.lut[kK1LutRowStride + 0U] = 2U;
    isolation.lut[kK1LutRowStride + 13U] = 0U;
    vectors.push_back(isolation);

    K1Vector padding = make_base("padded-buffer-round-trip", 0xA00AU, 36U);
    vectors.push_back(padding);

    return vectors;
}

std::vector<K1Vector> make_deterministic_vectors(std::uint64_t seed, std::size_t count)
{
    std::vector<K1Vector> vectors;
    vectors.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        vectors.push_back(make_random_vector(
            seed,
            static_cast<std::uint64_t>(index),
            "case-" + std::to_string(index)));
    }
    return vectors;
}

Task make_task(const K1Vector& vector)
{
    Task task;
    task.header.shape = TaskShape{18U, 1U, 2U, 64U, 3U};
    task.topology.neighbors.assign(vector.neighbors.begin(), vector.neighbors.end());
    std::array<bool, kK1Population> is_updated{};
    for (const std::uint32_t neuron : vector.updated_neurons) {
        if (neuron < kK1Population) {
            is_updated[neuron] = true;
        }
    }
    for (std::size_t neuron = 0U; neuron < kK1Population; ++neuron) {
        if (!is_updated[neuron]) {
            task.topology.input_neurons.push_back(static_cast<std::uint32_t>(neuron));
        }
    }
    task.topology.output_neurons = {vector.updated_neurons[0U]};
    task.topology.signal_neuron = vector.updated_neurons[1U];
    return task;
}

xdna::runtime::K1LogicalInput logical_input(const K1Vector& vector)
{
    return xdna::runtime::K1LogicalInput{
        std::span<const Byte>(vector.state),
        std::span<const Byte>(vector.lut),
        std::span<const std::uint32_t>(vector.neighbors),
        std::span<const std::uint32_t>(vector.updated_neurons),
    };
}

std::vector<Byte> cpu_expected(const K1Vector& vector)
{
    const Task task = make_task(vector);
    xdna::bpp9000::Lut lut(task);
    lut.storage().assign(vector.lut.begin(), vector.lut.end());
    return xdna::bpp9000::recurrent_tick(task, vector.state, lut);
}

} // namespace k1_test
