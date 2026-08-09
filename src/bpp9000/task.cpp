#include "bpp9000/task.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <string_view>

namespace xdna::bpp9000 {
namespace {

[[nodiscard]] std::size_t checked_to_size(std::uint64_t value, std::string_view what)
{
    if (value > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw TaskError(TaskErrorCode::BadLength, std::string(what) + " exceeds host addressable size");
    }
    return static_cast<std::size_t>(value);
}

[[nodiscard]] std::uint64_t checked_add(std::uint64_t left, std::uint64_t right, std::string_view what)
{
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        throw TaskError(TaskErrorCode::BadLength, std::string(what) + " overflows");
    }
    return left + right;
}

[[nodiscard]] std::uint64_t checked_multiply(std::uint64_t left, std::uint64_t right, std::string_view what)
{
    if (left != 0U && right > std::numeric_limits<std::uint64_t>::max() / left) {
        throw TaskError(TaskErrorCode::BadLength, std::string(what) + " overflows");
    }
    return left * right;
}

[[nodiscard]] std::uint32_t read_u32(std::span<const Byte> bytes, std::size_t offset)
{
    if (offset > bytes.size() || bytes.size() - offset < 4U) {
        throw TaskError(TaskErrorCode::Truncated, "truncated little-endian uint32 field");
    }
    return static_cast<std::uint32_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U)
        | (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
}

[[nodiscard]] std::uint64_t read_u64(std::span<const Byte> bytes, std::size_t offset)
{
    if (offset > bytes.size() || bytes.size() - offset < 8U) {
        throw TaskError(TaskErrorCode::Truncated, "truncated little-endian uint64 field");
    }
    std::uint64_t value = 0U;
    for (std::size_t i = 0U; i < 8U; ++i) {
        value |= static_cast<std::uint64_t>(bytes[offset + i]) << (8U * i);
    }
    return value;
}

void write_u32(std::vector<Byte>& bytes, std::uint32_t value)
{
    for (std::size_t i = 0U; i < 4U; ++i) {
        bytes.push_back(static_cast<Byte>(value >> (8U * i)));
    }
}

void write_u64(std::vector<Byte>& bytes, std::uint64_t value)
{
    for (std::size_t i = 0U; i < 8U; ++i) {
        bytes.push_back(static_cast<Byte>(value >> (8U * i)));
    }
}

void write_digest(std::vector<Byte>& bytes, const Digest32& digest)
{
    bytes.insert(bytes.end(), digest.bytes.begin(), digest.bytes.end());
}

[[nodiscard]] Digest32 read_digest(std::span<const Byte> bytes, std::size_t offset)
{
    if (offset > bytes.size() || bytes.size() - offset < kHashBytes) {
        throw TaskError(TaskErrorCode::Truncated, "truncated 32-byte digest field");
    }
    Digest32 digest{};
    std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset), kHashBytes, digest.bytes.begin());
    return digest;
}

[[nodiscard]] bool has_unique_roles(const TaskShape& shape, const Topology& topology)
{
    std::vector<bool> seen(shape.population, false);
    for (const std::uint32_t neuron : topology.input_neurons) {
        if (neuron >= shape.population || seen[neuron]) {
            return false;
        }
        seen[neuron] = true;
    }
    for (const std::uint32_t neuron : topology.output_neurons) {
        if (neuron >= shape.population || seen[neuron]) {
            return false;
        }
        seen[neuron] = true;
    }
    if (topology.signal_neuron >= shape.population || seen[topology.signal_neuron]) {
        return false;
    }
    return true;
}

void validate_shape(const TaskShape& shape)
{
    if (shape.input_trits == 0U || shape.output_trits == 0U || shape.sequence_length == 0U
        || shape.population == 0U || shape.neighbors == 0U) {
        throw TaskError(TaskErrorCode::BadDimensions, "task dimensions must be nonzero");
    }
    const std::uint64_t role_count = static_cast<std::uint64_t>(shape.input_trits)
        + static_cast<std::uint64_t>(shape.output_trits) + 1U;
    if (role_count >= static_cast<std::uint64_t>(shape.population)) {
        throw TaskError(TaskErrorCode::BadDimensions, "population has no room for non-role neurons");
    }
    if ((shape.population & (shape.population - 1U)) != 0U) {
        throw TaskError(TaskErrorCode::BadDimensions, "population must be a power of two");
    }
}

