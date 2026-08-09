#include "bpp9000/reference.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace xdna::bpp9000;

class TestFailure final : public std::runtime_error {
public:
    explicit TestFailure(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

std::size_t assertion_count = 0U;

void expect(bool condition, const std::string& message)
{
    ++assertion_count;
    if (!condition) {
        throw TestFailure(message);
    }
}

template <typename Function>
void expect_task_error(TaskErrorCode code, Function&& function, const std::string& message)
{
    ++assertion_count;
    try {
        function();
    } catch (const TaskError& error) {
        if (error.code() != code) {
            throw TestFailure(message + " (wrong error code)");
        }
        return;
    }
    throw TestFailure(message + " (no TaskError)");
}

[[nodiscard]] std::uint64_t next_fixture_word(std::uint64_t& state)
{
    state += 0x9E3779B97F4A7C15ULL;
    std::uint64_t value = state;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
}

[[nodiscard]] TaskShape small_shape()
{
    return TaskShape{3U, 1U, 8U, 8U, 3U};
}

[[nodiscard]] ReferenceConfig small_config()
{
    return ReferenceConfig{2U, 8U, kProductionMutationSteps};
}

[[nodiscard]] std::string hex(Digest32 digest)
{
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (const Byte value : digest.bytes) {
        stream << std::setw(2) << static_cast<unsigned int>(value);
    }
    return stream.str();
}

void append_u32(std::vector<Byte>& bytes, std::uint32_t value)
{
    for (std::size_t i = 0U; i < 4U; ++i) {
        bytes.push_back(static_cast<Byte>(value >> (8U * i)));
    }
}

[[nodiscard]] Task make_fixture_task(const TaskShape& shape,
                                     std::uint64_t seed,
                                     unsigned int output_mode,
                                     const BlockDigestProvider& digest)
{
    Task task;
    task.header.shape = shape;
    task.topology.input_neurons.resize(shape.input_trits);
    for (std::uint32_t index = 0U; index < shape.input_trits; ++index) {
        task.topology.input_neurons[index] = index;
    }
    task.topology.output_neurons = {shape.input_trits};
    task.topology.signal_neuron = shape.input_trits + 1U;
    task.topology.neighbors.resize(
        static_cast<std::size_t>(shape.population) * static_cast<std::size_t>(shape.neighbors),
        task.topology.signal_neuron);

    const std::size_t input_count = static_cast<std::size_t>(shape.sequence_length)
        * static_cast<std::size_t>(shape.input_trits);
    const std::size_t output_count = static_cast<std::size_t>(shape.sequence_length)
        * static_cast<std::size_t>(shape.output_trits);
    task.inputs.resize(input_count, Trit::Zero);
    task.outputs.resize(output_count, Trit::Unknown);
    std::uint64_t state = seed;
    for (std::size_t row = 0U; row < static_cast<std::size_t>(shape.sequence_length); ++row) {
        for (std::size_t column = 0U; column < shape.input_trits; ++column) {
            task.inputs[row * shape.input_trits + column]
                = static_cast<Trit>(next_fixture_word(state) % 3U);
        }
        if (output_mode == 1U) {
            task.outputs[row] = Trit::Zero;
        } else if (output_mode == 2U) {
            task.outputs[row] = static_cast<Trit>((seed + row) % 3U);
        }
    }

    const std::vector<Byte> topology_block = serialize_topology(shape, task.topology);
    const std::vector<Byte> data_block = serialize_data(shape, task.inputs, task.outputs);
    task.header.topology_hash = digest.digest(topology_block);
    task.header.data_hash = digest.digest(data_block);
    task.packed_topology = topology_block;
    task.packed_data = data_block;
    return task;
}

[[nodiscard]] Task parse_fixture(const Task& source,
                                 const BlockDigestProvider& digest,
                                 bool require_expected_production_shape = false)
{
    const std::vector<Byte> bytes = serialize_task(source);
    if (require_expected_production_shape) {
        return parse_production_task(bytes, digest);
    }
    TaskParseOptions options;
    options.digest_provider = &digest;
    return parse_task(bytes, options);
}

[[nodiscard]] TaskParseOptions digest_options(const BlockDigestProvider& digest)
{
    TaskParseOptions options;
    options.digest_provider = &digest;
    return options;
}

[[nodiscard]] std::vector<Byte> fixture_bytes(const Task& source)
{
    return serialize_task(source);
}

void rewrite_u32(std::vector<Byte>& bytes, std::size_t offset, std::uint32_t value)
{
    for (std::size_t i = 0U; i < 4U; ++i) {
        bytes[offset + i] = static_cast<Byte>(value >> (8U * i));
    }
}

void rewrite_digest(std::vector<Byte>& bytes, std::size_t offset, Digest32 digest)
{
    std::copy(digest.bytes.begin(), digest.bytes.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset));
}

[[nodiscard]] std::vector<Byte> with_recomputed_block_digest(std::vector<Byte> bytes,
                                                             const TaskShape& shape,
                                                             const BlockDigestProvider& digest,
                                                             bool topology_changed)
{
    const std::size_t topology_offset = kTaskHeaderBytes;
    const std::size_t data_offset = topology_offset + topology_bytes_for_shape(shape);
    if (topology_changed) {
        rewrite_digest(bytes,
                       32U,
                       digest.digest(std::span<const Byte>(bytes).subspan(topology_offset, topology_bytes_for_shape(shape))));
    } else {
        rewrite_digest(bytes,
                       64U,
                       digest.digest(std::span<const Byte>(bytes).subspan(data_offset, data_bytes_for_shape(shape))));
    }
    return bytes;
}

[[nodiscard]] Digest32 summary_digest(const std::vector<Byte>& bytes)
{
    DeterministicFixtureDigest digest(0xC0FFEE5EED000001ULL);
    return digest.digest(bytes);
}

void test_parser_and_serialization()
{
    DeterministicFixtureDigest digest;
    const Task original = make_fixture_task(small_shape(), 7U, 2U, digest);
    const std::vector<Byte> bytes = fixture_bytes(original);
    expect(bytes.size() == 96U + 116U + 16U, "small task serialized length");
    expect(bytes[0] == 0x4CU && bytes[1] == 0x55U && bytes[2] == 0x54U && bytes[3] == 0x54U,
           "magic is encoded little endian");
    expect(bytes[16] == 8U && bytes[17] == 0U && bytes[18] == 0U && bytes[19] == 0U,
           "uint64 sequence length starts in little endian order");

    const Task parsed = parse_fixture(original, digest);
    expect(parsed.header.shape == original.header.shape, "header shape survives parsing");
    expect(parsed.topology.neighbors == original.topology.neighbors, "topology survives parsing");
    expect(parsed.inputs == original.inputs && parsed.outputs == original.outputs, "data survives parsing");
    expect(serialize_task(parsed) == bytes, "canonical parse/serialize is byte-identical");

    std::vector<Byte> wrong_magic = bytes;
    rewrite_u32(wrong_magic, 0U, 0U);
    expect_task_error(TaskErrorCode::BadMagic,
                      [&] { (void)parse_task(wrong_magic, digest_options(digest)); },
                      "wrong magic is rejected");

    std::vector<Byte> wrong_version = bytes;
    rewrite_u32(wrong_version, 4U, 2U);
    expect_task_error(TaskErrorCode::BadVersion,
                      [&] { (void)parse_task(wrong_version, digest_options(digest)); },
                      "wrong version is rejected");

    TaskParseOptions production_options{&digest, production_shape(), true};
    expect_task_error(TaskErrorCode::BadDimensions,
                      [&] { (void)parse_task(bytes, production_options); },
                      "unexpected dimensions are rejected when production shape is required");
    expect_task_error(TaskErrorCode::MissingDigestProvider,
                      [&] { (void)parse_task(bytes); },
                      "hash metadata without a provider is rejected");
}

void test_parser_fail_closed()
{
    DeterministicFixtureDigest digest;
    const Task original = make_fixture_task(small_shape(), 19U, 0U, digest);
    const std::vector<Byte> bytes = fixture_bytes(original);

    std::vector<Byte> truncated_header(bytes.begin(), bytes.begin() + 95);
    expect_task_error(TaskErrorCode::Truncated,
                      [&] { (void)parse_task(truncated_header, digest_options(digest)); },
                      "truncated header is rejected");

    std::vector<Byte> truncated_data(bytes.begin(), bytes.end() - 1);
    expect_task_error(TaskErrorCode::Truncated,
                      [&] { (void)parse_task(truncated_data, digest_options(digest)); },
                      "truncated data is rejected");

    std::vector<Byte> trailing = bytes;
    trailing.push_back(0U);
    expect_task_error(TaskErrorCode::TrailingBytes,
                      [&] { (void)parse_task(trailing, digest_options(digest)); },
                      "trailing data is rejected");

    const std::size_t topology_size = topology_bytes_for_shape(small_shape());
    const std::size_t data_offset = kTaskHeaderBytes + topology_size;
    std::vector<Byte> bad_trit = bytes;
    bad_trit[data_offset] = 243U;
    bad_trit = with_recomputed_block_digest(std::move(bad_trit), small_shape(), digest, false);
    expect_task_error(TaskErrorCode::InvalidPackedTrit,
                      [&] { (void)parse_task(bad_trit, digest_options(digest)); },
                      "packed byte 243 is rejected");

    std::vector<Byte> bad_neighbor = bytes;
    rewrite_u32(bad_neighbor, kTaskHeaderBytes + 4U * (3U + 1U), 8U);
    bad_neighbor = with_recomputed_block_digest(std::move(bad_neighbor), small_shape(), digest, true);
    expect_task_error(TaskErrorCode::InvalidTopology,
                      [&] { (void)parse_task(bad_neighbor, digest_options(digest)); },
                      "out-of-range neighbor is rejected");

    std::vector<Byte> bad_role = bytes;
    rewrite_u32(bad_role, kTaskHeaderBytes + 3U * 4U, 0U);
    bad_role = with_recomputed_block_digest(std::move(bad_role), small_shape(), digest, true);
    expect_task_error(TaskErrorCode::InvalidTopology,
                      [&] { (void)parse_task(bad_role, digest_options(digest)); },
                      "duplicate role index is rejected");

    std::vector<Byte> wrong_hash = bytes;
    wrong_hash[32U] ^= 0x01U;
    expect_task_error(TaskErrorCode::HashMismatch,
                      [&] { (void)parse_task(wrong_hash, digest_options(digest)); },
                      "hash metadata mismatch is rejected");
}

void test_trits_and_lut_index()
{
    const std::array<Trit, 8U> values{
        Trit::Zero, Trit::One, Trit::Unknown, Trit::One, Trit::Zero, Trit::Unknown, Trit::Zero, Trit::One};
    const std::vector<Byte> packed = pack_trits(values);
    const std::vector<Trit> unpacked = unpack_trits(packed, values.size());
    expect(std::equal(values.begin(), values.end(), unpacked.begin()), "trit pack/unpack round trip");
    expect(packed.size() == 2U, "five-trit packing uses the expected byte count");
    expect(packed[1] < 27U, "unused positions in the final packed byte are serialized as zero");

    expect_task_error(TaskErrorCode::InvalidPackedTrit,
                      [&] { (void)unpack_trits(std::array<Byte, 1U>{243U}, 1U); },
                      "invalid base-243 byte is rejected by unpacker");
    expect_task_error(TaskErrorCode::InvalidDomainValue,
                      [&] { (void)pack_trits(std::array<Trit, 1U>{static_cast<Trit>(3U)}); },
                      "invalid domain trit is rejected by packer");

    std::array<bool, kLutEntries> seen{};
    for (Byte first = 0U; first < 3U; ++first) {
        for (Byte second = 0U; second < 3U; ++second) {
            for (Byte third = 0U; third < 3U; ++third) {
                const std::uint32_t index = lut_index(static_cast<Trit>(first),
                                                       static_cast<Trit>(second),
                                                       static_cast<Trit>(third));
                expect(index < kLutEntries, "three-trit LUT index is in range");
                expect(!seen[index], "three-trit LUT index is unique");
                seen[index] = true;
            }
        }
    }
    expect(std::all_of(seen.begin(), seen.end(), [](bool value) { return value; }),
           "all 27 LUT combinations are addressable");
    expect(lut_index(Trit::Zero, Trit::Zero, Trit::Zero) == 0U, "LUT lower boundary is zero");
    expect(lut_index(Trit::Unknown, Trit::Unknown, Trit::Unknown) == 26U, "LUT upper boundary is 26");

    DeterministicFixtureDigest digest;
    const Task task = make_fixture_task(small_shape(), 29U, 0U, digest);
    Lut lut(task);
    expect_task_error(TaskErrorCode::InvalidDomainValue,
                      [&] { (void)lut.at_row(0U, 27U); },
                      "logical LUT entry 27 is rejected");
    lut.storage()[0U] = 3U;
    expect_task_error(TaskErrorCode::InvalidDomainValue,
                      [&] { (void)score_lut(task, lut, small_config()); },
                      "invalid LUT trit is rejected before scoring");
}

void test_recurrent_double_buffer()
{
    DeterministicFixtureDigest digest;
    Task task = make_fixture_task(small_shape(), 23U, 0U, digest);
    const std::uint32_t output = task.topology.output_neurons[0];
    const std::uint32_t signal = task.topology.signal_neuron;
    for (std::size_t k = 0U; k < 3U; ++k) {
        task.topology.neighbors[static_cast<std::size_t>(output) * 3U + k] = static_cast<std::uint32_t>(k);
        task.topology.neighbors[static_cast<std::size_t>(signal) * 3U + k] = signal;
    }
    const std::uint32_t later_neuron = 5U;
    for (std::size_t k = 0U; k < 3U; ++k) {
        task.topology.neighbors[static_cast<std::size_t>(later_neuron) * 3U + k] = output;
    }
    Lut lut(task);
    lut.fill(Trit::Zero);
    lut.set_neuron(output, 21U, Trit::One);
    lut.set_neuron(later_neuron, 26U, Trit::Zero);
    RecurrentState state(task);
    state.reset_unknown();
    state.set_input(0U, Trit::Zero);
    state.set_input(1U, Trit::One);
    state.set_input(2U, Trit::Unknown);
    state.tick(task, lut);

    expect(static_cast<Trit>(state.current()[output]) == Trit::One, "output uses the three prior-tick inputs");
    expect(static_cast<Trit>(state.current()[later_neuron]) == Trit::Zero,
           "later neuron reads the old output, not the just-written output");
    expect(static_cast<Trit>(state.current()[0U]) == Trit::Zero, "input state survives the buffer swap");
    state.set_all_inputs_unknown();
    state.tick(task, lut);
    expect(static_cast<Trit>(state.current()[0U]) == Trit::Unknown, "input reset is explicit");
}

void test_score_and_thresholds()
{
    DeterministicFixtureDigest digest;
    const ReferenceConfig config = small_config();
    const Task all_unknown = make_fixture_task(small_shape(), 31U, 0U, digest);
    Lut unknown_lut(all_unknown);
    unknown_lut.fill(Trit::Unknown);
    const ScoreResult zero_score = score_lut(all_unknown, unknown_lut, config);
    expect(zero_score.score == 0U && zero_score.windows_evaluated == 6U, "zero score is counted across all windows");
    expect(is_valid_score(zero_score, 6U), "zero score is valid");
    expect(is_good_score(zero_score, Threshold{0U}, 6U), "score equal to threshold is good");
    expect(!is_good_score(zero_score, Threshold{std::numeric_limits<std::uint32_t>::max()}, 0U),
           "window-count bound is enforced by score predicate");

    const Task all_zero = make_fixture_task(small_shape(), 32U, 1U, digest);
    Lut all_zero_lut(all_zero);
    all_zero_lut.fill(Trit::Unknown);
    const ScoreResult maximum_score = score_lut(all_zero, all_zero_lut, config);
    expect(maximum_score.score == 6U && is_valid_score(maximum_score, 6U), "maximum finite score is exact");
    expect(!is_good_score(maximum_score, Threshold{5U}, 6U), "score above threshold is rejected");
    expect(is_good_score(maximum_score, Threshold{6U}, 6U), "score equal to upper threshold is accepted");

    const Task timeout_task = make_fixture_task(small_shape(), 33U, 0U, digest);
    Lut timeout_lut(timeout_task);
    timeout_lut.fill(Trit::Zero);
    const ScoreResult timeout = score_lut(timeout_task, timeout_lut, config);
    expect(timeout.score == kTimeoutScore && timeout.timed_out(), "non-settling signal returns timeout sentinel");
    expect(!is_valid_score(timeout, 6U), "timeout is never a valid score");
    const WindowResult one_window = score_window(all_unknown, unknown_lut, 0U, config);
    expect(one_window.feed_count == 2U && one_window.score.score == 0U, "one-window feed and target indexing are exact");
}

void test_nonce_and_mutation()
{
    Nonce valid{};
    valid.bytes[0] = 1U;
    valid.bytes[1] = 1U;
    expect(is_canonical_nonce(valid), "canonical nonce is accepted");
    valid.bytes[1] = 10U;
    expect(is_canonical_nonce(valid), "maximum L nonce is accepted");
    valid.bytes[0] = 0U;
    expect(!is_canonical_nonce(valid), "wrong algorithm byte is rejected");
    valid.bytes[0] = 1U;
    valid.bytes[1] = 0U;
    expect(!is_canonical_nonce(valid), "zero L is rejected");
    valid.bytes[1] = 11U;
    expect(!is_canonical_nonce(valid), "L above ten is rejected");
    valid.bytes[1] = 1U;
    valid.bytes[2] = 1U;
    expect(!is_canonical_nonce(valid), "nonzero K field is rejected");

    MiningSeed zero_seed{};
    expect(!is_valid_mining_seed(zero_seed), "zero mining seed is rejected");
    zero_seed.bytes[31] = 1U;
    expect(is_valid_mining_seed(zero_seed), "nonzero mining seed is accepted");

    DeterministicFixtureDigest digest;
    const Task task = make_fixture_task(small_shape(), 41U, 0U, digest);
    Lut lut(task);
    lut.fill(Trit::Zero);
    const MutationRecord first = mutate_lut(lut, 0U);
    expect(first.row == 0U && first.entry == 0U && first.old_value == Trit::Zero && first.new_value == Trit::One,
           "mutation from zero with delta zero is exact");
    rollback_mutation(lut, first);
    expect(lut.at_row(0U, 0U) == Trit::Zero, "mutation rollback restores zero");

    lut.set_row(0U, 1U, Trit::One);
    const MutationRecord second = mutate_lut(lut, 2U);
    expect(second.entry == 1U && second.old_value == Trit::One && second.new_value == Trit::Unknown,
           "mutation from one is exact");
    rollback_mutation(lut, second);
    lut.set_row(0U, 2U, Trit::Unknown);
    const MutationRecord third = mutate_lut(lut, 4U);
    expect(third.entry == 2U && third.old_value == Trit::Unknown && third.new_value == Trit::Zero,
           "mutation from unknown is exact");
    rollback_mutation(lut, third);

    const MutationRecord delta_one = mutate_lut(lut, 1U);
    expect(delta_one.new_value == Trit::Unknown, "mutation delta bit selects the other replacement");
    rollback_mutation(lut, delta_one);
    const MutationRecord repeated_a = mutate_lut(lut, 0U);
    const MutationRecord repeated_b = mutate_lut(lut, 0U);
    rollback_mutation(lut, repeated_b);
    rollback_mutation(lut, repeated_a);
    expect(lut.at_row(0U, 0U) == Trit::Zero, "repeated mutations roll back in reverse order");
}

void test_random_boundary_and_candidate_search()
{
    DeterministicFixtureDigest digest;
    const Task task = make_fixture_task(small_shape(), 53U, 2U, digest);
    const ReferenceConfig config = small_config();
    PublicKey public_key{};
    public_key.bytes[0] = 7U;
    MiningSeed mining_seed{};
    mining_seed.bytes[0] = 9U;
    Nonce nonce{};
    nonce.bytes[0] = 1U;
    nonce.bytes[1] = 2U;
    nonce.bytes[2] = 0U;

    DeterministicFixtureRandom random_a(0x12345678U);
    const std::size_t logical_root_bytes = task.header.shape.population * kLutEntries;
    const std::size_t padded_root_bytes = ((logical_root_bytes + 63U) / 64U) * 64U;
    const std::size_t logical_mutation_bytes
        = static_cast<std::size_t>(config.mutation_steps) * kMaxMutationsPerStep * sizeof(std::uint64_t);
    const std::size_t padded_mutation_words = ((logical_mutation_bytes + 63U) / 64U) * 64U / sizeof(std::uint64_t);
    std::vector<Byte> root_a(padded_root_bytes, 0U);
    std::vector<std::uint64_t> words_a(padded_mutation_words, 0U);
    random_a.fill_root_lut(mining_seed, public_key, root_a);
    random_a.fill_mutation_words(mining_seed, public_key, nonce, words_a);
    expect(random_a.events().size() == 2U, "random draw boundary records both draw calls");
    expect(random_a.events()[0].byte_count == root_a.size(), "root draw size is explicit");
    expect(random_a.events()[1].byte_count == words_a.size() * sizeof(std::uint64_t),
           "mutation draw size is explicit");

    DeterministicFixtureRandom random_b(0x12345678U);
    std::vector<Byte> root_b(root_a.size(), 0U);
    std::vector<std::uint64_t> words_b(words_a.size(), 0U);
    random_b.fill_root_lut(mining_seed, public_key, root_b);
    random_b.fill_mutation_words(mining_seed, public_key, nonce, words_b);
    expect(root_a == root_b && words_a == words_b, "fixture random source is repeatable");

    const CandidateResult first = score_candidate(task, public_key, mining_seed, nonce, random_a, config);
    const CandidateResult second = score_candidate(task, public_key, mining_seed, nonce, random_b, config);
    expect(first.score_calls == 101U && first.attempts.size() == 100U, "full search performs 101 score calls");
    expect(first.best.score == second.best.score && first.current_lut.storage() == second.current_lut.storage(),
           "candidate search is deterministic");
    expect(std::all_of(first.attempts.begin(), first.attempts.end(), [&](const MutationAttempt& attempt) {
        return attempt.mutations.size() == nonce.bytes[1];
    }), "each mutation step applies exactly L changes");

    Nonce invalid = nonce;
    invalid.bytes[0] = 0U;
    expect_task_error(TaskErrorCode::InvalidDomainValue,
                      [&] { (void)score_candidate(task, public_key, mining_seed, invalid, random_b, config); },
                      "candidate scorer rejects noncanonical nonce");
    MiningSeed zero_seed{};
    expect_task_error(TaskErrorCode::InvalidDomainValue,
                      [&] { (void)score_candidate(task, public_key, zero_seed, nonce, random_b, config); },
                      "candidate scorer rejects zero mining seed");
}

struct CorpusResult {
    Digest32 generated_digest{};
    Digest32 production_digest{};
};

[[nodiscard]] CorpusResult run_corpus(bool print_progress)
{
    DeterministicFixtureDigest fixture_digest(0xC0DEC0DEC0DEC0DEULL);
    const ReferenceConfig config = small_config();
    std::vector<Byte> generated_summary;
    generated_summary.reserve(100U * 16U);
    PublicKey public_key{};
    public_key.bytes[0] = 0xA5U;
    MiningSeed seed{};
    seed.bytes[0] = 0x5AU;
    Nonce nonce{};
    nonce.bytes[0] = 1U;
    nonce.bytes[2] = 0U;
    for (std::uint32_t case_id = 0U; case_id < 100U; ++case_id) {
        const Task fixture = make_fixture_task(small_shape(), 0x1000U + case_id, case_id % 3U, fixture_digest);
        const Task parsed = parse_fixture(fixture, fixture_digest);
        nonce.bytes[1] = static_cast<Byte>((case_id % 10U) + 1U);
        seed.bytes[1] = static_cast<Byte>(case_id + 1U);
        DeterministicFixtureRandom first_random(0x90000000ULL + case_id);
        DeterministicFixtureRandom second_random(0x90000000ULL + case_id);
        const CandidateResult first = score_candidate(parsed, public_key, seed, nonce, first_random, config);
        const CandidateResult second = score_candidate(parsed, public_key, seed, nonce, second_random, config);
        expect(first.best.score == second.best.score && first.current_lut.storage() == second.current_lut.storage(),
               "generated corpus case is repeatable");
        expect(first.score_calls == 101U, "generated corpus case has 101 score calls");
        append_u32(generated_summary, case_id);
        append_u32(generated_summary, first.best.score);
        append_u32(generated_summary, first.initial.score);
        append_u32(generated_summary, first.score_calls);
        if (print_progress && case_id == 0U) {
            std::cout << "generated_cases=100\n";
        }
    }

    std::vector<Byte> production_summary;
    production_summary.reserve(10U * 16U);
    const ReferenceConfig production_config{};
    for (std::uint32_t case_id = 0U; case_id < 10U; ++case_id) {
        const Task fixture = make_fixture_task(production_shape(), 0x5000U + case_id, 0U, fixture_digest);
        const Task parsed = parse_fixture(fixture, fixture_digest, true);
        expect(parsed.header.shape == production_shape(), "production-shaped corpus dimensions are exact");
        expect(serialize_task(parsed).size() == 44744U, "production-shaped corpus serialized length is exact");
        Lut lut(parsed);
        lut.fill(Trit::Unknown);
        const WindowResult result = score_window(parsed, lut, case_id, production_config);
        expect(result.score.status == ScoreStatus::Settled && result.score.score == 0U,
               "production-shaped corpus window settles deterministically");
        expect(result.feed_count == kProductionWindowWidth, "production-shaped corpus feeds the full production window");
        append_u32(production_summary, case_id);
        append_u32(production_summary, result.score.score);
        append_u32(production_summary, result.feed_count);
        append_u32(production_summary, result.score.ticks);
    }
    if (print_progress) {
        std::cout << "production_shaped_cases=10\n";
    }
    return CorpusResult{summary_digest(generated_summary), summary_digest(production_summary)};
}

void test_deterministic_corpus()
{
    const CorpusResult result = run_corpus(false);
    std::cout << "generated_digest=" << hex(result.generated_digest) << '\n';
    std::cout << "production_digest=" << hex(result.production_digest) << '\n';
    // Filled after the first independent generation run; these values are the
    // committed corpus manifest for generator version m1-v1.
    expect(hex(result.generated_digest) == "2979889feed3352b3c12831a301a357b6c9099f3de80b955f152c53bca2f8c03",
           "generated corpus manifest digest");
    expect(hex(result.production_digest) == "7c1da1028b9ecdbae54616654606185e62076ff7b69e209ecbf3d23f6a2fede1",
           "production corpus manifest digest");
}

void run_test(std::string_view name, const std::function<void()>& function, std::size_t& passed, std::size_t& failed)
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

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "--corpus") {
        const CorpusResult result = run_corpus(true);
        std::cout << "generator_version=m1-v1\n";
        std::cout << "generated_digest=" << hex(result.generated_digest) << '\n';
        std::cout << "production_digest=" << hex(result.production_digest) << '\n';
        return 0;
    }

    std::size_t passed = 0U;
    std::size_t failed = 0U;
    run_test("parser_and_serialization", test_parser_and_serialization, passed, failed);
    run_test("parser_fail_closed", test_parser_fail_closed, passed, failed);
    run_test("trits_and_lut_index", test_trits_and_lut_index, passed, failed);
    run_test("recurrent_double_buffer", test_recurrent_double_buffer, passed, failed);
    run_test("score_and_thresholds", test_score_and_thresholds, passed, failed);
    run_test("nonce_and_mutation", test_nonce_and_mutation, passed, failed);
    run_test("random_boundary_and_candidate_search", test_random_boundary_and_candidate_search, passed, failed);
    run_test("deterministic_corpus", test_deterministic_corpus, passed, failed);
    std::cout << "tests_passed=" << passed << " tests_failed=" << failed << " assertions=" << assertion_count << '\n';
    return failed == 0U ? 0 : 1;
}
