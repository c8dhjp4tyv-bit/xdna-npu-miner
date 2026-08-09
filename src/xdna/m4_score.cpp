#include "xdna/m4_score.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <string>

namespace xdna::runtime {
namespace {

using bpp9000::Byte;
using bpp9000::Lut;
using bpp9000::Task;
using bpp9000::TaskShape;
using bpp9000::Trit;

void require_m4_task_shape(const Task& task, const Lut& lut)
{
    const TaskShape& shape = task.header.shape;
    if (task.header.magic != bpp9000::kTaskMagic || task.header.version != bpp9000::kTaskVersion) {
        throw bpp9000::TaskError(
            bpp9000::TaskErrorCode::BadMagic,
            "M4 scoring requires a validated task header");
    }
    if (shape.input_trits != kM4InputTrits || shape.output_trits != 1U || shape.population != kM4Population
        || shape.neighbors != kM4NeighborsPerNeuron || shape.sequence_length == 0U) {
        throw bpp9000::TaskError(
            bpp9000::TaskErrorCode::BadDimensions,
            "M4 artifact requires the production neuron/input/topology shape");
    }
    if (task.topology.input_neurons.size() != kM4InputTrits || task.topology.output_neurons.size() != 1U
        || task.topology.neighbors.size() != kM4NeighborsCount) {
        throw bpp9000::TaskError(
            bpp9000::TaskErrorCode::InvalidTopology,
            "M4 task topology lengths do not match the device contract");
    }
    if (lut.population() != kM4Population || lut.rows() != kM4UpdatedNeurons) {
        throw bpp9000::TaskError(
            bpp9000::TaskErrorCode::InvalidTopology,
            "M4 LUT dimensions do not match the device contract");
    }
    const std::size_t sequence_length = static_cast<std::size_t>(shape.sequence_length);
    if (shape.sequence_length > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() / kM4InputTrits)
        || task.inputs.size() != sequence_length * kM4InputTrits
        || task.outputs.size() != sequence_length) {
        throw bpp9000::TaskError(
            bpp9000::TaskErrorCode::BadLength,
            "M4 task data length is not addressable or does not match the header");
    }
    for (const Trit value : task.inputs) {
        if (!bpp9000::is_valid_trit_byte(bpp9000::trit_to_byte(value))) {
            throw bpp9000::TaskError(
                bpp9000::TaskErrorCode::InvalidDomainValue,
                "M4 task input contains an invalid trit");
        }
    }
    for (const Trit value : task.outputs) {
        if (!bpp9000::is_valid_trit_byte(bpp9000::trit_to_byte(value))) {
            throw bpp9000::TaskError(
                bpp9000::TaskErrorCode::InvalidDomainValue,
                "M4 task output contains an invalid trit");
        }
    }
}

void require_m4_window_config(const Task& task, const bpp9000::ReferenceConfig& config)
{
    if (config.window_width < 2U
        || config.window_width > static_cast<std::uint64_t>(kM4MaxWindowWidth)
        || config.window_width >= task.header.shape.sequence_length
        || config.max_ticks <= config.window_width
        || config.max_ticks > bpp9000::kProductionMaxTicks) {
        throw bpp9000::TaskError(
            bpp9000::TaskErrorCode::BadDimensions,
            "M4 window configuration is invalid");
    }
    const std::uint64_t window_count = task.header.shape.sequence_length - config.window_width;
    if (window_count > std::numeric_limits<std::uint32_t>::max()) {
        throw bpp9000::TaskError(
            bpp9000::TaskErrorCode::BadDimensions,
            "M4 window count does not fit the score result");
    }
}

[[nodiscard]] M4LogicalInput make_input(const Task& task,
                                        const Lut& lut,
                                        M4Mode mode,
                                        std::span<const Byte> initial_state,
                                        std::span<const Byte> input_sequence,
                                        std::span<const Byte> targets,
                                        std::uint32_t tick_count,
                                        std::uint32_t window_width,
                                        std::uint32_t max_ticks)
{
    return M4LogicalInput{
        mode,
        tick_count,
        window_width,
        max_ticks,
        task.topology.output_neurons[0U],
        task.topology.signal_neuron,
        initial_state,
        std::span<const Byte>(lut.storage()),
        std::span<const std::uint32_t>(task.topology.neighbors),
        std::span<const std::uint32_t>(lut.updated_neurons()),
        std::span<const std::uint32_t>(task.topology.input_neurons),
        input_sequence,
        targets,
    };
}

[[nodiscard]] bool window_equal(const bpp9000::WindowResult& cpu,
                                const bpp9000::WindowResult& npu) noexcept
{
    const ScoreVerification score = verify_score_exact(cpu.score, npu.score);
    return score.verified() && cpu.predicted == npu.predicted && cpu.expected == npu.expected
        && cpu.feed_count == npu.feed_count;
}

void add_score_result(bpp9000::ScoreResult& total,
                      const bpp9000::ScoreResult& current,
                      std::uint64_t window_index)
{
    if (window_index >= std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("M4 window index cannot be represented by the score result");
    }
    total.windows_evaluated = static_cast<std::uint32_t>(window_index + 1U);
    total.ticks = current.ticks;
    if (current.timed_out()) {
        total.score = bpp9000::kTimeoutScore;
        total.status = bpp9000::ScoreStatus::Timeout;
        return;
    }
    if (total.score > std::numeric_limits<std::uint32_t>::max() - current.score) {
        throw std::runtime_error("M4 score accumulator overflowed its uint32 contract");
    }
    total.score += current.score;
}

} // namespace

