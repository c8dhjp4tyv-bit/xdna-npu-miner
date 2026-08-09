#include "bpp9000/reference.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace xdna::bpp9000 {
namespace {

void require_scorable_task(const Task& task, const ReferenceConfig& config)
{
    const TaskShape& shape = task.header.shape;
    if (task.header.magic != kTaskMagic) {
        throw TaskError(TaskErrorCode::BadMagic, "scoring requires a validated task header magic");
    }
    if (task.header.version != kTaskVersion) {
        throw TaskError(TaskErrorCode::BadVersion, "scoring requires a validated task version");
    }
    if (shape.input_trits == 0U || shape.population == 0U || shape.sequence_length == 0U
        || (shape.population & (shape.population - 1U)) != 0U) {
        throw TaskError(TaskErrorCode::BadDimensions, "scoring requires valid nonzero power-of-two dimensions");
    }
    if (task.topology.input_neurons.size() != shape.input_trits
        || task.topology.output_neurons.size() != shape.output_trits
        || task.topology.neighbors.size()
            != static_cast<std::size_t>(shape.population) * static_cast<std::size_t>(shape.neighbors)) {
        throw TaskError(TaskErrorCode::InvalidTopology, "scoring requires complete topology vectors");
    }
    std::vector<bool> seen(shape.population, false);
    for (const std::uint32_t neuron : task.topology.input_neurons) {
        if (neuron >= shape.population || seen[neuron]) {
            throw TaskError(TaskErrorCode::InvalidTopology, "scoring requires unique in-range input roles");
        }
        seen[neuron] = true;
    }
    for (const std::uint32_t neuron : task.topology.output_neurons) {
        if (neuron >= shape.population || seen[neuron]) {
            throw TaskError(TaskErrorCode::InvalidTopology, "scoring requires unique in-range output roles");
        }
        seen[neuron] = true;
    }
    if (task.topology.signal_neuron >= shape.population || seen[task.topology.signal_neuron]) {
        throw TaskError(TaskErrorCode::InvalidTopology, "scoring requires a unique in-range signal role");
    }
    for (const std::uint32_t neighbor : task.topology.neighbors) {
        if (neighbor >= shape.population) {
            throw TaskError(TaskErrorCode::InvalidTopology, "scoring requires in-range neighbor indices");
        }
    }
    if (shape.sequence_length > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())
        || (shape.input_trits != 0U
            && shape.sequence_length
                > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() / shape.input_trits))
        || (shape.output_trits != 0U
            && shape.sequence_length
                > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() / shape.output_trits))) {
        throw TaskError(TaskErrorCode::BadLength, "scoring data dimensions exceed host addressable size");
    }
    const std::size_t expected_inputs = static_cast<std::size_t>(shape.sequence_length)
        * static_cast<std::size_t>(shape.input_trits);
    const std::size_t expected_outputs = static_cast<std::size_t>(shape.sequence_length)
        * static_cast<std::size_t>(shape.output_trits);
    if (task.inputs.size() != expected_inputs || task.outputs.size() != expected_outputs) {
        throw TaskError(TaskErrorCode::InvalidDomainValue, "scoring requires complete task data vectors");
    }
    for (const Trit value : task.inputs) {
        if (!is_valid_trit_byte(trit_to_byte(value))) {
            throw TaskError(TaskErrorCode::InvalidDomainValue, "scoring received an invalid input trit");
        }
    }
    for (const Trit value : task.outputs) {
        if (!is_valid_trit_byte(trit_to_byte(value))) {
            throw TaskError(TaskErrorCode::InvalidDomainValue, "scoring received an invalid output trit");
        }
    }
    if (shape.output_trits != 1U || shape.neighbors != 3U) {
        throw TaskError(TaskErrorCode::BadDimensions, "BPP9000 scoring requires one output and three neighbors");
    }
    if (config.window_width < 2U || config.window_width >= shape.sequence_length) {
        throw TaskError(TaskErrorCode::BadDimensions, "window width must be at least two and smaller than the sequence");
    }
    if (config.max_ticks <= config.window_width) {
        throw TaskError(TaskErrorCode::BadDimensions, "maximum ticks must exceed the window width");
    }
    const std::uint64_t window_count = shape.sequence_length - config.window_width;
    if (window_count > std::numeric_limits<std::uint32_t>::max()) {
        throw TaskError(TaskErrorCode::BadDimensions, "window count does not fit the score type");
    }
}