void validate_task_components(const Task& task)
{
    const TaskShape& shape = task.header.shape;
    validate_shape(shape);
    if (task.header.magic != kTaskMagic) {
        throw TaskError(TaskErrorCode::BadMagic, "unexpected task magic");
    }
    if (task.header.version != kTaskVersion) {
        throw TaskError(TaskErrorCode::BadVersion, "unexpected task version");
    }
    if (task.topology.input_neurons.size() != shape.input_trits
        || task.topology.output_neurons.size() != shape.output_trits
        || task.topology.neighbors.size()
            != static_cast<std::size_t>(shape.population) * static_cast<std::size_t>(shape.neighbors)) {
        throw TaskError(TaskErrorCode::InvalidTopology, "topology vector lengths do not match the header");
    }
    if (!has_unique_roles(shape, task.topology)) {
        throw TaskError(TaskErrorCode::InvalidTopology, "role neuron indices must be unique and in range");
    }
    for (const std::uint32_t neighbor : task.topology.neighbors) {
        if (neighbor >= shape.population) {
            throw TaskError(TaskErrorCode::InvalidTopology, "neighbor index is outside the population");
        }
    }
    const std::uint64_t input_count = checked_multiply(shape.sequence_length, shape.input_trits, "input trit count");
    const std::uint64_t output_count = checked_multiply(shape.sequence_length, shape.output_trits, "output trit count");
    if (task.inputs.size() != checked_to_size(input_count, "input trit count")
        || task.outputs.size() != checked_to_size(output_count, "output trit count")) {
        throw TaskError(TaskErrorCode::InvalidDomainValue, "task data vector lengths do not match the header");
    }
    for (const Trit value : task.inputs) {
        if (!is_valid_trit_byte(trit_to_byte(value))) {
            throw TaskError(TaskErrorCode::InvalidDomainValue, "input contains an invalid trit");
        }
    }
    for (const Trit value : task.outputs) {
        if (!is_valid_trit_byte(trit_to_byte(value))) {
            throw TaskError(TaskErrorCode::InvalidDomainValue, "output contains an invalid trit");
        }
    }
}

} // namespace

Digest32 DeterministicFixtureDigest::digest(std::span<const Byte> block) const
{
    // Four independent lanes are enough for a stable fixture fingerprint. It
    // is deliberately not presented as a cryptographic digest.
    std::array<std::uint64_t, 4U> lanes{
        seed_ ^ 0x243F6A8885A308D3ULL,
        seed_ ^ 0x13198A2E03707344ULL,
        seed_ ^ 0xA4093822299F31D0ULL,
        seed_ ^ 0x082EFA98EC4E6C89ULL,
    };
    for (std::size_t index = 0U; index < block.size(); ++index) {
        const std::uint64_t byte = block[index];
        lanes[0] = (lanes[0] ^ (byte + index)) * 0x100000001B3ULL;
        lanes[1] = std::rotl(lanes[1] + byte + 0x9E3779B97F4A7C15ULL, 13U) ^ lanes[0];
        lanes[2] = (lanes[2] + (byte ^ (index * 0x9DULL))) * 0xD6E8FEB86659FD93ULL;
        lanes[3] = std::rotl(lanes[3] ^ (lanes[2] + byte), 29U) + 0xC2B2AE3D27D4EB4FULL;
    }
    for (std::size_t i = 0U; i < lanes.size(); ++i) {
        lanes[i] ^= static_cast<std::uint64_t>(block.size()) + i * 0x9E3779B97F4A7C15ULL;
        lanes[i] ^= lanes[i] >> 30U;
        lanes[i] *= 0xBF58476D1CE4E5B9ULL;
        lanes[i] ^= lanes[i] >> 27U;
        lanes[i] *= 0x94D049BB133111EBULL;
        lanes[i] ^= lanes[i] >> 31U;
    }

    Digest32 result{};
    for (std::size_t lane = 0U; lane < lanes.size(); ++lane) {
        for (std::size_t byte = 0U; byte < 8U; ++byte) {
            result.bytes[lane * 8U + byte] = static_cast<Byte>(lanes[lane] >> (8U * byte));
        }
    }
    return result;
}

std::size_t packed_bytes_for_trits(std::uint64_t trit_count)
{
    const std::uint64_t numerator = checked_add(trit_count, kTritsPerPackedByte - 1U, "packed trit count");
    return checked_to_size(numerator / kTritsPerPackedByte, "packed byte count");
}