M4DeviceResult M4NpuScorer::dispatch(const M4LogicalInput& input)
{
    M4PackedBuffers buffers = pack_m4(input);
    runtime_.dispatch_m4(buffers);
    return unpack_m4_result(buffers.output);
}

M4LogicalInput M4NpuScorer::task_input(const Task& task,
                                       const Lut& lut,
                                       M4Mode mode,
                                       std::span<const Byte> initial_state,
                                       std::span<const Byte> input_sequence,
                                       std::span<const Byte> targets,
                                       std::uint32_t tick_count,
                                       std::uint32_t window_width,
                                       std::uint32_t max_ticks) const
{
    return make_input(
        task,
        lut,
        mode,
        initial_state,
        input_sequence,
        targets,
        tick_count,
        window_width,
        max_ticks);
}

M4DeviceResult M4NpuScorer::dispatch_single_tick(const Task& task,
                                                 std::span<const Byte> initial_state,
                                                 const Lut& lut)
{
    require_m4_task_shape(task, lut);
    const M4LogicalInput input
        = task_input(task, lut, M4Mode::SingleTick, initial_state, {}, {}, 1U, 0U, 0U);
    return dispatch(input);
}

M4DeviceResult M4NpuScorer::dispatch_repeated_ticks(const Task& task,
                                                    std::span<const Byte> initial_state,
                                                    const Lut& lut,
                                                    std::span<const Byte> input_sequence)
{
    require_m4_task_shape(task, lut);
    if (input_sequence.empty() || input_sequence.size() % kM4InputTrits != 0U) {
        throw bpp9000::TaskError(
            bpp9000::TaskErrorCode::BadLength,
            "M4 repeated-tick input sequence must contain complete 18-trit rows");
    }
    const std::size_t rows = input_sequence.size() / kM4InputTrits;
    if (rows > kM4MaxWindowWidth) {
        throw bpp9000::TaskError(
            bpp9000::TaskErrorCode::BadLength,
            "M4 repeated-tick input sequence exceeds the device arena");
    }
    const M4LogicalInput input = task_input(
        task,
        lut,
        M4Mode::RepeatedTicks,
        initial_state,
        input_sequence,
        {},
        static_cast<std::uint32_t>(rows),
        0U,
        0U);
    return dispatch(input);
}

