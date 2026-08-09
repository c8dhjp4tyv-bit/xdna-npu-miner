#include "m4_vectors.hpp"
#include "xdna/m4.hpp"
#include "xdna/verification.hpp"

#include <array>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using xdna::bpp9000::Byte;
using xdna::bpp9000::ScoreResult;
using xdna::bpp9000::ScoreStatus;
using xdna::bpp9000::Trit;
using xdna::runtime::M4ContractError;
using xdna::runtime::M4LogicalInput;
using xdna::runtime::M4Mode;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void expect_contract_failure(const std::function<void()>& function, const std::string& message)
{
    try {
        function();
    } catch (const M4ContractError&) {
        return;
    }
    throw std::runtime_error(message);
}

void expect_task_failure(const std::function<void()>& function, const std::string& message)
{
    try {
        function();
    } catch (const xdna::bpp9000::TaskError&) {
        return;
    }
    throw std::runtime_error(message);
}

void write_u32(std::vector<Byte>& bytes, std::size_t offset, std::uint32_t value)
{
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void test_pack_contract()
{
    const m4_test::M4Case fixture = m4_test::make_case(0xC0DEU, 0U);
    std::array<Byte, 64U> initial{};
    initial.fill(2U);
    std::vector<Byte> inputs(2U * 18U, 1U);
    std::vector<Byte> targets(3U, 2U);
    const M4LogicalInput logical{
        M4Mode::WindowScore,
        0U,
        2U,
        8U,
        fixture.task.topology.output_neurons[0U],
        fixture.task.topology.signal_neuron,
        initial,
        fixture.lut.storage(),
        fixture.task.topology.neighbors,
        fixture.lut.updated_neurons(),
        fixture.task.topology.input_neurons,
        inputs,
        targets,
    };
    const xdna::runtime::M4PackedBuffers packed = xdna::runtime::pack_m4(logical);
    expect(packed.input.size() == xdna::runtime::kM4InputBufferBytes, "M4 packed input size is exact");
    expect(packed.output.size() == xdna::runtime::kM4OutputBufferBytes, "M4 packed output size is exact");

    std::vector<Byte> invalid_state = inputs;
    invalid_state[0U] = 3U;
    const M4LogicalInput invalid = M4LogicalInput{
        logical.mode,
        logical.tick_count,
        logical.window_width,
        logical.max_ticks,
        logical.output_neuron,
        logical.signal_neuron,
        std::array<Byte, 64U>{},
        logical.lut,
        logical.neighbors,
        logical.updated_neurons,
        logical.input_neurons,
        invalid_state,
        logical.targets,
    };
    expect_contract_failure([&] { (void)xdna::runtime::pack_m4(invalid); }, "invalid M4 input trit is rejected");

    std::vector<Byte> malformed_targets(2U, 2U);
    const M4LogicalInput malformed = M4LogicalInput{
        logical.mode,
        logical.tick_count,
        logical.window_width,
        logical.max_ticks,
        logical.output_neuron,
        logical.signal_neuron,
        logical.initial_state,
        logical.lut,
        logical.neighbors,
        logical.updated_neurons,
        logical.input_neurons,
        logical.input_sequence,
        malformed_targets,
    };
    expect_contract_failure([&] { (void)xdna::runtime::pack_m4(malformed); }, "malformed target sequence is rejected");

    std::vector<std::uint32_t> invalid_neighbors(logical.neighbors.begin(), logical.neighbors.end());
    invalid_neighbors[0U] = 64U;
    M4LogicalInput invalid_topology = logical;
    invalid_topology.neighbors = invalid_neighbors;
    expect_contract_failure(
        [&] { (void)xdna::runtime::pack_m4(invalid_topology); },
        "invalid M4 topology is rejected");

    std::vector<Byte> malformed_inputs(18U, 1U);
    M4LogicalInput malformed_sequence = logical;
    malformed_sequence.input_sequence = malformed_inputs;
    expect_contract_failure(
        [&] { (void)xdna::runtime::pack_m4(malformed_sequence); },
        "malformed M4 input sequence is rejected");

    M4LogicalInput excessive_ticks = logical;
    excessive_ticks.max_ticks = xdna::bpp9000::kProductionMaxTicks + 1U;
    expect_contract_failure(
        [&] { (void)xdna::runtime::pack_m4(excessive_ticks); },
        "M4 maximum tick contract is bounded");

    xdna::runtime::M4PackedBuffers corrupted = xdna::runtime::pack_m4(logical);
    write_u32(corrupted.input, xdna::runtime::kM4NeighborsOffset, 64U);
    expect_contract_failure(
        [&] { xdna::runtime::validate_m4_packed_input(corrupted); },
        "invalid packed M4 topology is rejected");

    expect_task_failure(
        [&] {
            (void)xdna::bpp9000::score_window(
                fixture.task,
                fixture.lut,
                fixture.task.header.shape.sequence_length,
                fixture.config);
        },
        "invalid M4 window index is rejected before dispatch");
}

void test_timeout_transport_and_gate()
{
    std::vector<Byte> output(xdna::runtime::kM4OutputBufferBytes, 0U);
    for (std::size_t index = 0U; index < 64U; ++index) {
        output[index] = 2U;
    }
    write_u32(output, xdna::runtime::kM4OutputScoreOffset, xdna::bpp9000::kTimeoutScore);
    write_u32(output,
              xdna::runtime::kM4OutputStatusOffset,
              static_cast<std::uint32_t>(xdna::runtime::M4DeviceStatus::Timeout));
    write_u32(output, xdna::runtime::kM4OutputTicksOffset, 100000U);
    write_u32(output, xdna::runtime::kM4OutputFeedCountOffset, 2U);
    write_u32(output, xdna::runtime::kM4OutputPredictedOffset, 2U);
    write_u32(output, xdna::runtime::kM4OutputExpectedOffset, 2U);
    write_u32(output, xdna::runtime::kM4OutputMagicOffset, xdna::runtime::kM4OutputMagic);
    const auto result = xdna::runtime::unpack_m4_result(output);
    expect(result.timed_out() && result.score == xdna::bpp9000::kTimeoutScore,
           "M4 timeout sentinel survives device result transport");

    const ScoreResult equal_timeout{
        xdna::bpp9000::kTimeoutScore,
        ScoreStatus::Timeout,
        3U,
        100000U,
    };
    const auto equal = xdna::runtime::verify_score_exact(equal_timeout, equal_timeout);
    expect(equal.verified(), "equal timeout results are verified");
    const auto mismatch = xdna::runtime::verify_score_exact(
        ScoreResult{1U, ScoreStatus::Settled, 3U, 4U},
        equal_timeout);
    expect(!mismatch.verified() && mismatch.status == xdna::runtime::VerificationStatus::RejectedMismatch,
           "score/status mismatch is rejected fail-closed");
}

} // namespace

int main()
{
    try {
        test_pack_contract();
        test_timeout_transport_and_gate();
        std::cout << "PASS m4_contracts\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL m4_contracts: " << error.what() << '\n';
        return 1;
    }
}