[[nodiscard]] bool is_input_neuron(const Task& task, std::uint32_t neuron)
{
    return std::find(task.topology.input_neurons.begin(), task.topology.input_neurons.end(), neuron)
        != task.topology.input_neurons.end();
}

[[nodiscard]] std::size_t checked_size_product(std::size_t left, std::size_t right, const char* description)
{
    if (left != 0U && right > std::numeric_limits<std::size_t>::max() / left) {
        throw TaskError(TaskErrorCode::BadLength, std::string(description) + " overflows");
    }
    return left * right;
}

[[nodiscard]] std::size_t padded_draw_bytes(std::size_t logical_bytes)
{
    const std::size_t remainder = logical_bytes % 64U;
    if (remainder == 0U) {
        return logical_bytes;
    }
    const std::size_t padding = 64U - remainder;
    if (logical_bytes > std::numeric_limits<std::size_t>::max() - padding) {
        throw TaskError(TaskErrorCode::BadLength, "random draw padding overflows");
    }
    return logical_bytes + padding;
}

void validate_lut_storage(const Lut& lut)
{
    for (std::size_t row = 0U; row < lut.rows(); ++row) {
        for (std::size_t entry = 0U; entry < kLutEntries; ++entry) {
            (void)lut.at_row(row, entry);
        }
    }
}

} // namespace

bool is_valid_score(const ScoreResult& result, std::uint64_t window_count) noexcept
{
    return result.status == ScoreStatus::Settled
        && result.score != kTimeoutScore
        && static_cast<std::uint64_t>(result.windows_evaluated) == window_count
        && static_cast<std::uint64_t>(result.score) <= window_count;
}

bool is_good_score(const ScoreResult& result, Threshold threshold, std::uint64_t window_count) noexcept
{
    return is_valid_score(result, window_count) && result.score <= threshold.value;
}

bool is_canonical_nonce(const Nonce& nonce) noexcept
{
    return nonce.bytes[0] == 1U && nonce.bytes[1] >= 1U && nonce.bytes[1] <= kMaxMutationsPerStep
        && nonce.bytes[2] == 0U;
}

bool is_valid_mining_seed(const MiningSeed& seed) noexcept
{
    return !seed.is_zero();
}

std::uint32_t lut_index(Trit first, Trit second, Trit third)
{
    if (!is_valid_trit_byte(trit_to_byte(first)) || !is_valid_trit_byte(trit_to_byte(second))
        || !is_valid_trit_byte(trit_to_byte(third))) {
        throw TaskError(TaskErrorCode::InvalidDomainValue, "LUT index received a value outside trits 0, 1, 2");
    }
    return static_cast<std::uint32_t>(trit_to_byte(first))
        + 3U * static_cast<std::uint32_t>(trit_to_byte(second))
        + 9U * static_cast<std::uint32_t>(trit_to_byte(third));
}

Lut::Lut(const Task& task)
{
    const std::size_t population = task.header.shape.population;
    if (population == 0U || task.topology.input_neurons.size() != task.header.shape.input_trits) {
        throw TaskError(TaskErrorCode::InvalidTopology, "cannot construct LUT for an incomplete task");
    }
    row_for_neuron_.assign(population, -1);
    for (std::size_t neuron = 0U; neuron < population; ++neuron) {
        if (!is_input_neuron(task, static_cast<std::uint32_t>(neuron))) {
            row_for_neuron_[neuron] = static_cast<std::int32_t>(updated_neurons_.size());
            updated_neurons_.push_back(static_cast<std::uint32_t>(neuron));
        }
    }
    storage_.assign(checked_size_product(updated_neurons_.size(), kLutStride, "LUT storage size"), 0U);
    fill(Trit::Unknown);
}

Trit Lut::at_row(std::size_t row, std::size_t entry) const
{
    if (row >= rows() || entry >= kLutEntries) {
        throw TaskError(TaskErrorCode::InvalidDomainValue, "LUT row or logical entry is out of range");
    }
    const Byte value = storage_[row * kLutStride + entry];
    if (!is_valid_trit_byte(value)) {
        throw TaskError(TaskErrorCode::InvalidDomainValue, "LUT storage contains an invalid trit");
    }
    return static_cast<Trit>(value);
}