std::size_t topology_bytes_for_shape(const TaskShape& shape)
{
    validate_shape(shape);
    const std::uint64_t neuron_count = checked_add(
        checked_add(shape.input_trits, shape.output_trits, "topology role count"), 1U, "topology role count");
    const std::uint64_t neighbor_count = checked_multiply(shape.population, shape.neighbors, "topology neighbor count");
    const std::uint64_t words = checked_add(neuron_count, neighbor_count, "topology word count");
    return checked_to_size(checked_multiply(words, 4U, "topology byte count"), "topology byte count");
}

std::size_t data_bytes_for_shape(const TaskShape& shape)
{
    validate_shape(shape);
    const std::uint64_t row_bytes = checked_add(
        packed_bytes_for_trits(shape.input_trits), packed_bytes_for_trits(shape.output_trits), "data row byte count");
    return checked_to_size(
        checked_multiply(shape.sequence_length, row_bytes, "data byte count"), "data byte count");
}

std::vector<Byte> pack_trits(std::span<const Trit> trits)
{
    std::vector<Byte> packed(packed_bytes_for_trits(trits.size()), 0U);
    for (std::size_t index = 0U; index < trits.size(); ++index) {
        const Byte value = trit_to_byte(trits[index]);
        if (!is_valid_trit_byte(value)) {
            throw TaskError(TaskErrorCode::InvalidDomainValue, "cannot pack a value outside trits 0, 1, 2");
        }
        const std::size_t byte_index = index / kTritsPerPackedByte;
        const std::size_t position = index % kTritsPerPackedByte;
        std::uint32_t existing = packed[byte_index];
        std::uint32_t weight = 1U;
        for (std::size_t i = 0U; i < position; ++i) {
            weight *= 3U;
        }
        existing += static_cast<std::uint32_t>(value) * weight;
        packed[byte_index] = static_cast<Byte>(existing);
    }
    return packed;
}

std::vector<Trit> unpack_trits(std::span<const Byte> packed, std::size_t trit_count)
{
    const std::size_t required = packed_bytes_for_trits(trit_count);
    if (packed.size() < required) {
        throw TaskError(TaskErrorCode::Truncated, "packed trit block is shorter than its declared count");
    }
    std::vector<Trit> trits(trit_count, Trit::Zero);
    for (std::size_t byte_index = 0U; byte_index < required; ++byte_index) {
        std::uint32_t value = packed[byte_index];
        if (value >= kPackedByteLimit) {
            throw TaskError(TaskErrorCode::InvalidPackedTrit, "packed trit byte is outside base-243");
        }
        for (std::size_t position = 0U; position < kTritsPerPackedByte; ++position) {
            const std::size_t trit_index = byte_index * kTritsPerPackedByte + position;
            if (trit_index < trit_count) {
                trits[trit_index] = static_cast<Trit>(value % 3U);
            }
            value /= 3U;
        }
    }
    return trits;
}

std::vector<Byte> serialize_topology(const TaskShape& shape, const Topology& topology)
{
    validate_shape(shape);
    if (topology.input_neurons.size() != shape.input_trits
        || topology.output_neurons.size() != shape.output_trits
        || topology.neighbors.size()
            != static_cast<std::size_t>(shape.population) * static_cast<std::size_t>(shape.neighbors)
        || !has_unique_roles(shape, topology)) {
        throw TaskError(TaskErrorCode::InvalidTopology, "topology does not match the declared shape");
    }
    for (const std::uint32_t neighbor : topology.neighbors) {
        if (neighbor >= shape.population) {
            throw TaskError(TaskErrorCode::InvalidTopology, "topology neighbor is outside the population");
        }
    }

    std::vector<Byte> bytes;
    bytes.reserve(topology_bytes_for_shape(shape));
    for (const std::uint32_t neuron : topology.input_neurons) {
        write_u32(bytes, neuron);
    }
    for (const std::uint32_t neuron : topology.output_neurons) {
        write_u32(bytes, neuron);
    }
    write_u32(bytes, topology.signal_neuron);
    for (const std::uint32_t neighbor : topology.neighbors) {
        write_u32(bytes, neighbor);
    }
    return bytes;
}

