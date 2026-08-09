#include "m4_vectors.hpp"

#include <array>
#include <algorithm>

namespace m4_test {
namespace {

using xdna::bpp9000::Byte;
using xdna::bpp9000::DeterministicFixtureDigest;
using xdna::bpp9000::Lut;
using xdna::bpp9000::Task;
using xdna::bpp9000::TaskShape;
using xdna::bpp9000::Trit;

constexpr std::array<std::uint32_t, 18U> kInputRoles{
    1U, 4U, 7U, 11U, 14U, 18U, 21U, 25U, 28U,
    32U, 35U, 39U, 42U, 46U, 49U, 53U, 58U, 62U,
};

class Rng {
public:
    explicit Rng(std::uint64_t state)
        : state_(state)
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

[[nodiscard]] std::vector<std::uint32_t> shuffled_input_roles(Rng& rng)
{
    std::array<std::uint32_t, kInputRoles.size()> roles = kInputRoles;
    for (std::size_t index = roles.size() - 1U; index > 0U; --index) {
        const std::size_t swap_index = static_cast<std::size_t>(rng.next() % (index + 1U));
        std::swap(roles[index], roles[swap_index]);
    }
    return std::vector<std::uint32_t>(roles.begin(), roles.end());
}

} // namespace

M4Case make_case(std::uint64_t seed,
                 std::uint64_t case_index,
                 std::uint64_t sequence_length,
                 std::uint32_t window_width,
                 bool timeout,
                 unsigned int output_mode)
{
    Rng rng(seed ^ (case_index * 0xD1342543DE82EF95ULL));
    Task task;
    task.header.shape = TaskShape{18U, 1U, sequence_length, 64U, 3U};
    task.topology.input_neurons = shuffled_input_roles(rng);
    std::array<bool, 64U> input_mask{};
    for (const std::uint32_t neuron : task.topology.input_neurons) {
        input_mask[neuron] = true;
    }
    std::vector<std::uint32_t> updated;
    updated.reserve(46U);
    for (std::size_t neuron = 0U; neuron < input_mask.size(); ++neuron) {
        if (!input_mask[neuron]) {
            updated.push_back(static_cast<std::uint32_t>(neuron));
        }
    }
    task.topology.output_neurons = {updated[0U]};
    task.topology.signal_neuron = updated[1U];
    task.topology.neighbors.resize(64U * 3U);
    for (std::uint32_t& neighbor : task.topology.neighbors) {
        neighbor = static_cast<std::uint32_t>(rng.next() % 64U);
    }
    if (timeout) {
        for (std::size_t index = 0U; index < 3U; ++index) {
            task.topology.neighbors[task.topology.signal_neuron * 3U + index] = task.topology.signal_neuron;
        }
    }

    const std::size_t rows = static_cast<std::size_t>(sequence_length);
    task.inputs.resize(rows * 18U);
    task.outputs.resize(rows);
    for (Trit& value : task.inputs) {
        value = static_cast<Trit>(rng.trit());
    }
    for (std::size_t row = 0U; row < rows; ++row) {
        if (output_mode == 1U) {
            task.outputs[row] = Trit::Zero;
        } else if (output_mode == 2U) {
            task.outputs[row] = static_cast<Trit>((seed + row) % 3U);
        } else {
            task.outputs[row] = Trit::Unknown;
        }
    }

    Lut lut(task);
    if (timeout) {
        lut.fill(Trit::Zero);
    } else {
        for (std::size_t row = 0U; row < lut.rows(); ++row) {
            for (std::size_t entry = 0U; entry < xdna::bpp9000::kLutEntries; ++entry) {
                lut.set_row(row, entry, static_cast<Trit>(rng.next() % 3U));
            }
        }
        const auto signal_iterator
            = std::find(lut.updated_neurons().begin(), lut.updated_neurons().end(), task.topology.signal_neuron);
        const std::size_t signal_row
            = static_cast<std::size_t>(signal_iterator - lut.updated_neurons().begin());
        lut.set_row(signal_row, 0U, Trit::Unknown);
        lut.set_row(signal_row, 1U, Trit::Unknown);
        lut.set_row(signal_row, 2U, Trit::Unknown);
        for (std::size_t entry = 3U; entry < xdna::bpp9000::kLutEntries; ++entry) {
            lut.set_row(signal_row, entry, Trit::Unknown);
        }
        // Keep the signal self-loop for deterministic settling while allowing
        // every other recurrent row to exercise arbitrary topology/LUT data.
        for (std::size_t index = 0U; index < 3U; ++index) {
            task.topology.neighbors[task.topology.signal_neuron * 3U + index]
                = task.topology.signal_neuron;
        }
    }

    const DeterministicFixtureDigest digest;
    task.packed_topology = xdna::bpp9000::serialize_topology(task.header.shape, task.topology);
    task.packed_data = xdna::bpp9000::serialize_data(task.header.shape, task.inputs, task.outputs);
    task.header.topology_hash = digest.digest(task.packed_topology);
    task.header.data_hash = digest.digest(task.packed_data);

    const xdna::bpp9000::ReferenceConfig config{
        window_width,
        timeout ? window_width + 4U : 100000U,
        xdna::bpp9000::kProductionMutationSteps,
    };
    return M4Case{
        "m4-case-" + std::to_string(case_index),
        seed,
        case_index,
        std::move(task),
        std::move(lut),
        config,
    };
}

std::vector<M4Case> make_fixed_cases(std::size_t count)
{
    std::vector<M4Case> cases;
    cases.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        cases.push_back(make_case(
            0x4D34464958454431ULL + index,
            static_cast<std::uint64_t>(index),
            8U,
            2U,
            index == 2U,
            static_cast<unsigned int>(index % 3U)));
    }
    return cases;
}

std::vector<M4Case> make_random_cases(std::uint64_t seed, std::size_t count)
{
    std::vector<M4Case> cases;
    cases.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        const bool timeout = index % 97U == 0U;
        const std::uint32_t width = static_cast<std::uint32_t>((index % 4U) + 2U);
        cases.push_back(make_case(
            seed,
            static_cast<std::uint64_t>(index),
            static_cast<std::uint64_t>(width + 6U),
            width,
            timeout,
            static_cast<unsigned int>(index % 3U)));
    }
    return cases;
}

} // namespace m4_test