bpp9000::WindowResult M4NpuScorer::score_window_npu(const Task& task,
                                                    const Lut& lut,
                                                    std::uint64_t window_start,
                                                    const bpp9000::ReferenceConfig& config)
{
    require_m4_task_shape(task, lut);
    require_m4_window_config(task, config);
    const std::uint64_t window_count = task.header.shape.sequence_length - config.window_width;
    if (window_start >= window_count) {
        throw bpp9000::TaskError(
            bpp9000::TaskErrorCode::BadDimensions,
            "M4 window start is outside the task");
    }
    const std::size_t width = static_cast<std::size_t>(config.window_width);
    const std::size_t start = static_cast<std::size_t>(window_start);
    std::vector<Byte> input_sequence(width * kM4InputTrits, 0U);
    for (std::size_t index = 0U; index < input_sequence.size(); ++index) {
        input_sequence[index] = bpp9000::trit_to_byte(task.inputs[start * kM4InputTrits + index]);
    }
    std::vector<Byte> targets(width + 1U, 0U);
    for (std::size_t index = 0U; index < targets.size(); ++index) {
        targets[index] = bpp9000::trit_to_byte(task.outputs[start + index]);
    }
    std::array<Byte, kM4StateLogicalBytes> reset_state{};
    reset_state.fill(bpp9000::trit_to_byte(Trit::Unknown));
    const M4LogicalInput input = task_input(
        task,
        lut,
        M4Mode::WindowScore,
        reset_state,
        input_sequence,
        targets,
        0U,
        static_cast<std::uint32_t>(config.window_width),
        config.max_ticks);
    const M4DeviceResult device = dispatch(input);
    if (device.feed_count > config.window_width) {
        throw std::runtime_error("M4 device returned a feed count beyond the requested window");
    }

    bpp9000::WindowResult result;
    result.feed_count = device.feed_count;
    result.score.windows_evaluated = 1U;
    result.score.ticks = device.ticks;
    if (device.timed_out()) {
        result.score.score = bpp9000::kTimeoutScore;
        result.score.status = bpp9000::ScoreStatus::Timeout;
        result.predicted = Trit::Unknown;
        result.expected = Trit::Unknown;
        return result;
    }
    const std::size_t expected_index = start + static_cast<std::size_t>(device.feed_count);
    const Trit host_expected = task.outputs[expected_index];
    if (device.expected != host_expected) {
        throw std::runtime_error("M4 device target lookup disagrees with the host task sequence");
    }
    result.predicted = device.predicted;
    result.expected = device.expected;
    result.score.score = device.score;
    result.score.status = bpp9000::ScoreStatus::Settled;
    const std::uint32_t expected_score = result.predicted == result.expected ? 0U : 1U;
    if (device.score != expected_score) {
        throw std::runtime_error("M4 device output comparison disagrees with its score");
    }
    return result;
}

bpp9000::ScoreResult M4NpuScorer::score_lut_npu(const Task& task,
                                                const Lut& lut,
                                                const bpp9000::ReferenceConfig& config)
{
    require_m4_task_shape(task, lut);
    require_m4_window_config(task, config);
    const std::uint64_t window_count = task.header.shape.sequence_length - config.window_width;
    bpp9000::ScoreResult total{0U, bpp9000::ScoreStatus::Settled, 0U, 0U};
    for (std::uint64_t window = 0U; window < window_count; ++window) {
        const bpp9000::WindowResult current = score_window_npu(task, lut, window, config);
        add_score_result(total, current.score, window);
        if (current.score.timed_out()) {
            return total;
        }
    }
    return total;
}