Trit Lut::at_neuron(std::size_t neuron, std::size_t entry) const
{
    if (neuron >= row_for_neuron_.size() || row_for_neuron_[neuron] < 0) {
        throw TaskError(TaskErrorCode::InvalidDomainValue, "LUT lookup requested for an input neuron");
    }
    return at_row(static_cast<std::size_t>(row_for_neuron_[neuron]), entry);
}

void Lut::set_row(std::size_t row, std::size_t entry, Trit value)
{
    if (row >= rows() || entry >= kLutEntries || !is_valid_trit_byte(trit_to_byte(value))) {
        throw TaskError(TaskErrorCode::InvalidDomainValue, "invalid LUT row, entry, or trit");
    }
    storage_[row * kLutStride + entry] = trit_to_byte(value);
}

void Lut::set_neuron(std::size_t neuron, std::size_t entry, Trit value)
{
    if (neuron >= row_for_neuron_.size() || row_for_neuron_[neuron] < 0) {
        throw TaskError(TaskErrorCode::InvalidDomainValue, "cannot set the LUT of an input neuron");
    }
    set_row(static_cast<std::size_t>(row_for_neuron_[neuron]), entry, value);
}

void Lut::fill(Trit value)
{
    if (!is_valid_trit_byte(trit_to_byte(value))) {
        throw TaskError(TaskErrorCode::InvalidDomainValue, "cannot fill LUT with an invalid trit");
    }
    for (std::size_t row = 0U; row < rows(); ++row) {
        for (std::size_t entry = 0U; entry < kLutEntries; ++entry) {
            storage_[row * kLutStride + entry] = trit_to_byte(value);
        }
        for (std::size_t entry = kLutEntries; entry < kLutStride; ++entry) {
            storage_[row * kLutStride + entry] = 0U;
        }
    }
}

void Lut::initialize_from_root_bytes(std::span<const Byte> root_bytes)
{
    const std::size_t required = checked_size_product(population(), kLutEntries, "root LUT draw size");
    if (root_bytes.size() < required) {
        throw TaskError(TaskErrorCode::BadLength, "root LUT draw is shorter than the population table");
    }
    fill(Trit::Zero);
    for (std::size_t row = 0U; row < rows(); ++row) {
        const std::size_t neuron = updated_neurons_[row];
        for (std::size_t entry = 0U; entry < kLutEntries; ++entry) {
            set_row(row, entry, static_cast<Trit>(root_bytes[neuron * kLutEntries + entry] % 3U));
        }
    }
}

RecurrentState::RecurrentState(const Task& task)
    : current_(task.header.shape.population, trit_to_byte(Trit::Unknown)),
      next_(task.header.shape.population, trit_to_byte(Trit::Unknown)),
      input_mask_(task.header.shape.population, false)
{
    for (const std::uint32_t neuron : task.topology.input_neurons) {
        if (neuron >= input_mask_.size() || input_mask_[neuron]) {
            throw TaskError(TaskErrorCode::InvalidTopology, "invalid input role while creating recurrent state");
        }
        input_mask_[neuron] = true;
    }
}

void RecurrentState::reset_unknown()
{
    std::fill(current_.begin(), current_.end(), trit_to_byte(Trit::Unknown));
    std::fill(next_.begin(), next_.end(), trit_to_byte(Trit::Unknown));
}

void RecurrentState::set_input(std::size_t neuron, Trit value)
{
    if (neuron >= input_mask_.size() || !input_mask_[neuron] || !is_valid_trit_byte(trit_to_byte(value))) {
        throw TaskError(TaskErrorCode::InvalidDomainValue, "set_input requires an input neuron and a valid trit");
    }
    current_[neuron] = trit_to_byte(value);
}

void RecurrentState::set_all_inputs_unknown()
{
    for (std::size_t neuron = 0U; neuron < input_mask_.size(); ++neuron) {
        if (input_mask_[neuron]) {
            current_[neuron] = trit_to_byte(Trit::Unknown);
        }
    }
}

