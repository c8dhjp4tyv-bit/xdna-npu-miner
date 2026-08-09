#include "k1_vectors.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using xdna::bpp9000::Byte;
using xdna::runtime::K1ContractError;
using xdna::runtime::K1DeviceLayout;
using xdna::runtime::K1LogicalInput;
using xdna::runtime::K1PackedBuffers;
using xdna::runtime::compare_k1_output;
using xdna::runtime::k1_default_layout;
using xdna::runtime::pack_k1;
using xdna::runtime::unpack_k1;
using xdna::runtime::validate_k1_device_layout;
using xdna::runtime::validate_k1_logical_input;
using xdna::runtime::validate_k1_packed_input;

std::size_t assertions = 0U;

void expect(bool condition, const std::string& message)
{
    ++assertions;
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Function>
void expect_contract_error(Function&& function, const std::string& message)
{
    ++assertions;
    try {
        function();
    } catch (const K1ContractError&) {
        return;
    }
    throw std::runtime_error(message);
}

void test_layout_and_padding()
{
    const K1DeviceLayout layout = k1_default_layout();
    validate_k1_device_layout(layout);
    const auto vectors = k1_test::make_edge_vectors();
    const K1LogicalInput input = k1_test::logical_input(vectors.front());
    K1PackedBuffers packed = pack_k1(input, layout);
    expect(packed.previous_state.size() == layout.state_stride_bytes, "K1 state device stride is explicit");
    expect(packed.lut.size() == 46U * 32U, "K1 LUT device bytes preserve row stride");
    expect(packed.neighbors.size() == 64U * 3U, "K1 topology device rows are explicit");
    expect(packed.updated_neurons.size() == 48U, "K1 updated list has explicit padded word count");
    expect(packed.next_state.size() == layout.state_stride_bytes, "K1 output stride is explicit");

    std::copy(vectors.front().state.begin(), vectors.front().state.end(), packed.next_state.begin());
    for (std::size_t index = 64U; index < packed.next_state.size(); ++index) {
        packed.next_state[index] = static_cast<Byte>(0xD0U + index);
    }
    const std::vector<Byte> unpacked = unpack_k1(packed.next_state, layout);
    expect(std::equal(vectors.front().state.begin(), vectors.front().state.end(), unpacked.begin()),
           "logical state survives pack/unpack while output padding is ignored");

    packed.previous_state[64U] = 0x01U;
    packed.previous_state[95U] = 0xFEU;
    packed.lut[27U] = 0xF1U;
    packed.lut[31U] = 0xF2U;
    validate_k1_packed_input(packed, layout);
    expect(true, "semantically unused padding is accepted");

    K1DeviceLayout malformed = layout;
    malformed.state_stride_bytes = 64U;
    expect_contract_error([&] { validate_k1_device_layout(malformed); }, "malformed state stride is rejected");
}

void test_malformed_logical_inputs()
{
    const k1_test::K1Vector source = k1_test::make_edge_vectors().front();
    std::vector<Byte> state(source.state.begin(), source.state.end());
    std::vector<Byte> lut(source.lut.begin(), source.lut.end());
    std::vector<std::uint32_t> neighbors(source.neighbors.begin(), source.neighbors.end());
    std::vector<std::uint32_t> updated(source.updated_neurons.begin(), source.updated_neurons.end());

    auto input = [&] {
        return K1LogicalInput{state, lut, neighbors, updated};
    };

    state.pop_back();
    expect_contract_error([&] { validate_k1_logical_input(input()); }, "wrong state length is rejected");
    state.assign(source.state.begin(), source.state.end());

    lut.pop_back();
    expect_contract_error([&] { validate_k1_logical_input(input()); }, "wrong LUT length is rejected");
    lut.assign(source.lut.begin(), source.lut.end());

    neighbors.pop_back();
    expect_contract_error([&] { validate_k1_logical_input(input()); }, "wrong topology length is rejected");
    neighbors.assign(source.neighbors.begin(), source.neighbors.end());

    updated.pop_back();
    expect_contract_error([&] { validate_k1_logical_input(input()); }, "wrong updated-neuron length is rejected");
    updated.assign(source.updated_neurons.begin(), source.updated_neurons.end());

    state[0U] = 3U;
    expect_contract_error([&] { validate_k1_logical_input(input()); }, "state trit above two is rejected");
    state[0U] = source.state[0U];
    lut[0U] = 3U;
    expect_contract_error([&] { validate_k1_logical_input(input()); }, "LUT trit above two is rejected");
    lut[0U] = source.lut[0U];
    neighbors[0U] = 64U;
    expect_contract_error([&] { validate_k1_logical_input(input()); }, "neighbor index 64 is rejected");
    neighbors[0U] = source.neighbors[0U];
    updated[1U] = updated[0U];
    expect_contract_error([&] { validate_k1_logical_input(input()); }, "duplicate updated-neuron row is rejected");

    K1PackedBuffers malformed = pack_k1(k1_test::logical_input(source));
    malformed.next_state.pop_back();
    expect_contract_error([&] { validate_k1_packed_input(malformed); }, "malformed packed output size is rejected");
}

void test_oracle_and_exact_comparison()
{
    const std::vector<k1_test::K1Vector> edges = k1_test::make_edge_vectors();
    const auto isolation = std::find_if(edges.begin(), edges.end(), [](const auto& vector) {
        return vector.id == "previous-state-isolation";
    });
    expect(isolation != edges.end(), "isolation vector is present");
    const std::vector<Byte> expected = k1_test::cpu_expected(*isolation);
    expect(expected[isolation->updated_neurons[0U]] == 1U, "first recurrent update uses the prior snapshot");
    expect(expected[isolation->updated_neurons[1U]] == 2U,
           "later recurrent update does not observe the just-written value");

    const auto role_case = std::find_if(edges.begin(), edges.end(), [](const auto& vector) {
        return vector.id == "noncontiguous-external-input-roles";
    });
    expect(role_case != edges.end(), "external input role vector is present");
    const std::vector<Byte> role_expected = k1_test::cpu_expected(*role_case);
    expect(role_expected[1U] == role_case->state[1U] && role_expected[62U] == role_case->state[62U],
           "noncontiguous input roles are copied/held by the CPU oracle");

    const std::vector<Byte> actual = expected;
    const auto match = compare_k1_output(expected, actual);
    expect(match.matches(), "equal logical outputs compare exactly");
    std::vector<Byte> altered = actual;
    altered[isolation->updated_neurons[1U]] ^= 1U;
    const auto mismatch = compare_k1_output(expected, altered);
    expect(!mismatch.matches() && mismatch.differing_indices.size() == 1U,
           "output mismatch path identifies the exact differing index");

    std::size_t combinations = 0U;
    for (const auto& vector : edges) {
        if (vector.id.starts_with("k3-combination-")) {
            const std::vector<Byte> combination_expected = k1_test::cpu_expected(vector);
            expect(combination_expected[vector.updated_neurons[0U]] == 1U,
                   "CPU oracle covers each three-trit LUT combination");
            ++combinations;
        }
    }
    expect(combinations == 27U, "all 27 three-trit combinations are represented");
}

void run_test(const char* name, void (*function)(), std::size_t& passed, std::size_t& failed)
{
    try {
        function();
        ++passed;
        std::cout << "PASS " << name << '\n';
    } catch (const std::exception& error) {
        ++failed;
        std::cerr << "FAIL " << name << ": " << error.what() << '\n';
    }
}

} // namespace

int main()
{
    std::size_t passed = 0U;
    std::size_t failed = 0U;
    run_test("layout_and_padding", test_layout_and_padding, passed, failed);
    run_test("malformed_logical_inputs", test_malformed_logical_inputs, passed, failed);
    run_test("oracle_and_exact_comparison", test_oracle_and_exact_comparison, passed, failed);
    std::cout << "tests_passed=" << passed << " tests_failed=" << failed << " assertions=" << assertions << '\n';
    return failed == 0U ? 0 : 1;
}