M4ScoreRun M4NpuScorer::score_lut_verified(const Task& task,
                                            const Lut& lut,
                                            const bpp9000::ReferenceConfig& config)
{
    require_m4_task_shape(task, lut);
    require_m4_window_config(task, config);
    const std::uint64_t window_count = task.header.shape.sequence_length - config.window_width;
    M4ScoreRun run;
    run.cpu = bpp9000::ScoreResult{0U, bpp9000::ScoreStatus::Settled, 0U, 0U};
    run.npu = run.cpu;
    run.status = VerificationStatus::Verified;
    for (std::uint64_t window = 0U; window < window_count; ++window) {
        const bpp9000::WindowResult cpu_window = bpp9000::score_window(task, lut, window, config);
        const bpp9000::WindowResult npu_window = score_window_npu(task, lut, window, config);
        ++run.windows_compared;
        add_score_result(run.cpu, cpu_window.score, window);
        add_score_result(run.npu, npu_window.score, window);
        if (!window_equal(cpu_window, npu_window)) {
            run.status = VerificationStatus::RejectedMismatch;
            run.first_mismatch_window = window;
            run.first_mismatch = WindowVerification{
                cpu_window,
                npu_window,
                VerificationStatus::RejectedMismatch,
            };
            return run;
        }
        if (cpu_window.score.timed_out()) {
            return run;
        }
    }
    return run;
}

M4CandidateResult M4NpuScorer::score_candidate_verified(
    const Task& task,
    const bpp9000::PublicKey& public_key,
    const bpp9000::MiningSeed& mining_seed,
    const bpp9000::Nonce& nonce,
    const bpp9000::CandidateRandomSource& random_source,
    const bpp9000::ReferenceConfig& config)
{
    const bpp9000::CandidateMaterial material
        = bpp9000::make_candidate_material(task, public_key, mining_seed, nonce, random_source, config);
    bpp9000::Lut current(task);
    current.initialize_from_root_bytes(material.root_bytes);

    M4CandidateResult result;
    result.score_calls = 1U;
    result.npu_score_calls = 1U;
    M4ScoreRun initial_run = score_lut_verified(task, current, config);
    result.windows_compared += initial_run.windows_compared;
    if (!initial_run.verified()) {
        initial_run.candidate_score_call = result.score_calls;
        result.first_mismatch = std::move(initial_run);
        return result;
    }
    if (initial_run.cpu.timed_out()) {
        ++result.timeout_score_calls;
    } else {
        ++result.finite_score_calls;
    }
    const bpp9000::ScoreResult initial = initial_run.cpu;
    bpp9000::CandidateResult candidate{initial, initial, current, current, 1U, {}};
    candidate.attempts.reserve(config.mutation_steps);

    bpp9000::ScoreResult current_score = initial;
    for (std::uint32_t step = 0U; step < config.mutation_steps; ++step) {
        bpp9000::MutationAttempt attempt;
        attempt.mutations.reserve(nonce.bytes[1]);
        const std::size_t word_base = static_cast<std::size_t>(step) * bpp9000::kMaxMutationsPerStep;
        for (std::size_t mutation = 0U; mutation < nonce.bytes[1]; ++mutation) {
            attempt.mutations.push_back(
                bpp9000::mutate_lut(current, material.mutation_words[word_base + mutation]));
        }
        ++result.score_calls;
        ++result.npu_score_calls;
        M4ScoreRun measured = score_lut_verified(task, current, config);
        result.windows_compared += measured.windows_compared;
        if (!measured.verified()) {
            measured.candidate_score_call = result.score_calls;
            measured.candidate_mutation_step = step;
            result.first_mismatch = std::move(measured);
            return result;
        }
        if (measured.cpu.timed_out()) {
            ++result.timeout_score_calls;
        } else {
            ++result.finite_score_calls;
        }
        attempt.measured = measured.cpu;
        attempt.accepted = attempt.measured.score <= current_score.score;
        if (attempt.accepted) {
            current_score = attempt.measured;
        } else {
            for (auto iterator = attempt.mutations.rbegin(); iterator != attempt.mutations.rend(); ++iterator) {
                bpp9000::rollback_mutation(current, *iterator);
            }
        }
        if (current_score.score < candidate.best.score) {
            candidate.best = current_score;
            candidate.best_lut = current;
        }
        candidate.attempts.push_back(std::move(attempt));
    }
    candidate.current_lut = current;
    result.status = VerificationStatus::Verified;
    result.cpu_result = std::move(candidate);
    return result;
}

} // namespace xdna::runtime