std::vector<Byte> serialize_data(const TaskShape& shape,
                                 std::span<const Trit> inputs,
                                 std::span<const Trit> outputs)
{
    validate_shape(shape);
    const std::size_t input_count = checked_to_size(
        checked_multiply(shape.sequence_length, shape.input_trits, "input trit count"), "input trit count");
    const std::size_t output_count = checked_to_size(
        checked_multiply(shape.sequence_length, shape.output_trits, "output trit count"), "output trit count");
    if (inputs.size() != input_count || outputs.size() != output_count) {
        throw TaskError(TaskErrorCode::InvalidDomainValue, "data vectors do not match the declared shape");
    }

    const std::size_t input_bytes = packed_bytes_for_trits(shape.input_trits);
    const std::size_t output_bytes = packed_bytes_for_trits(shape.output_trits);
    std::vector<Byte> data;
    data.reserve(data_bytes_for_shape(shape));
    for (std::size_t row = 0U; row < static_cast<std::size_t>(shape.sequence_length); ++row) {
        const std::span<const Trit> input_row = inputs.subspan(row * shape.input_trits, shape.input_trits);
        const std::span<const Trit> output_row = outputs.subspan(row * shape.output_trits, shape.output_trits);
        const std::vector<Byte> packed_input = pack_trits(input_row);
        const std::vector<Byte> packed_output = pack_trits(output_row);
        if (packed_input.size() != input_bytes || packed_output.size() != output_bytes) {
            throw TaskError(TaskErrorCode::BadLength, "internal packed row length mismatch");
        }
        data.insert(data.end(), packed_input.begin(), packed_input.end());
        data.insert(data.end(), packed_output.begin(), packed_output.end());
    }
    return data;
}

std::vector<Byte> serialize_task(const Task& task)
{
    validate_task_components(task);
    const std::vector<Byte> topology = serialize_topology(task.header.shape, task.topology);
    const std::vector<Byte> data = serialize_data(task.header.shape, task.inputs, task.outputs);

    std::vector<Byte> bytes;
    bytes.reserve(kTaskHeaderBytes + topology.size() + data.size());
    write_u32(bytes, task.header.magic);
    write_u32(bytes, task.header.version);
    write_u32(bytes, task.header.shape.input_trits);
    write_u32(bytes, task.header.shape.output_trits);
    write_u64(bytes, task.header.shape.sequence_length);
    write_u32(bytes, task.header.shape.population);
    write_u32(bytes, task.header.shape.neighbors);
    write_digest(bytes, task.header.topology_hash);
    write_digest(bytes, task.header.data_hash);
    bytes.insert(bytes.end(), topology.begin(), topology.end());
    bytes.insert(bytes.end(), data.begin(), data.end());
    return bytes;
}