void RecurrentState::tick(const Task& task, const Lut& lut)
{
    const std::size_t population = task.header.shape.population;
    if (current_.size() != population || lut.population() != population) {
        throw TaskError(TaskErrorCode::InvalidTopology, "state and LUT population sizes do not match");
    }
    for (std::size_t neuron = 0U; neuron < population; ++neuron) {
        if (input_mask_[neuron]) {
            next_[neuron] = current_[neuron];
            continue;
        }
        const std::size_t neighbor_base = neuron * task.header.shape.neighbors;
        const Trit first = static_cast<Trit>(current_[task.topology.neighbors[neighbor_base]]);
        const Trit second = static_cast<Trit>(current_[task.topology.neighbors[neighbor_base + 1U]]);
        const Trit third = static_cast<Trit>(current_[task.topology.neighbors[neighbor_base + 2U]]);
        next_[neuron] = trit_to_byte(lut.at_neuron(neuron, lut_index(first, second, third)));
    }
    current_.swap(next_);
}

WindowResult score_window(const Task& task,
                          const Lut& lut,
                          std::uint64_t window_start,
                          const ReferenceConfig& config)
{
    require_scorable_task(task, config);
    const std::uint64_t window_count = task.header.shape.sequence_length - config.window_width;
    if (window_start >= window_count) {
        throw TaskError(TaskErrorCode::BadDimensions, "window start is outside the task");
    }
    if (lut.population() != task.header.shape.population) {
        throw TaskError(TaskErrorCode::InvalidTopology, "LUT population does not match task");
    }
    validate_lut_storage(lut);

    RecurrentState state(task);
    state.reset_unknown();
    std::uint64_t feed_count = 0U;
    bool settled = false;
    std::uint32_t ticks = 0U;
    for (; ticks < config.max_ticks; ++ticks) {
        if (state.current()[task.topology.signal_neuron] == trit_to_byte(Trit::Unknown)) {
            if (feed_count >= config.window_width) {
                settled = true;
                break;
            }
            const std::size_t row = static_cast<std::size_t>(window_start + feed_count);
            const std::size_t input_base = row * task.header.shape.input_trits;
            for (std::size_t index = 0U; index < task.header.shape.input_trits; ++index) {
                state.set_input(task.topology.input_neurons[index], task.inputs[input_base + index]);
            }
            ++feed_count;
        } else {
            state.set_all_inputs_unknown();
        }
        state.tick(task, lut);
    }

    WindowResult result;
    result.feed_count = static_cast<std::uint32_t>(feed_count);
    result.score.windows_evaluated = 1U;
    result.score.ticks = ticks;
    if (!settled) {
        result.score.score = kTimeoutScore;
        result.score.status = ScoreStatus::Timeout;
        result.predicted = Trit::Unknown;
        result.expected = Trit::Unknown;
        return result;
    }

    result.predicted = static_cast<Trit>(state.current()[task.topology.output_neurons[0]]);
    const std::size_t expected_row = static_cast<std::size_t>(window_start + feed_count);
    result.expected = task.outputs[expected_row * task.header.shape.output_trits];
    result.score.score = (result.predicted == result.expected) ? 0U : 1U;
    result.score.status = ScoreStatus::Settled;
    return result;
}

ScoreResult score_lut(const Task& task, const Lut& lut, const ReferenceConfig& config)
{
    require_scorable_task(task, config);
    const std::uint64_t window_count = task.header.shape.sequence_length - config.window_width;
    ScoreResult total;
    total.score = 0U;
    total.status = ScoreStatus::Settled;
    total.windows_evaluated = 0U;
    total.ticks = 0U;
    for (std::uint64_t window = 0U; window < window_count; ++window) {
        const WindowResult current = score_window(task, lut, window, config);
        total.windows_evaluated = static_cast<std::uint32_t>(window + 1U);
        total.ticks = current.score.ticks;
        if (current.score.timed_out()) {
            total.score = kTimeoutScore;
            total.status = ScoreStatus::Timeout;
            return total;
        }
        total.score += current.score.score;
    }
    return total;
}

MutationRecord mutate_lut(Lut& lut, std::uint64_t mutation_seed)
{
    if (lut.rows() == 0U) {
        throw TaskError(TaskErrorCode::InvalidTopology, "cannot mutate an empty LUT");
    }
    const std::size_t total_entries = checked_size_product(lut.rows(), kLutEntries, "mutation entry count");
    const std::size_t flat = static_cast<std::size_t>((mutation_seed >> 1U) % total_entries);
    const std::size_t row = flat / kLutEntries;
    const std::size_t entry = flat % kLutEntries;
    const Trit old_value = lut.at_row(row, entry);
    const Byte delta = static_cast<Byte>(mutation_seed & 1U);
    const Byte new_byte = static_cast<Byte>((trit_to_byte(old_value) + 1U + delta) % 3U);
    const Trit new_value = static_cast<Trit>(new_byte);
    lut.set_row(row, entry, new_value);
    return MutationRecord{row, lut.updated_neurons()[row], entry, old_value, new_value};
}

void rollback_mutation(Lut& lut, const MutationRecord& record)
{
    if (record.row >= lut.rows() || record.entry >= kLutEntries
        || lut.updated_neurons()[record.row] != record.neuron) {
        throw TaskError(TaskErrorCode::InvalidDomainValue, "mutation record does not belong to this LUT");
    }
    if (lut.at_row(record.row, record.entry) != record.new_value) {
        throw TaskError(TaskErrorCode::InvalidDomainValue, "mutation rollback order or current value is invalid");
    }
    lut.set_row(record.row, record.entry, record.old_value);
}

CandidateResult score_candidate(const Task& task,
                                const PublicKey& public_key,
                                const MiningSeed& mining_seed,
                                const Nonce& nonce,
                                const CandidateRandomSource& random_source,
                                const ReferenceConfig& config)
{
    require_scorable_task(task, config);
    if (!is_canonical_nonce(nonce)) {
        throw TaskError(TaskErrorCode::InvalidDomainValue, "nonce is not canonical BPP9000 work");
    }
    if (!is_valid_mining_seed(mining_seed)) {
        throw TaskError(TaskErrorCode::InvalidDomainValue, "mining seed is zero or unavailable");
    }
    if (config.mutation_steps > std::numeric_limits<std::size_t>::max() / kMaxMutationsPerStep) {
        throw TaskError(TaskErrorCode::BadLength, "mutation step count is too large");
    }

    const std::size_t root_logical_bytes = checked_size_product(task.header.shape.population, kLutEntries, "root LUT bytes");
    const std::size_t root_bytes_count = padded_draw_bytes(root_logical_bytes);
    std::vector<Byte> root_bytes(root_bytes_count, 0U);
    random_source.fill_root_lut(mining_seed, public_key, root_bytes);

    const std::size_t mutation_logical_bytes = checked_size_product(
        checked_size_product(static_cast<std::size_t>(config.mutation_steps), kMaxMutationsPerStep, "mutation count"),
        sizeof(std::uint64_t),
        "mutation draw bytes");
    const std::size_t mutation_count = padded_draw_bytes(mutation_logical_bytes) / sizeof(std::uint64_t);
    std::vector<std::uint64_t> mutation_words(mutation_count, 0U);
    random_source.fill_mutation_words(mining_seed, public_key, nonce, mutation_words);

    Lut current(task);
    current.initialize_from_root_bytes(root_bytes);
    const ScoreResult initial = score_lut(task, current, config);
    CandidateResult result{initial, initial, current, current, 1U, {}};
    result.attempts.reserve(config.mutation_steps);

    ScoreResult current_score = initial;
    for (std::uint32_t step = 0U; step < config.mutation_steps; ++step) {
        Lut before = current;
        MutationAttempt attempt;
        attempt.mutations.reserve(nonce.bytes[1]);
        const std::size_t word_base = static_cast<std::size_t>(step) * kMaxMutationsPerStep;
        for (std::size_t mutation = 0U; mutation < nonce.bytes[1]; ++mutation) {
            attempt.mutations.push_back(mutate_lut(current, mutation_words[word_base + mutation]));
        }
        attempt.measured = score_lut(task, current, config);
        ++result.score_calls;

        attempt.accepted = attempt.measured.score <= current_score.score;
        if (attempt.accepted) {
            current_score = attempt.measured;
        } else {
            for (auto iterator = attempt.mutations.rbegin(); iterator != attempt.mutations.rend(); ++iterator) {
                rollback_mutation(current, *iterator);
            }
        }
        if (current_score.score < result.best.score) {
            result.best = current_score;
            result.best_lut = current;
        }
        result.attempts.push_back(std::move(attempt));
    }
    result.current_lut = current;
    return result;
}

} // namespace xdna::bpp9000