Task parse_task(std::span<const Byte> bytes, const TaskParseOptions& options)
{
    if (bytes.size() < kTaskHeaderBytes) {
        throw TaskError(TaskErrorCode::Truncated, "task is shorter than its 96-byte header");
    }

    Task task;
    task.header.magic = read_u32(bytes, 0U);
    task.header.version = read_u32(bytes, 4U);
    task.header.shape.input_trits = read_u32(bytes, 8U);
    task.header.shape.output_trits = read_u32(bytes, 12U);
    task.header.shape.sequence_length = read_u64(bytes, 16U);
    task.header.shape.population = read_u32(bytes, 24U);
    task.header.shape.neighbors = read_u32(bytes, 28U);
    task.header.topology_hash = read_digest(bytes, 32U);
    task.header.data_hash = read_digest(bytes, 64U);

    if (task.header.magic != kTaskMagic) {
        throw TaskError(TaskErrorCode::BadMagic, "task magic does not identify a LUT task");
    }
    if (task.header.version != kTaskVersion) {
        throw TaskError(TaskErrorCode::BadVersion, "unsupported task format version");
    }
    validate_shape(task.header.shape);
    if (options.expected_shape.has_value() && task.header.shape != options.expected_shape.value()) {
        throw TaskError(TaskErrorCode::BadDimensions, "task dimensions do not match the requested BPP9000 shape");
    }

    const std::size_t topology_size = topology_bytes_for_shape(task.header.shape);
    const std::size_t data_size = data_bytes_for_shape(task.header.shape);
    const std::uint64_t expected_length_u64 = checked_add(
        checked_add(kTaskHeaderBytes, topology_size, "task length"), data_size, "task length");
    const std::size_t expected_length = checked_to_size(expected_length_u64, "task length");
    if (bytes.size() < expected_length) {
        throw TaskError(TaskErrorCode::Truncated, "task is shorter than its declared topology/data blocks");
    }
    if (bytes.size() > expected_length) {
        throw TaskError(TaskErrorCode::TrailingBytes, "task contains forbidden trailing bytes");
    }

    const bool topology_hash_is_zero = task.header.topology_hash.is_zero();
    const bool data_hash_is_zero = task.header.data_hash.is_zero();
    if (options.require_hash_metadata && (topology_hash_is_zero || data_hash_is_zero)) {
        throw TaskError(TaskErrorCode::HashMismatch, "task is missing required topology/data hash metadata");
    }
    if ((!topology_hash_is_zero || !data_hash_is_zero) && options.digest_provider == nullptr) {
        throw TaskError(TaskErrorCode::MissingDigestProvider, "hash metadata requires an injected block digest provider");
    }

    const std::span<const Byte> topology_block = bytes.subspan(kTaskHeaderBytes, topology_size);
    const std::span<const Byte> data_block = bytes.subspan(kTaskHeaderBytes + topology_size, data_size);
    if (options.digest_provider != nullptr) {
        const Digest32 actual_topology = options.digest_provider->digest(topology_block);
        const Digest32 actual_data = options.digest_provider->digest(data_block);
        if ((!topology_hash_is_zero && actual_topology != task.header.topology_hash)
            || (!data_hash_is_zero && actual_data != task.header.data_hash)) {
            throw TaskError(TaskErrorCode::HashMismatch, "task block digest does not match header metadata");
        }
    }

    task.topology.input_neurons.reserve(task.header.shape.input_trits);
    task.topology.output_neurons.reserve(task.header.shape.output_trits);
    std::size_t offset = 0U;
    for (std::uint32_t index = 0U; index < task.header.shape.input_trits; ++index) {
        task.topology.input_neurons.push_back(read_u32(topology_block, offset));
        offset += 4U;
    }
    for (std::uint32_t index = 0U; index < task.header.shape.output_trits; ++index) {
        task.topology.output_neurons.push_back(read_u32(topology_block, offset));
        offset += 4U;
    }
    task.topology.signal_neuron = read_u32(topology_block, offset);
    offset += 4U;
    const std::size_t neighbor_count = static_cast<std::size_t>(task.header.shape.population)
        * static_cast<std::size_t>(task.header.shape.neighbors);
    task.topology.neighbors.reserve(neighbor_count);
    for (std::size_t index = 0U; index < neighbor_count; ++index) {
        task.topology.neighbors.push_back(read_u32(topology_block, offset));
        offset += 4U;
    }
    if (offset != topology_block.size() || !has_unique_roles(task.header.shape, task.topology)) {
        throw TaskError(TaskErrorCode::InvalidTopology, "task topology roles are duplicated or out of range");
    }
    for (const std::uint32_t neighbor : task.topology.neighbors) {
        if (neighbor >= task.header.shape.population) {
            throw TaskError(TaskErrorCode::InvalidTopology, "task topology neighbor is out of range");
        }
    }

    const std::size_t input_bytes = packed_bytes_for_trits(task.header.shape.input_trits);
    const std::size_t output_bytes = packed_bytes_for_trits(task.header.shape.output_trits);
    const std::size_t row_bytes = input_bytes + output_bytes;
    const std::size_t input_count = checked_to_size(
        checked_multiply(task.header.shape.sequence_length, task.header.shape.input_trits, "input trit count"),
        "input trit count");
    const std::size_t output_count = checked_to_size(
        checked_multiply(task.header.shape.sequence_length, task.header.shape.output_trits, "output trit count"),
        "output trit count");
    task.inputs.reserve(input_count);
    task.outputs.reserve(output_count);
    for (std::size_t row = 0U; row < static_cast<std::size_t>(task.header.shape.sequence_length); ++row) {
        const std::span<const Byte> packed_input = data_block.subspan(row * row_bytes, input_bytes);
        const std::span<const Byte> packed_output = data_block.subspan(row * row_bytes + input_bytes, output_bytes);
        const std::vector<Trit> input_row = unpack_trits(packed_input, task.header.shape.input_trits);
        const std::vector<Trit> output_row = unpack_trits(packed_output, task.header.shape.output_trits);
        task.inputs.insert(task.inputs.end(), input_row.begin(), input_row.end());
        task.outputs.insert(task.outputs.end(), output_row.begin(), output_row.end());
    }

    task.packed_topology.assign(topology_block.begin(), topology_block.end());
    task.packed_data.assign(data_block.begin(), data_block.end());
    return task;
}

Task parse_production_task(std::span<const Byte> bytes, const BlockDigestProvider& digest_provider)
{
    TaskParseOptions options;
    options.digest_provider = &digest_provider;
    options.expected_shape = production_shape();
    return parse_task(bytes, options);
}

} // namespace xdna::bpp9000
