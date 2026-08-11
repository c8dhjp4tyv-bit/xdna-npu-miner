#include "pearl/reference.hpp"

#include "pearl/blake3_ffi.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <string_view>

namespace xdna::pearl {
namespace {

constexpr std::uint32_t kPatternPeriodLimit = 1U << 24U;
constexpr std::uint32_t kTileDwordBytes = 4U;
constexpr std::uint32_t kWorkerInputLimit = 1U << 22U;
constexpr std::int32_t kSignalMin = -64;
constexpr std::int32_t kSignalMax = 64;
constexpr std::int32_t kQuantizationMax = 63;

[[noreturn]] void fail(ErrorCode code, const std::string& message)
{
    throw Error(code, message);
}

void require(bool condition, ErrorCode code, const std::string& message)
{
    if (!condition) {
        fail(code, message);
    }
}

[[nodiscard]] std::size_t checked_product(std::size_t left,
                                          std::size_t right,
                                          const char* what)
{
    require(left == 0U || right <= std::numeric_limits<std::size_t>::max() / left,
            ErrorCode::InvalidLength,
            std::string(what) + " overflows host size");
    return left * right;
}

[[nodiscard]] std::uint32_t checked_u32(std::uint64_t value, const char* what)
{
    require(value <= std::numeric_limits<std::uint32_t>::max(),
            ErrorCode::InvalidValue,
            std::string(what) + " does not fit u32");
    return static_cast<std::uint32_t>(value);
}

void append_u16(std::vector<std::uint8_t>& output, std::uint16_t value)
{
    output.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    output.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void append_u32(std::vector<std::uint8_t>& output, std::uint32_t value)
{
    for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
        output.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
    }
}

void append_digest(std::vector<std::uint8_t>& output, const Digest& digest)
{
    output.insert(output.end(), digest.begin(), digest.end());
}

class Reader final {
public:
    explicit Reader(std::span<const std::uint8_t> bytes)
        : bytes_(bytes)
    {
    }

    [[nodiscard]] std::size_t remaining() const noexcept
    {
        return bytes_.size() - offset_;
    }

    [[nodiscard]] std::uint8_t u8()
    {
        require(remaining() >= 1U, ErrorCode::InvalidLength, "truncated u8");
        return bytes_[offset_++];
    }

    [[nodiscard]] std::uint16_t u16()
    {
        require(remaining() >= 2U, ErrorCode::InvalidLength, "truncated u16");
        const std::uint16_t value = static_cast<std::uint16_t>(bytes_[offset_])
            | (static_cast<std::uint16_t>(bytes_[offset_ + 1U]) << 8U);
        offset_ += 2U;
        return value;
    }

    [[nodiscard]] std::uint32_t u32()
    {
        require(remaining() >= 4U, ErrorCode::InvalidLength, "truncated u32");
        std::uint32_t value = 0U;
        for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
            value |= static_cast<std::uint32_t>(bytes_[offset_++]) << shift;
        }
        return value;
    }

    [[nodiscard]] std::uint64_t u64()
    {
        require(remaining() >= 8U, ErrorCode::InvalidLength, "truncated u64");
        std::uint64_t value = 0U;
        for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
            value |= static_cast<std::uint64_t>(bytes_[offset_++]) << shift;
        }
        return value;
    }

    [[nodiscard]] std::vector<std::uint8_t> bytes(std::size_t length)
    {
        require(remaining() >= length, ErrorCode::InvalidLength, "truncated byte field");
        std::vector<std::uint8_t> result(bytes_.begin() + static_cast<std::ptrdiff_t>(offset_),
                                         bytes_.begin() + static_cast<std::ptrdiff_t>(offset_ + length));
        offset_ += length;
        return result;
    }

    template <std::size_t N>
    [[nodiscard]] std::array<std::uint8_t, N> fixed_bytes()
    {
        require(remaining() >= N, ErrorCode::InvalidLength, "truncated fixed byte field");
        std::array<std::uint8_t, N> result{};
        std::copy_n(bytes_.begin() + static_cast<std::ptrdiff_t>(offset_),
                    N,
                    result.begin());
        offset_ += N;
        return result;
    }

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t offset_ = 0U;
};

void require_no_remaining(const Reader& reader, const char* label)
{
    require(reader.remaining() == 0U,
            ErrorCode::InvalidLength,
            std::string(label) + " has trailing bytes");
}

void validate_pattern_shape(const std::array<PeriodicPattern::Shape, 3U>& shape)
{
    std::uint64_t minimum_stride = 1U;
    bool done = false;
    for (const PeriodicPattern::Shape item : shape) {
        require(item.stride >= 1U && item.length >= 1U,
                ErrorCode::InvalidShape,
                "pattern stride and length must be positive");
        const std::uint64_t stride = item.stride;
        require(stride % minimum_stride == 0U,
                ErrorCode::NonCanonical,
                "pattern stride is not a multiple of the preceding period");
        const std::uint64_t factor = stride / minimum_stride;
        require(factor <= 256U && item.length <= 256U,
                ErrorCode::InvalidShape,
                "pattern factor or length exceeds one-byte encoding");
        if (item.length == 1U || done) {
            require(factor == 1U && item.length == 1U,
                    ErrorCode::NonCanonical,
                    "pattern contains non-canonical trailing dimensions");
            done = true;
        }
        require(minimum_stride <= kPatternPeriodLimit / (factor * item.length),
                ErrorCode::InvalidShape,
                "pattern period exceeds 2^24");
        minimum_stride = stride * item.length;
    }
}

[[nodiscard]] std::uint32_t rotl32(std::uint32_t value, std::uint32_t amount)
{
    return std::rotl(value, static_cast<int>(amount));
}

[[nodiscard]] std::int32_t checked_i32(std::int64_t value, const char* what)
{
    require(value >= std::numeric_limits<std::int32_t>::min()
                && value <= std::numeric_limits<std::int32_t>::max(),
            ErrorCode::ArithmeticOverflow,
            std::string(what) + " exceeds int32");
    return static_cast<std::int32_t>(value);
}

[[nodiscard]] std::uint32_t as_u32(std::int32_t value)
{
    return std::bit_cast<std::uint32_t>(value);
}

[[nodiscard]] std::uint32_t mul_hi_u32(std::uint32_t left, std::uint32_t right)
{
    return static_cast<std::uint32_t>((static_cast<std::uint64_t>(left)
                                       * static_cast<std::uint64_t>(right))
                                      >> 32U);
}

[[nodiscard]] Digest hash_unkeyed(std::span<const std::uint8_t> data)
{
    Digest digest{};
    const int result = pearl_blake3_hash(data.data(), data.size(), digest.data());
    require(result == 0, ErrorCode::InvalidValue, "BLAKE3 unkeyed FFI call failed");
    return digest;
}

[[nodiscard]] std::vector<std::uint8_t> padded_chunk_data(std::span<const std::uint8_t> data)
{
    if (data.empty()) {
        return {};
    }
    const std::size_t chunks = (data.size() + kMerkleChunkBytes - 1U) / kMerkleChunkBytes;
    const std::size_t padded_size = checked_product(chunks, kMerkleChunkBytes, "padded data size");
    std::vector<std::uint8_t> padded(padded_size, 0U);
    std::copy(data.begin(), data.end(), padded.begin());
    return padded;
}

struct MerkleLayers {
    std::vector<std::vector<Digest>> layers;
    std::vector<std::uint8_t> padded_data;
};

[[nodiscard]] MerkleLayers build_merkle_layers(std::span<const std::uint8_t> data,
                                               const Digest& key)
{
    MerkleLayers result;
    result.padded_data = padded_chunk_data(data);
    if (result.padded_data.empty()) {
        return result;
    }
    const std::size_t leaf_count = result.padded_data.size() / kMerkleChunkBytes;
    result.layers.emplace_back();
    result.layers.back().reserve(leaf_count);
    if (leaf_count == 1U) {
        result.layers.back().push_back(blake3_keyed(key, result.padded_data));
        return result;
    }
    for (std::size_t index = 0U; index < leaf_count; ++index) {
        const auto* begin = result.padded_data.data() + index * kMerkleChunkBytes;
        result.layers.back().push_back(blake3_chunk_cv(
            key,
            std::span<const std::uint8_t>(begin, kMerkleChunkBytes),
            static_cast<std::uint64_t>(index)));
    }
    while (result.layers.back().size() > 2U) {
        const auto& previous = result.layers.back();
        std::vector<Digest> next;
        next.reserve((previous.size() + 1U) / 2U);
        for (std::size_t index = 0U; index < previous.size(); index += 2U) {
            if (index + 1U == previous.size()) {
                next.push_back(previous[index]);
            } else {
                next.push_back(blake3_parent_cv(key, previous[index], previous[index + 1U], false));
            }
        }
        result.layers.push_back(std::move(next));
    }
    if (result.layers.back().size() == 2U) {
        const auto& previous = result.layers.back();
        result.layers.push_back(
            {blake3_parent_cv(key, previous[0], previous[1], true)});
    }
    return result;
}

void validate_merkle_shape(const MerkleProof& proof)
{
    require(!proof.leaf_indices.empty(), ErrorCode::InvalidLength, "Merkle proof has no leaves");
    require(proof.leaf_indices.size() == proof.leaf_data.size(),
            ErrorCode::InvalidLength,
            "Merkle proof leaf index/data lengths differ");
    require(std::is_sorted(proof.leaf_indices.begin(), proof.leaf_indices.end()),
            ErrorCode::NonCanonical,
            "Merkle proof leaf indices are not sorted");
    require(std::adjacent_find(proof.leaf_indices.begin(), proof.leaf_indices.end())
                == proof.leaf_indices.end(),
            ErrorCode::NonCanonical,
            "Merkle proof leaf indices are not unique");
    require(proof.total_leaves != 0U, ErrorCode::InvalidShape, "Merkle proof has zero total leaves");
    require(proof.leaf_indices.back() < proof.total_leaves,
            ErrorCode::OutOfBounds,
            "Merkle proof leaf index is out of bounds");
}

[[nodiscard]] std::size_t sibling_count_for(const std::vector<std::size_t>& indices,
                                            std::size_t total_leaves)
{
    std::set<std::size_t> current(indices.begin(), indices.end());
    std::size_t count = 0U;
    std::size_t level_len = total_leaves;
    while (level_len > 1U && !current.empty()) {
        for (const std::size_t index : current) {
            if ((index & 1U) != 0U) {
                if (!current.contains(index - 1U)) {
                    ++count;
                }
            } else if (index + 1U < level_len && !current.contains(index + 1U)) {
                ++count;
            }
        }
        std::set<std::size_t> next;
        for (const std::size_t index : current) {
            next.insert(index / 2U);
        }
        current = std::move(next);
        level_len = (level_len + 1U) / 2U;
    }
    return count;
}

void serialize_merkle_proof(std::vector<std::uint8_t>& output, const MerkleProof& proof)
{
    validate_merkle_shape(proof);
    append_u32(output, checked_u32(proof.leaf_indices.size(), "leaf count"));
    append_u32(output, checked_u32(proof.total_leaves, "total leaf count"));
    for (const std::size_t index : proof.leaf_indices) {
        append_u32(output, checked_u32(index, "leaf index"));
    }
    for (const auto& leaf : proof.leaf_data) {
        output.insert(output.end(), leaf.begin(), leaf.end());
    }
    append_digest(output, proof.root);
    append_u32(output, checked_u32(proof.siblings.size(), "sibling count"));
    for (const Digest& sibling : proof.siblings) {
        append_digest(output, sibling);
    }
}

[[nodiscard]] MerkleProof deserialize_merkle_proof(Reader& reader)
{
    const std::size_t leaf_count = reader.u32();
    const std::size_t total_leaves = reader.u32();
    require(leaf_count != 0U, ErrorCode::InvalidLength, "Merkle proof has no leaves");
    require(leaf_count <= 1'000'000U, ErrorCode::InvalidLength, "Merkle proof leaf count is too large");
    require(total_leaves != 0U && leaf_count <= total_leaves,
            ErrorCode::InvalidShape,
            "Merkle proof total leaf count is invalid");
    MerkleProof proof;
    proof.total_leaves = total_leaves;
    proof.leaf_indices.reserve(leaf_count);
    for (std::size_t index = 0U; index < leaf_count; ++index) {
        proof.leaf_indices.push_back(reader.u32());
    }
    proof.leaf_data.reserve(leaf_count);
    for (std::size_t index = 0U; index < leaf_count; ++index) {
        proof.leaf_data.push_back(reader.fixed_bytes<kMerkleChunkBytes>());
    }
    proof.root = reader.fixed_bytes<kDigestBytes>();
    const std::size_t sibling_count = reader.u32();
    require(sibling_count == sibling_count_for(proof.leaf_indices, proof.total_leaves),
            ErrorCode::InvalidShape,
            "Merkle proof sibling count is inconsistent with its leaf set");
    proof.siblings.reserve(sibling_count);
    for (std::size_t index = 0U; index < sibling_count; ++index) {
        proof.siblings.push_back(reader.fixed_bytes<kDigestBytes>());
    }
    validate_merkle_shape(proof);
    return proof;
}

} // namespace

Error::Error(ErrorCode code, const std::string& message)
    : std::runtime_error(message),
      code_(code)
{
}

std::vector<std::uint32_t> PeriodicPattern::indices() const
{
    validate_pattern_shape(shape_);
    std::vector<std::uint32_t> result{0U};
    for (const Shape item : shape_) {
        const std::size_t next_size = checked_product(result.size(), item.length, "pattern index count");
        std::vector<std::uint32_t> next;
        next.reserve(next_size);
        for (std::uint32_t offset = 0U; offset < item.length; ++offset) {
            for (const std::uint32_t value : result) {
                const std::uint64_t next_value = static_cast<std::uint64_t>(value)
                    + static_cast<std::uint64_t>(offset) * item.stride;
                next.push_back(checked_u32(next_value, "pattern index"));
            }
        }
        result = std::move(next);
    }
    return result;
}

std::vector<std::uint32_t> PeriodicPattern::indices_with_offset(std::uint32_t offset) const
{
    std::vector<std::uint32_t> result = indices();
    for (std::uint32_t& value : result) {
        value = checked_u32(static_cast<std::uint64_t>(value) + offset, "pattern offset");
    }
    return result;
}

std::array<std::uint8_t, 6U> PeriodicPattern::to_bytes() const
{
    validate_pattern_shape(shape_);
    std::array<std::uint8_t, 6U> result{};
    std::uint64_t minimum_stride = 1U;
    for (std::size_t index = 0U; index < shape_.size(); ++index) {
        const Shape item = shape_[index];
        const std::uint64_t factor = item.stride / minimum_stride;
        result[index * 2U] = static_cast<std::uint8_t>(factor - 1U);
        result[index * 2U + 1U] = static_cast<std::uint8_t>(item.length - 1U);
        minimum_stride = static_cast<std::uint64_t>(item.stride) * item.length;
    }
    return result;
}

bool PeriodicPattern::offset_is_valid(std::uint32_t offset) const
{
    validate_pattern_shape(shape_);
    for (auto iterator = shape_.rbegin(); iterator != shape_.rend(); ++iterator) {
        const std::uint32_t period = checked_u32(static_cast<std::uint64_t>(iterator->stride)
                                                      * iterator->length,
                                                  "pattern period");
        offset %= period;
        if (offset >= iterator->stride) {
            return false;
        }
    }
    return true;
}

std::uint32_t PeriodicPattern::max_index() const
{
    const std::vector<std::uint32_t> values = indices();
    return *std::max_element(values.begin(), values.end());
}

PeriodicPattern PeriodicPattern::from_bytes(std::span<const std::uint8_t> bytes)
{
    require(bytes.size() == 6U, ErrorCode::InvalidLength, "periodic pattern must be six bytes");
    std::array<Shape, 3U> shape{};
    std::uint64_t minimum_stride = 1U;
    bool done = false;
    for (std::size_t index = 0U; index < shape.size(); ++index) {
        const std::uint32_t factor = static_cast<std::uint32_t>(bytes[index * 2U]) + 1U;
        const std::uint32_t length = static_cast<std::uint32_t>(bytes[index * 2U + 1U]) + 1U;
        if (length == 1U || done) {
            require(factor == 1U && length == 1U,
                    ErrorCode::NonCanonical,
                    "periodic pattern has non-canonical trailing bytes");
            done = true;
        } else if (factor == 1U && minimum_stride != 1U) {
            fail(ErrorCode::NonCanonical, "periodic pattern breaks a single stride");
        }
        require(minimum_stride <= kPatternPeriodLimit / (factor * length),
                ErrorCode::InvalidShape,
                "periodic pattern period exceeds 2^24");
        shape[index] = Shape{checked_u32(minimum_stride * factor, "pattern stride"), length};
        minimum_stride *= static_cast<std::uint64_t>(factor) * length;
    }
    const PeriodicPattern result(shape);
    const auto canonical = result.to_bytes();
    require(std::equal(canonical.begin(), canonical.end(), bytes.begin(), bytes.end()),
            ErrorCode::NonCanonical,
            "periodic pattern does not round-trip canonically");
    return result;
}

PeriodicPattern PeriodicPattern::from_indices(std::span<const std::uint32_t> indices)
{
    require(!indices.empty(), ErrorCode::InvalidLength, "periodic pattern cannot be empty");
    require(indices.front() == 0U, ErrorCode::InvalidValue, "periodic pattern must start at zero");
    require(std::is_sorted(indices.begin(), indices.end()),
            ErrorCode::InvalidValue,
            "periodic pattern indices must be sorted");
    require(std::adjacent_find(indices.begin(), indices.end()) == indices.end(),
            ErrorCode::InvalidValue,
            "periodic pattern indices must be unique");

    std::vector<std::uint32_t> remaining(indices.begin(), indices.end());
    std::vector<Shape> shape;
    while (remaining.size() > 1U) {
        bool found = false;
        for (std::size_t period = 1U; period < remaining.size(); ++period) {
            if (remaining.size() % period != 0U) {
                continue;
            }
            const std::uint32_t stride = remaining[period];
            bool periodic = true;
            for (std::size_t index = 0U; index + period < remaining.size(); ++index) {
                if (static_cast<std::uint64_t>(remaining[index]) + stride != remaining[index + period]) {
                    periodic = false;
                    break;
                }
            }
            if (periodic) {
                shape.push_back(Shape{stride, static_cast<std::uint32_t>(remaining.size() / period)});
                remaining.resize(period);
                found = true;
                break;
            }
        }
        require(found, ErrorCode::InvalidShape, "indices are not a periodic pattern");
    }
    std::reverse(shape.begin(), shape.end());
    const std::uint32_t period = shape.empty()
        ? 1U
        : checked_u32(static_cast<std::uint64_t>(shape.back().stride) * shape.back().length,
                      "pattern period");
    while (shape.size() < 3U) {
        shape.push_back(Shape{period, 1U});
    }
    require(shape.size() == 3U, ErrorCode::InvalidShape, "pattern has too many dimensions");
    return PeriodicPattern(std::array<Shape, 3U>{shape[0], shape[1], shape[2]});
}

std::array<std::uint8_t, kMiningConfigBytes> MiningConfiguration::to_bytes() const
{
    std::vector<std::uint8_t> bytes;
    bytes.reserve(kMiningConfigBytes);
    append_u32(bytes, common_dim);
    append_u16(bytes, rank);
    append_u16(bytes, mma_type);
    const auto rows = rows_pattern.to_bytes();
    const auto cols = cols_pattern.to_bytes();
    bytes.insert(bytes.end(), rows.begin(), rows.end());
    bytes.insert(bytes.end(), cols.begin(), cols.end());
    bytes.insert(bytes.end(), dense_trailer.begin(), dense_trailer.end());
    require(bytes.size() == kMiningConfigBytes,
            ErrorCode::InvalidLength,
            "mining configuration has unexpected serialized size");
    std::array<std::uint8_t, kMiningConfigBytes> result{};
    std::copy(bytes.begin(), bytes.end(), result.begin());
    return result;
}

MiningConfiguration MiningConfiguration::from_bytes(std::span<const std::uint8_t> bytes)
{
    require(bytes.size() == kMiningConfigBytes,
            ErrorCode::InvalidLength,
            "mining configuration must be 52 bytes");
    Reader reader(bytes);
    MiningConfiguration result;
    result.common_dim = reader.u32();
    result.rank = reader.u16();
    result.mma_type = reader.u16();
    result.rows_pattern = PeriodicPattern::from_bytes(reader.bytes(6U));
    result.cols_pattern = PeriodicPattern::from_bytes(reader.bytes(6U));
    result.dense_trailer = reader.fixed_bytes<32U>();
    require(std::all_of(result.dense_trailer.begin() + 4,
                        result.dense_trailer.end(),
                        [](std::uint8_t value) { return value == 0U; }),
            ErrorCode::InvalidValue,
            "reserved mining configuration bytes must be zero");
    require(result.dense_trailer[0] == 0U && result.dense_trailer[1] == 0U
                && result.dense_trailer[2] == 0U && result.dense_trailer[3] == 0U,
            ErrorCode::InvalidValue,
            "P1 supports dense configuration only");
    require_no_remaining(reader, "mining configuration");
    const auto canonical = result.to_bytes();
    require(std::equal(canonical.begin(), canonical.end(), bytes.begin(), bytes.end()),
            ErrorCode::NonCanonical,
            "mining configuration is not canonical");
    return result;
}

std::size_t MiningConfiguration::dot_product_length() const
{
    require(rank != 0U, ErrorCode::InvalidValue, "rank cannot be zero");
    return static_cast<std::size_t>(common_dim)
        - (static_cast<std::size_t>(common_dim) % static_cast<std::size_t>(rank));
}

void validate_configuration(const MiningConfiguration& config,
                            std::uint32_t m,
                            std::uint32_t n,
                            std::uint32_t t_rows,
                            std::uint32_t t_cols,
                            ValidationProfile profile)
{
    require(config.mma_type == 0U,
            ErrorCode::InvalidValue,
            "only Int7xInt7ToInt32 MMA type is supported");
    require(config.rank >= 32U && config.rank <= 1024U
                && (config.rank & (config.rank - 1U)) == 0U
                && config.rank % 16U == 0U,
            ErrorCode::InvalidShape,
            "rank must be a power of two from 32 through 1024 and divisible by 16");
    if (profile == ValidationProfile::PinnedCurrent) {
        require(config.rank >= kNoiseRank,
                ErrorCode::InvalidShape,
                "current Pearl rank floor is 128");
    }
    require(config.common_dim >= 1024U && config.common_dim <= kMaxCommonDimension
                && config.common_dim % 64U == 0U,
            ErrorCode::InvalidShape,
            "common dimension must be 1024..65536 and divisible by 64");
    const std::uint64_t rank = config.rank;
    require(16U * rank <= config.common_dim && config.common_dim <= 4U * rank * rank,
            ErrorCode::InvalidShape,
            "common dimension is outside the rank-dependent bounds");
    const std::size_t dot_length = config.dot_product_length();
    require(dot_length % kTileDwordBytes == 0U,
            ErrorCode::InvalidShape,
            "dot product length must be divisible by the circuit dword size");
    const std::vector<std::uint32_t> rows = config.rows_pattern.indices();
    const std::vector<std::uint32_t> cols = config.cols_pattern.indices();
    require(rows.size() % 2U == 0U && cols.size() % 2U == 0U,
            ErrorCode::InvalidShape,
            "hash pattern dimensions must be even");
    const std::uint64_t tile_area = static_cast<std::uint64_t>(rows.size()) * cols.size();
    require(tile_area >= 32U && tile_area <= 256U,
            ErrorCode::InvalidShape,
            "hash pattern area must be 32..256");
    require(m != 0U && n != 0U && m <= kMaxMatrixDimension && n <= kMaxMatrixDimension,
            ErrorCode::InvalidShape,
            "matrix dimensions must be nonzero and at most 2^24");
    require(config.rows_pattern.offset_is_valid(t_rows)
                && config.cols_pattern.offset_is_valid(t_cols),
            ErrorCode::InvalidValue,
            "tile offsets are not valid for their periodic patterns");
    require(static_cast<std::uint64_t>(t_rows) + config.rows_pattern.max_index() < m
                && static_cast<std::uint64_t>(t_cols) + config.cols_pattern.max_index() < n,
            ErrorCode::OutOfBounds,
            "selected pattern indices exceed matrix dimensions");
    const std::uint64_t worker_input =
        (static_cast<std::uint64_t>(rows.size()) + cols.size()) * dot_length;
    require(worker_input <= kWorkerInputLimit,
            ErrorCode::InvalidShape,
            "worker input exceeds the 2^22-byte bound");
}

std::array<std::uint8_t, kHeaderBytes> serialize_header(const IncompleteBlockHeader& header)
{
    std::vector<std::uint8_t> bytes;
    bytes.reserve(kHeaderBytes);
    append_u32(bytes, header.version);
    bytes.insert(bytes.end(), header.prev_block.rbegin(), header.prev_block.rend());
    bytes.insert(bytes.end(), header.merkle_root.rbegin(), header.merkle_root.rend());
    append_u32(bytes, header.timestamp);
    append_u32(bytes, header.nbits);
    require(bytes.size() == kHeaderBytes, ErrorCode::InvalidLength, "header has unexpected serialized size");
    std::array<std::uint8_t, kHeaderBytes> result{};
    std::copy(bytes.begin(), bytes.end(), result.begin());
    return result;
}

IncompleteBlockHeader deserialize_header(std::span<const std::uint8_t> bytes)
{
    require(bytes.size() == kHeaderBytes, ErrorCode::InvalidLength, "header must be 76 bytes");
    Reader reader(bytes);
    IncompleteBlockHeader result;
    result.version = reader.u32();
    const auto wire_prev = reader.fixed_bytes<kDigestBytes>();
    const auto wire_merkle = reader.fixed_bytes<kDigestBytes>();
    std::copy(wire_prev.rbegin(), wire_prev.rend(), result.prev_block.begin());
    std::copy(wire_merkle.rbegin(), wire_merkle.rend(), result.merkle_root.begin());
    result.timestamp = reader.u32();
    result.nbits = reader.u32();
    require_no_remaining(reader, "header");
    const auto canonical = serialize_header(result);
    require(std::equal(canonical.begin(), canonical.end(), bytes.begin(), bytes.end()),
            ErrorCode::NonCanonical,
            "header does not round-trip canonically");
    return result;
}

Int8Matrix::Int8Matrix(std::size_t rows, std::size_t cols, std::vector<std::int8_t> values)
    : rows_(rows),
      cols_(cols),
      values_(std::move(values))
{
    require(rows != 0U && cols != 0U, ErrorCode::InvalidShape, "int8 matrix dimensions must be nonzero");
    require(values_.size() == checked_product(rows, cols, "int8 matrix size"),
            ErrorCode::InvalidLength,
            "int8 matrix value count does not match dimensions");
}

std::int8_t Int8Matrix::at(std::size_t row, std::size_t col) const
{
    require(row < rows_ && col < cols_, ErrorCode::OutOfBounds, "int8 matrix index is out of bounds");
    return values_[row * cols_ + col];
}

std::int8_t& Int8Matrix::at(std::size_t row, std::size_t col)
{
    require(row < rows_ && col < cols_, ErrorCode::OutOfBounds, "int8 matrix index is out of bounds");
    return values_[row * cols_ + col];
}

std::vector<std::uint8_t> Int8Matrix::raw_bytes() const
{
    std::vector<std::uint8_t> result;
    result.reserve(values_.size());
    for (const std::int8_t value : values_) {
        result.push_back(std::bit_cast<std::uint8_t>(value));
    }
    return result;
}

void Int8Matrix::require_signal_range() const
{
    for (const std::int8_t value : values_) {
        const std::int32_t widened = value;
        require(widened >= kSignalMin && widened <= kSignalMax,
                ErrorCode::InvalidValue,
                "matrix value is outside the [-64,64] mining signal range");
    }
}

namespace {

[[nodiscard]] float round_to_even(float value)
{
    const float lower = std::floor(value);
    const float fraction = value - lower;
    if (fraction < 0.5F) {
        return lower;
    }
    if (fraction > 0.5F) {
        return lower + 1.0F;
    }
    const float parity = std::fmod(std::fabs(lower), 2.0F);
    return parity == 0.0F ? lower : lower + 1.0F;
}

} // namespace

QuantizedMatrix quantize_fp32(std::span<const float> values,
                              std::size_t rows,
                              std::size_t cols)
{
    require(rows != 0U && cols != 0U, ErrorCode::InvalidShape, "quantized matrix dimensions must be nonzero");
    require(values.size() == checked_product(rows, cols, "quantized matrix size"),
            ErrorCode::InvalidLength,
            "quantized input value count does not match dimensions");
    std::vector<std::int8_t> quantized(values.size(), 0);
    std::vector<float> scales(rows, 0.0F);
    for (std::size_t row = 0U; row < rows; ++row) {
        float max_abs = 0.0F;
        for (std::size_t col = 0U; col < cols; ++col) {
            const float value = values[row * cols + col];
            require(std::isfinite(value), ErrorCode::InvalidValue, "quantization input is not finite");
            max_abs = std::max(max_abs, std::fabs(value));
        }
        if (max_abs == 0.0F) {
            continue;
        }
        const float scale = max_abs / static_cast<float>(kQuantizationMax);
        scales[row] = scale;
        for (std::size_t col = 0U; col < cols; ++col) {
            const float scaled = values[row * cols + col] / scale;
            const float rounded = round_to_even(scaled);
            const float clamped = std::clamp(rounded,
                                             -static_cast<float>(kQuantizationMax),
                                             static_cast<float>(kQuantizationMax));
            quantized[row * cols + col] = static_cast<std::int8_t>(clamped);
        }
    }
    Int8Matrix matrix(rows, cols, std::move(quantized));
    matrix.require_signal_range();
    return QuantizedMatrix{std::move(matrix), std::move(scales)};
}

Digest blake3_keyed(const Digest& key, std::span<const std::uint8_t> data)
{
    Digest digest{};
    const int result = pearl_blake3_hash_keyed(key.data(), data.data(), data.size(), digest.data());
    require(result == 0, ErrorCode::InvalidValue, "BLAKE3 keyed FFI call failed");
    return digest;
}

Digest blake3_chunk_cv(const Digest& key,
                       std::span<const std::uint8_t> data,
                       std::uint64_t chunk_index)
{
    require(data.size() <= kMerkleChunkBytes,
            ErrorCode::InvalidLength,
            "BLAKE3 chunk input exceeds 1024 bytes");
    Digest digest{};
    const int result = pearl_blake3_chunk_cv(
        key.data(), data.data(), data.size(), chunk_index, digest.data());
    require(result == 0, ErrorCode::InvalidValue, "BLAKE3 chunk CV FFI call failed");
    return digest;
}

Digest blake3_parent_cv(const Digest& key,
                        const Digest& left,
                        const Digest& right,
                        bool root)
{
    Digest digest{};
    const int result = pearl_blake3_parent_cv(
        key.data(), left.data(), right.data(), root, digest.data());
    require(result == 0, ErrorCode::InvalidValue, "BLAKE3 parent CV FFI call failed");
    return digest;
}

Digest job_key(const IncompleteBlockHeader& header, const MiningConfiguration& config)
{
    const auto header_bytes = serialize_header(header);
    const auto config_bytes = config.to_bytes();
    std::vector<std::uint8_t> input;
    input.reserve(header_bytes.size() + config_bytes.size());
    input.insert(input.end(), header_bytes.begin(), header_bytes.end());
    input.insert(input.end(), config_bytes.begin(), config_bytes.end());
    return hash_unkeyed(input);
}

CommitmentSeeds commitment_seeds(const Digest& key,
                                 const Digest& hash_a,
                                 const Digest& hash_b)
{
    std::vector<std::uint8_t> input;
    input.reserve(64U);
    append_digest(input, key);
    append_digest(input, hash_b);
    const Digest b_noise_seed = hash_unkeyed(input);
    input.clear();
    append_digest(input, b_noise_seed);
    append_digest(input, hash_a);
    return CommitmentSeeds{b_noise_seed, hash_unkeyed(input)};
}

namespace {

constexpr std::array<std::uint8_t, kDigestBytes> kSeedLabelA = {
    static_cast<std::uint8_t>('A'), static_cast<std::uint8_t>('_'), static_cast<std::uint8_t>('t'),
    static_cast<std::uint8_t>('e'), static_cast<std::uint8_t>('n'), static_cast<std::uint8_t>('s'),
    static_cast<std::uint8_t>('o'), static_cast<std::uint8_t>('r'),
};
constexpr std::array<std::uint8_t, kDigestBytes> kSeedLabelB = {
    static_cast<std::uint8_t>('B'), static_cast<std::uint8_t>('_'), static_cast<std::uint8_t>('t'),
    static_cast<std::uint8_t>('e'), static_cast<std::uint8_t>('n'), static_cast<std::uint8_t>('s'),
    static_cast<std::uint8_t>('o'), static_cast<std::uint8_t>('r'),
};

[[nodiscard]] Digest random_hash(std::size_t index,
                                 const std::array<std::uint8_t, kDigestBytes>& seed,
                                 const Digest& key,
                                 std::size_t prepend_index)
{
    require(prepend_index < 8U, ErrorCode::InvalidValue, "noise prepend index is out of bounds");
    std::vector<std::uint8_t> message(64U, 0U);
    const std::uint32_t value = checked_u32(static_cast<std::uint64_t>(index) + 1U,
                                            "noise hash index");
    const std::size_t offset = prepend_index * 4U;
    for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
        message[offset + shift / 8U] = static_cast<std::uint8_t>((value >> shift) & 0xFFU);
    }
    std::copy(seed.begin(), seed.end(), message.begin() + 32);
    return blake3_keyed(key, message);
}

[[nodiscard]] std::int8_t uniform_noise_byte(std::uint8_t value)
{
    const std::int32_t translated = static_cast<std::int32_t>(value & 63U) - 32;
    return static_cast<std::int8_t>(translated);
}

[[nodiscard]] std::vector<std::int8_t> uniform_row(
    std::size_t row_index,
    std::size_t column_count,
    const std::array<std::uint8_t, kDigestBytes>& seed,
    const Digest& key)
{
    const std::size_t start = checked_product(row_index, column_count, "noise row offset");
    const std::size_t end = start + column_count;
    const std::size_t first_block = start / kDigestBytes;
    const std::size_t last_block = (end + kDigestBytes - 1U) / kDigestBytes;
    std::vector<std::int8_t> result;
    result.reserve(column_count);
    for (std::size_t block = first_block; block < last_block; ++block) {
        const Digest hash = random_hash(block, seed, key, 0U);
        for (std::size_t byte_index = 0U; byte_index < hash.size(); ++byte_index) {
            const std::size_t global_index = block * kDigestBytes + byte_index;
            if (global_index >= start && global_index < end) {
                result.push_back(uniform_noise_byte(hash[byte_index]));
            }
        }
    }
    require(result.size() == column_count,
            ErrorCode::InvalidLength,
            "uniform noise row has an unexpected length");
    return result;
}

struct SparsePair {
    std::uint32_t first = 0U;
    std::uint32_t second = 0U;
};

[[nodiscard]] std::vector<SparsePair> permutation_pairs(std::size_t k,
                                                         std::size_t rank,
                                                         const std::array<std::uint8_t, kDigestBytes>& seed,
                                                         const Digest& key)
{
    require(rank != 0U && (rank & (rank - 1U)) == 0U,
            ErrorCode::InvalidShape,
            "noise rank must be a power of two");
    require(rank <= 128U && rank % kDigestBytes == 0U,
            ErrorCode::InvalidShape,
            "noise rank must be at most 128 and divisible by 32");
    std::vector<SparsePair> result(k);
    const std::uint32_t mask = static_cast<std::uint32_t>(rank - 1U);
    for (std::size_t block = 0U; block < (k + 7U) / 8U; ++block) {
        const Digest hash = random_hash(block, seed, key, 1U);
        const std::size_t count = std::min<std::size_t>(8U, k - block * 8U);
        for (std::size_t index = 0U; index < count; ++index) {
            const std::size_t offset = index * 4U;
            const std::uint32_t random = static_cast<std::uint32_t>(hash[offset])
                | (static_cast<std::uint32_t>(hash[offset + 1U]) << 8U)
                | (static_cast<std::uint32_t>(hash[offset + 2U]) << 16U)
                | (static_cast<std::uint32_t>(hash[offset + 3U]) << 24U);
            const std::uint32_t first = random & mask;
            const std::uint32_t second = first ^ (1U + mul_hi_u32(static_cast<std::uint32_t>(rank - 1U), random));
            result[block * 8U + index] = SparsePair{first, second};
        }
    }
    return result;
}

[[nodiscard]] Int8Matrix make_sparse_e_ar(const std::vector<SparsePair>& pairs,
                                          std::size_t rank)
{
    std::vector<std::int8_t> values(checked_product(rank, pairs.size(), "E_AR size"), 0);
    for (std::size_t column = 0U; column < pairs.size(); ++column) {
        values[pairs[column].first * pairs.size() + column] = 1;
        values[pairs[column].second * pairs.size() + column] = -1;
    }
    return Int8Matrix(rank, pairs.size(), std::move(values));
}

[[nodiscard]] Int8Matrix make_sparse_e_bl(const std::vector<SparsePair>& pairs,
                                          std::size_t rank)
{
    std::vector<std::int8_t> values(checked_product(pairs.size(), rank, "E_BL size"), 0);
    for (std::size_t row = 0U; row < pairs.size(); ++row) {
        values[row * rank + pairs[row].first] = 1;
        values[row * rank + pairs[row].second] = -1;
    }
    return Int8Matrix(pairs.size(), rank, std::move(values));
}

[[nodiscard]] std::vector<std::int8_t> matrix_product_i8(const Int8Matrix& left,
                                                         const Int8Matrix& right,
                                                         const char* label)
{
    require(left.cols() == right.rows(), ErrorCode::InvalidShape, std::string(label) + " dimensions mismatch");
    std::vector<std::int8_t> result(checked_product(left.rows(), right.cols(), label), 0);
    for (std::size_t row = 0U; row < left.rows(); ++row) {
        for (std::size_t col = 0U; col < right.cols(); ++col) {
            std::int64_t accumulator = 0;
            for (std::size_t inner = 0U; inner < left.cols(); ++inner) {
                accumulator += static_cast<std::int32_t>(left.at(row, inner))
                    * static_cast<std::int32_t>(right.at(inner, col));
            }
            const std::int32_t value = checked_i32(accumulator, label);
            require(value >= -64 && value <= 64,
                    ErrorCode::InvalidValue,
                    std::string(label) + " noise value is outside [-64,64]");
            result[row * right.cols() + col] = static_cast<std::int8_t>(value);
        }
    }
    return result;
}

} // namespace

NoiseMatrices generate_noise(std::size_t k,
                             std::size_t rank,
                             const CommitmentSeeds& seeds,
                             std::span<const std::size_t> a_rows,
                             std::span<const std::size_t> b_columns)
{
    require(k != 0U && rank != 0U, ErrorCode::InvalidShape, "noise dimensions must be nonzero");
    require(rank <= 128U && rank % kDigestBytes == 0U && (rank & (rank - 1U)) == 0U,
            ErrorCode::InvalidShape,
            "noise rank must be a power of two, divisible by 32, and at most 128");
    for (const std::size_t row : a_rows) {
        (void)row;
    }
    std::vector<std::int8_t> e_al_values;
    e_al_values.reserve(checked_product(a_rows.size(), rank, "E_AL size"));
    for (const std::size_t row : a_rows) {
        const auto values = uniform_row(row, rank, kSeedLabelA, seeds.a_noise_seed);
        e_al_values.insert(e_al_values.end(), values.begin(), values.end());
    }
    std::vector<std::int8_t> e_br_values(checked_product(rank, b_columns.size(), "E_BR size"), 0);
    for (std::size_t column_index = 0U; column_index < b_columns.size(); ++column_index) {
        const std::size_t column = b_columns[column_index];
        const auto values = uniform_row(column, rank, kSeedLabelB, seeds.b_noise_seed);
        for (std::size_t row = 0U; row < values.size(); ++row) {
            e_br_values[row * b_columns.size() + column_index] = values[row];
        }
    }
    const auto e_ar_pairs = permutation_pairs(k, rank, kSeedLabelA, seeds.a_noise_seed);
    const auto e_bl_pairs = permutation_pairs(k, rank, kSeedLabelB, seeds.b_noise_seed);
    NoiseMatrices result{
        Int8Matrix(a_rows.size(), rank, std::move(e_al_values)),
        make_sparse_e_ar(e_ar_pairs, rank),
        make_sparse_e_bl(e_bl_pairs, rank),
        Int8Matrix(rank, b_columns.size(), std::move(e_br_values)),
        Int8Matrix(),
        Int8Matrix(),
    };
    result.noise_a = Int8Matrix(
        result.e_al.rows(),
        result.e_ar.cols(),
        matrix_product_i8(result.e_al, result.e_ar, "E_A"));

    // E_BR is stored rank × selected_columns; E_BL is k × rank. The source
    // protocol exposes E_B transposed as selected_columns × k.
    std::vector<std::int8_t> noise_b_values(
        checked_product(result.e_br.cols(), result.e_bl.rows(), "E_B size"),
        0);
    for (std::size_t column = 0U; column < result.e_br.cols(); ++column) {
        for (std::size_t row = 0U; row < result.e_bl.rows(); ++row) {
            std::int64_t accumulator = 0;
            for (std::size_t inner = 0U; inner < rank; ++inner) {
                accumulator += static_cast<std::int32_t>(result.e_br.at(inner, column))
                    * static_cast<std::int32_t>(result.e_bl.at(row, inner));
            }
            const std::int32_t value = checked_i32(accumulator, "E_B");
            require(value >= -64 && value <= 64,
                    ErrorCode::InvalidValue,
                    "E_B noise value is outside [-64,64]");
            noise_b_values[column * result.e_bl.rows() + row] = static_cast<std::int8_t>(value);
        }
    }
    result.noise_b_transposed = Int8Matrix(
        result.e_br.cols(), result.e_bl.rows(), std::move(noise_b_values));
    return result;
}

NoisedOperands make_noised_operands(const Int8Matrix& a,
                                    const Int8Matrix& b,
                                    const NoiseMatrices& noise)
{
    a.require_signal_range();
    b.require_signal_range();
    require(a.rows() == noise.noise_a.rows() && a.cols() == noise.noise_a.cols(),
            ErrorCode::InvalidShape,
            "A dimensions do not match selected A noise");
    require(b.rows() == noise.noise_b_transposed.cols()
                && b.cols() == noise.noise_b_transposed.rows(),
            ErrorCode::InvalidShape,
            "B dimensions do not match selected B noise");
    std::vector<std::int8_t> a_values(a.values().size(), 0);
    for (std::size_t row = 0U; row < a.rows(); ++row) {
        for (std::size_t col = 0U; col < a.cols(); ++col) {
            const std::int32_t value = static_cast<std::int32_t>(a.at(row, col))
                + static_cast<std::int32_t>(noise.noise_a.at(row, col));
            require(value >= std::numeric_limits<std::int8_t>::min()
                        && value <= std::numeric_limits<std::int8_t>::max(),
                    ErrorCode::ArithmeticOverflow,
                    "noised A value exceeds int8");
            a_values[row * a.cols() + col] = static_cast<std::int8_t>(value);
        }
    }
    std::vector<std::int8_t> b_values(b.values().size(), 0);
    for (std::size_t row = 0U; row < b.rows(); ++row) {
        for (std::size_t col = 0U; col < b.cols(); ++col) {
            const std::int32_t value = static_cast<std::int32_t>(b.at(row, col))
                + static_cast<std::int32_t>(noise.noise_b_transposed.at(col, row));
            require(value >= std::numeric_limits<std::int8_t>::min()
                        && value <= std::numeric_limits<std::int8_t>::max(),
                    ErrorCode::ArithmeticOverflow,
                    "noised B value exceeds int8");
            b_values[row * b.cols() + col] = static_cast<std::int8_t>(value);
        }
    }
    return NoisedOperands{
        Int8Matrix(a.rows(), a.cols(), std::move(a_values)),
        Int8Matrix(b.rows(), b.cols(), std::move(b_values)),
    };
}

Int32Matrix::Int32Matrix(std::size_t rows,
                         std::size_t cols,
                         std::vector<std::int32_t> values)
    : rows_(rows),
      cols_(cols),
      values_(std::move(values))
{
    require(rows != 0U && cols != 0U, ErrorCode::InvalidShape, "int32 matrix dimensions must be nonzero");
    require(values_.size() == checked_product(rows, cols, "int32 matrix size"),
            ErrorCode::InvalidLength,
            "int32 matrix value count does not match dimensions");
}

std::int32_t Int32Matrix::at(std::size_t row, std::size_t col) const
{
    require(row < rows_ && col < cols_, ErrorCode::OutOfBounds, "int32 matrix index is out of bounds");
    return values_[row * cols_ + col];
}

std::int32_t& Int32Matrix::at(std::size_t row, std::size_t col)
{
    require(row < rows_ && col < cols_, ErrorCode::OutOfBounds, "int32 matrix index is out of bounds");
    return values_[row * cols_ + col];
}

Int32Matrix gemm_checked(const Int8Matrix& left, const Int8Matrix& right)
{
    require(left.cols() == right.rows(), ErrorCode::InvalidShape, "GEMM inner dimensions differ");
    std::vector<std::int32_t> result(checked_product(left.rows(), right.cols(), "GEMM result size"), 0);
    for (std::size_t row = 0U; row < left.rows(); ++row) {
        for (std::size_t col = 0U; col < right.cols(); ++col) {
            std::int64_t accumulator = 0;
            for (std::size_t inner = 0U; inner < left.cols(); ++inner) {
                accumulator += static_cast<std::int32_t>(left.at(row, inner))
                    * static_cast<std::int32_t>(right.at(inner, col));
            }
            result[row * right.cols() + col] = checked_i32(accumulator, "GEMM accumulator");
        }
    }
    return Int32Matrix(left.rows(), right.cols(), std::move(result));
}

Int32Matrix noised_gemm(const Int8Matrix& a,
                        const Int8Matrix& b,
                        const NoiseMatrices& noise)
{
    const NoisedOperands operands = make_noised_operands(a, b, noise);
    return gemm_checked(operands.a, operands.b);
}

Int32Matrix denoise_product_checked(const Int8Matrix& a,
                                    const Int8Matrix& b,
                                    const NoiseMatrices& noise,
                                    const Int32Matrix& noised_product)
{
    require(a.rows() == noise.noise_a.rows() && a.cols() == noise.noise_a.cols(),
            ErrorCode::InvalidShape,
            "A dimensions do not match denoising noise");
    require(b.rows() == noise.noise_b_transposed.cols()
                && b.cols() == noise.noise_b_transposed.rows(),
            ErrorCode::InvalidShape,
            "B dimensions do not match denoising noise");
    require(noised_product.rows() == a.rows() && noised_product.cols() == b.cols(),
            ErrorCode::InvalidShape,
            "noised product dimensions do not match operands");
    std::vector<std::int32_t> result(checked_product(a.rows(), b.cols(), "denoised result size"), 0);
    for (std::size_t row = 0U; row < a.rows(); ++row) {
        for (std::size_t col = 0U; col < b.cols(); ++col) {
            std::int64_t correction = 0;
            for (std::size_t inner = 0U; inner < a.cols(); ++inner) {
                const std::int32_t a_value = static_cast<std::int32_t>(a.at(row, inner));
                const std::int32_t b_value = static_cast<std::int32_t>(b.at(inner, col));
                const std::int32_t e_a = static_cast<std::int32_t>(noise.noise_a.at(row, inner));
                const std::int32_t e_b = static_cast<std::int32_t>(noise.noise_b_transposed.at(col, inner));
                correction += static_cast<std::int64_t>(a_value) * e_b;
                correction += static_cast<std::int64_t>(e_a) * b_value;
                correction += static_cast<std::int64_t>(e_a) * e_b;
            }
            const std::int64_t value = static_cast<std::int64_t>(noised_product.at(row, col)) - correction;
            result[row * b.cols() + col] = checked_i32(value, "denoised GEMM accumulator");
        }
    }
    return Int32Matrix(a.rows(), b.cols(), std::move(result));
}

TranscriptResult selected_transcript(const Int8Matrix& noised_a,
                                     const Int8Matrix& noised_b,
                                     const Int32Matrix& noised_product,
                                     std::size_t rank)
{
    require(rank != 0U, ErrorCode::InvalidShape, "transcript rank cannot be zero");
    require(noised_a.cols() == noised_b.rows(),
            ErrorCode::InvalidShape,
            "transcript operands have different inner dimensions");
    require(noised_product.rows() == noised_a.rows()
                && noised_product.cols() == noised_b.cols(),
            ErrorCode::InvalidShape,
            "transcript product dimensions do not match operands");
    const bool packed_selected_tile = noised_a.rows() <= 8U || noised_b.cols() < 250U;
    require((packed_selected_tile && noised_a.rows() >= kSelectedRows
             && noised_b.cols() >= kSelectedColumns)
                || (!packed_selected_tile && noised_a.rows() > 8U
                    && noised_b.cols() >= 250U),
            ErrorCode::OutOfBounds,
            "transcript requires either the packed 2x64 tile or full rows [0,8] and columns [8j,8j+1]");
    const std::size_t full_chunks = noised_a.cols() / rank;
    require(full_chunks != 0U, ErrorCode::InvalidShape, "transcript has no full-rank chunk");
    TranscriptResult result;
    std::array<std::array<std::int64_t, kSelectedColumns>, kSelectedRows> accumulated{};
    for (std::size_t chunk = 0U; chunk < full_chunks; ++chunk) {
        const std::size_t begin = chunk * rank;
        for (std::size_t row_index = 0U; row_index < kSelectedRows; ++row_index) {
            const std::size_t row = packed_selected_tile ? row_index : (row_index == 0U ? 0U : 8U);
            for (std::size_t column = 0U; column < kSelectedColumns; ++column) {
                const std::size_t selected_column = packed_selected_tile
                    ? column
                    : (column / 2U) * 8U + (column % 2U);
                for (std::size_t inner = begin; inner < begin + rank; ++inner) {
                    accumulated[row_index][column] +=
                        static_cast<std::int32_t>(noised_a.at(row, inner))
                        * static_cast<std::int32_t>(noised_b.at(inner, selected_column));
                }
            }
        }
        std::uint32_t combined = 0U;
        for (const auto& row : accumulated) {
            for (const std::int64_t value : row) {
                combined ^= as_u32(checked_i32(value, "transcript selected value"));
            }
        }
        const std::size_t transcript_index = chunk % kTranscriptWords;
        result.words[transcript_index] = rotl32(result.words[transcript_index], kTranscriptRotation) ^ combined;
        TranscriptStep step;
        step.reduction_index = checked_u32(chunk, "transcript reduction index");
        step.combined_xor = combined;
        step.state = result.words;
        result.trace.push_back(step);
    }
    for (std::size_t row_index = 0U; row_index < kSelectedRows; ++row_index) {
        const std::size_t row = packed_selected_tile ? row_index : (row_index == 0U ? 0U : 8U);
        for (std::size_t column = 0U; column < kSelectedColumns; ++column) {
            const std::size_t selected_column = packed_selected_tile
                ? column
                : (column / 2U) * 8U + (column % 2U);
            const std::int32_t expected = checked_i32(
                accumulated[row_index][column], "transcript final selected value");
            require(noised_product.at(row, selected_column) == expected,
                    ErrorCode::InvalidValue,
                    "transcript product does not match the accumulated selected data");
        }
    }
    return result;
}

Digest jackpot_hash(const std::array<std::uint32_t, kTranscriptWords>& transcript,
                    const Digest& commitment_hash)
{
    std::vector<std::uint8_t> bytes;
    bytes.reserve(kTranscriptWords * sizeof(std::uint32_t));
    for (const std::uint32_t word : transcript) {
        append_u32(bytes, word);
    }
    return blake3_keyed(commitment_hash, bytes);
}

Digest target_from_bytes(std::span<const std::uint8_t> bytes)
{
    require(bytes.size() == kDigestBytes,
            ErrorCode::InvalidLength,
            "target must be exactly 32 little-endian bytes");
    Digest target{};
    std::copy(bytes.begin(), bytes.end(), target.begin());
    return target;
}

bool jackpot_meets_target(const Digest& jackpot, const Digest& target)
{
    // Both values are little-endian 256-bit integers, so compare the most
    // significant byte first while retaining the exact wire byte order.
    for (std::size_t index = kDigestBytes; index != 0U; --index) {
        const std::uint8_t hash_byte = jackpot[index - 1U];
        const std::uint8_t target_byte = target[index - 1U];
        if (hash_byte < target_byte) {
            return true;
        }
        if (hash_byte > target_byte) {
            return false;
        }
    }
    return true; // Pinned Pearl uses <=, including equality.
}

Digest merkle_root(std::span<const std::uint8_t> data, const Digest& key)
{
    const MerkleLayers tree = build_merkle_layers(data, key);
    if (tree.layers.empty()) {
        return Digest{};
    }
    return tree.layers.back().front();
}

MatrixOpening open_matrix_rows(const Int8Matrix& matrix,
                               const Digest& key,
                               std::span<const std::size_t> rows)
{
    require(!rows.empty(), ErrorCode::InvalidLength, "matrix opening cannot have no rows");
    require(std::is_sorted(rows.begin(), rows.end()),
            ErrorCode::NonCanonical,
            "matrix opening rows must be sorted");
    require(std::adjacent_find(rows.begin(), rows.end()) == rows.end(),
            ErrorCode::NonCanonical,
            "matrix opening rows must be unique");
    for (const std::size_t row : rows) {
        require(row < matrix.rows(), ErrorCode::OutOfBounds, "matrix opening row is out of bounds");
    }
    const std::vector<std::uint8_t> raw = matrix.raw_bytes();
    const MerkleLayers tree = build_merkle_layers(raw, key);
    require(!tree.layers.empty(), ErrorCode::InvalidShape, "cannot open an empty Merkle tree");
    std::set<std::size_t> leaf_set;
    for (const std::size_t row : rows) {
        const std::size_t row_start = checked_product(row, matrix.cols(), "matrix row offset");
        const std::size_t row_end = checked_product(row + 1U, matrix.cols(), "matrix row end");
        const std::size_t first = row_start / kMerkleChunkBytes;
        const std::size_t last = (row_end - 1U) / kMerkleChunkBytes;
        for (std::size_t leaf = first; leaf <= last; ++leaf) {
            leaf_set.insert(leaf);
        }
    }
    MatrixOpening result;
    result.row_indices.assign(rows.begin(), rows.end());
    result.proof.total_leaves = tree.layers.front().size();
    result.proof.leaf_indices.assign(leaf_set.begin(), leaf_set.end());
    for (const std::size_t leaf : result.proof.leaf_indices) {
        std::array<std::uint8_t, kMerkleChunkBytes> data_chunk{};
        const auto* begin = tree.padded_data.data() + leaf * kMerkleChunkBytes;
        std::copy_n(begin, kMerkleChunkBytes, data_chunk.begin());
        result.proof.leaf_data.push_back(data_chunk);
    }
    result.proof.root = tree.layers.back().front();

    std::set<std::size_t> current(leaf_set.begin(), leaf_set.end());
    std::size_t level_len = result.proof.total_leaves;
    std::size_t level = 0U;
    while (level_len > 1U && !current.empty()) {
        const auto& level_nodes = tree.layers[level];
        for (const std::size_t index : current) {
            if ((index & 1U) != 0U) {
                if (!current.contains(index - 1U)) {
                    result.proof.siblings.push_back(level_nodes[index - 1U]);
                }
            } else if (index + 1U < level_len && !current.contains(index + 1U)) {
                result.proof.siblings.push_back(level_nodes[index + 1U]);
            }
        }
        std::set<std::size_t> next;
        for (const std::size_t index : current) {
            next.insert(index / 2U);
        }
        current = std::move(next);
        level_len = (level_len + 1U) / 2U;
        ++level;
    }
    validate_merkle_shape(result.proof);
    require(verify_merkle_proof(result.proof, key),
            ErrorCode::InvalidValue,
            "newly constructed matrix opening does not verify");
    return result;
}

bool verify_merkle_proof(const MerkleProof& proof, const Digest& key)
{
    try {
        validate_merkle_shape(proof);
        std::map<std::size_t, Digest> current;
        for (std::size_t index = 0U; index < proof.leaf_indices.size(); ++index) {
            const Digest leaf = proof.total_leaves == 1U
                ? blake3_keyed(key, proof.leaf_data[index])
                : blake3_chunk_cv(key,
                                  proof.leaf_data[index],
                                  static_cast<std::uint64_t>(proof.leaf_indices[index]));
            current.emplace(proof.leaf_indices[index], leaf);
        }
        std::size_t sibling_index = 0U;
        std::size_t level_len = proof.total_leaves;
        if (level_len == 1U) {
            return proof.leaf_indices.front() == 0U && proof.siblings.empty()
                && current.begin()->second == proof.root;
        }
        while (level_len > 2U) {
            std::map<std::size_t, Digest> next;
            for (const auto& [index, value] : current) {
                if ((index & 1U) != 0U && current.contains(index - 1U)) {
                    continue;
                }
                if ((index & 1U) == 0U) {
                    if (current.contains(index + 1U)) {
                        next.emplace(index / 2U,
                                     blake3_parent_cv(key,
                                                      value,
                                                      current.at(index + 1U),
                                                      false));
                    } else if (index + 1U < level_len) {
                        if (sibling_index >= proof.siblings.size()) {
                            return false;
                        }
                        next.emplace(index / 2U,
                                     blake3_parent_cv(key,
                                                      value,
                                                      proof.siblings[sibling_index++],
                                                      false));
                    } else {
                        next.emplace(index / 2U, value);
                    }
                } else {
                    if (sibling_index >= proof.siblings.size()) {
                        return false;
                    }
                    next.emplace(index / 2U,
                                 blake3_parent_cv(key,
                                                  proof.siblings[sibling_index++],
                                                  value,
                                                  false));
                }
            }
            current = std::move(next);
            level_len = (level_len + 1U) / 2U;
        }
        Digest left{};
        Digest right{};
        const auto left_iterator = current.find(0U);
        if (left_iterator != current.end()) {
            left = left_iterator->second;
        } else {
            if (sibling_index >= proof.siblings.size()) {
                return false;
            }
            left = proof.siblings[sibling_index++];
        }
        const auto right_iterator = current.find(1U);
        if (right_iterator != current.end()) {
            right = right_iterator->second;
        } else {
            if (sibling_index >= proof.siblings.size()) {
                return false;
            }
            right = proof.siblings[sibling_index++];
        }
        return sibling_index == proof.siblings.size()
            && blake3_parent_cv(key, left, right, true) == proof.root;
    } catch (const Error&) {
        return false;
    }
}

std::vector<std::uint8_t> extract_opening_bytes(const MerkleProof& proof,
                                                std::size_t offset,
                                                std::size_t length)
{
    validate_merkle_shape(proof);
    require(length <= std::numeric_limits<std::size_t>::max() - offset,
            ErrorCode::InvalidLength,
            "opening extraction range overflows");
    if (length == 0U) {
        return {};
    }
    const std::size_t end = offset + length;
    std::vector<std::uint8_t> result(length, 0U);
    std::size_t copied = 0U;
    for (std::size_t index = 0U; index < proof.leaf_indices.size(); ++index) {
        const std::size_t leaf_start = checked_product(proof.leaf_indices[index],
                                                       kMerkleChunkBytes,
                                                       "opening leaf offset");
        const std::size_t leaf_end = leaf_start + kMerkleChunkBytes;
        if (leaf_start < end && offset < leaf_end) {
            const std::size_t copy_start = std::max(offset, leaf_start);
            const std::size_t copy_end = std::min(end, leaf_end);
            const std::size_t count = copy_end - copy_start;
            std::copy_n(proof.leaf_data[index].begin()
                            + static_cast<std::ptrdiff_t>(copy_start - leaf_start),
                        count,
                        result.begin() + static_cast<std::ptrdiff_t>(copy_start - offset));
            copied += count;
        }
    }
    require(copied == length,
            ErrorCode::OutOfBounds,
            "opening does not cover the requested byte range");
    return result;
}

namespace {

void serialize_matrix_opening(std::vector<std::uint8_t>& output,
                              const MatrixOpening& opening)
{
    require(!opening.row_indices.empty(), ErrorCode::InvalidLength, "matrix opening has no rows");
    require(std::is_sorted(opening.row_indices.begin(), opening.row_indices.end()),
            ErrorCode::NonCanonical,
            "matrix opening rows are not sorted");
    require(std::adjacent_find(opening.row_indices.begin(), opening.row_indices.end())
                == opening.row_indices.end(),
            ErrorCode::NonCanonical,
            "matrix opening rows are not unique");
    append_u32(output, checked_u32(opening.row_indices.size(), "opening row count"));
    for (const std::size_t row : opening.row_indices) {
        append_u32(output, checked_u32(row, "opening row index"));
    }
    serialize_merkle_proof(output, opening.proof);
}

[[nodiscard]] MatrixOpening deserialize_matrix_opening(Reader& reader)
{
    const std::size_t row_count = reader.u32();
    require(row_count != 0U && row_count <= 256U,
            ErrorCode::InvalidLength,
            "matrix opening row count is invalid");
    MatrixOpening opening;
    opening.row_indices.reserve(row_count);
    for (std::size_t index = 0U; index < row_count; ++index) {
        opening.row_indices.push_back(reader.u32());
    }
    opening.proof = deserialize_merkle_proof(reader);
    return opening;
}

[[nodiscard]] std::vector<std::size_t> expected_opening_leaves(
    std::span<const std::size_t> rows,
    std::size_t row_width)
{
    require(row_width != 0U, ErrorCode::InvalidShape, "opening row width cannot be zero");
    std::set<std::size_t> leaves;
    for (const std::size_t row : rows) {
        const std::size_t start = checked_product(row, row_width, "opening row start");
        const std::size_t end = checked_product(row + 1U, row_width, "opening row end");
        const std::size_t first = start / kMerkleChunkBytes;
        const std::size_t last = (end - 1U) / kMerkleChunkBytes;
        for (std::size_t leaf = first; leaf <= last; ++leaf) {
            leaves.insert(leaf);
        }
    }
    return {leaves.begin(), leaves.end()};
}

void validate_plain_proof(const PlainProof& proof)
{
    require(proof.version == 1U, ErrorCode::InvalidValue, "unsupported P1 PlainProof version");
    require(proof.k == proof.config.common_dim && proof.rank == proof.config.rank,
            ErrorCode::InvalidValue,
            "PlainProof dimensions do not agree with the mining configuration");
    validate_configuration(proof.config,
                           proof.m,
                           proof.n,
                           proof.t_rows,
                           proof.t_cols,
                           ValidationProfile::PinnedCurrent);
    require(proof.header_config_key == job_key(proof.header, proof.config),
            ErrorCode::InvalidValue,
            "PlainProof header/config key is not canonical");
    const std::vector<std::uint32_t> expected_a = proof.config.rows_pattern.indices_with_offset(proof.t_rows);
    const std::vector<std::uint32_t> expected_b = proof.config.cols_pattern.indices_with_offset(proof.t_cols);
    require(proof.a_opening.row_indices.size() == expected_a.size()
                && proof.bt_opening.row_indices.size() == expected_b.size(),
            ErrorCode::InvalidShape,
            "PlainProof opening row count does not match the committed pattern");
    for (std::size_t index = 0U; index < expected_a.size(); ++index) {
        require(proof.a_opening.row_indices[index] == expected_a[index],
                ErrorCode::InvalidValue,
                "PlainProof A opening rows do not match the pattern");
    }
    for (std::size_t index = 0U; index < expected_b.size(); ++index) {
        require(proof.bt_opening.row_indices[index] == expected_b[index],
                ErrorCode::InvalidValue,
                "PlainProof B^T opening rows do not match the pattern");
    }
    const std::size_t a_bytes = checked_product(proof.m, proof.k, "PlainProof A size");
    const std::size_t b_bytes = checked_product(proof.n, proof.k, "PlainProof B^T size");
    const std::size_t expected_a_leaves = (a_bytes + kMerkleChunkBytes - 1U) / kMerkleChunkBytes;
    const std::size_t expected_b_leaves = (b_bytes + kMerkleChunkBytes - 1U) / kMerkleChunkBytes;
    require(proof.a_opening.proof.total_leaves == expected_a_leaves
                && proof.bt_opening.proof.total_leaves == expected_b_leaves,
            ErrorCode::InvalidShape,
            "PlainProof Merkle leaf count does not match matrix dimensions");
    require(proof.a_opening.proof.leaf_indices
                == expected_opening_leaves(proof.a_opening.row_indices, proof.k)
                && proof.bt_opening.proof.leaf_indices
                    == expected_opening_leaves(proof.bt_opening.row_indices, proof.k),
            ErrorCode::InvalidValue,
            "PlainProof Merkle leaves do not cover exactly the selected rows");
    require(proof.a_opening.proof.root == proof.hash_a
                && proof.bt_opening.proof.root == proof.hash_b,
            ErrorCode::InvalidValue,
            "PlainProof commitment roots do not match opening roots");
    require(verify_merkle_proof(proof.a_opening.proof, proof.header_config_key)
                && verify_merkle_proof(proof.bt_opening.proof, proof.header_config_key),
            ErrorCode::InvalidValue,
            "PlainProof Merkle opening verification failed");
    std::vector<std::uint8_t> hash_input;
    hash_input.reserve(64U);
    append_digest(hash_input, proof.header_config_key);
    append_digest(hash_input, proof.hash_b);
    require(proof.commitment_b == hash_unkeyed(hash_input),
            ErrorCode::InvalidValue,
            "PlainProof B commitment derivation is not canonical");
    hash_input.clear();
    append_digest(hash_input, proof.commitment_b);
    append_digest(hash_input, proof.hash_a);
    require(proof.commitment_a == hash_unkeyed(hash_input),
            ErrorCode::InvalidValue,
            "PlainProof A commitment derivation is not canonical");
    const std::size_t expected_trace = proof.config.dot_product_length() / proof.rank;
    require(proof.transcript.trace.size() == expected_trace,
            ErrorCode::InvalidShape,
            "PlainProof transcript trace length is not canonical");
    std::array<std::uint32_t, kTranscriptWords> state{};
    for (std::size_t index = 0U; index < proof.transcript.trace.size(); ++index) {
        const TranscriptStep& step = proof.transcript.trace[index];
        require(step.reduction_index == index,
                ErrorCode::NonCanonical,
                "PlainProof transcript reduction index is not sequential");
        const std::size_t slot = index % kTranscriptWords;
        state[slot] = rotl32(state[slot], kTranscriptRotation) ^ step.combined_xor;
        require(step.state == state,
                ErrorCode::InvalidValue,
                "PlainProof transcript trace state is inconsistent");
    }
    require(proof.transcript.words == state,
            ErrorCode::InvalidValue,
            "PlainProof final transcript is inconsistent with its trace");
    require(proof.jackpot == jackpot_hash(proof.transcript.words, proof.commitment_a),
            ErrorCode::InvalidValue,
            "PlainProof jackpot is not derived from the transcript and commitment");
}

} // namespace

std::vector<std::uint8_t> PlainProof::serialize() const
{
    validate_plain_proof(*this);
    std::vector<std::uint8_t> output;
    output.reserve(4096U);
    append_u32(output, version);
    const auto header_bytes = serialize_header(header);
    const auto config_bytes = config.to_bytes();
    output.insert(output.end(), header_bytes.begin(), header_bytes.end());
    output.insert(output.end(), config_bytes.begin(), config_bytes.end());
    append_digest(output, header_config_key);
    append_digest(output, hash_a);
    append_digest(output, hash_b);
    append_digest(output, commitment_b);
    append_digest(output, commitment_a);
    append_digest(output, jackpot);
    append_digest(output, target);
    append_u32(output, m);
    append_u32(output, n);
    append_u32(output, k);
    append_u32(output, rank);
    append_u32(output, t_rows);
    append_u32(output, t_cols);
    serialize_matrix_opening(output, a_opening);
    serialize_matrix_opening(output, bt_opening);
    append_u32(output, checked_u32(transcript.trace.size(), "transcript trace count"));
    for (const TranscriptStep& step : transcript.trace) {
        append_u32(output, step.reduction_index);
        append_u32(output, step.combined_xor);
        for (const std::uint32_t word : step.state) {
            append_u32(output, word);
        }
    }
    for (const std::uint32_t word : transcript.words) {
        append_u32(output, word);
    }
    return output;
}

PlainProof PlainProof::deserialize(std::span<const std::uint8_t> bytes)
{
    Reader reader(bytes);
    PlainProof result;
    result.version = reader.u32();
    result.header = deserialize_header(reader.fixed_bytes<kHeaderBytes>());
    result.config = MiningConfiguration::from_bytes(reader.fixed_bytes<kMiningConfigBytes>());
    result.header_config_key = reader.fixed_bytes<kDigestBytes>();
    result.hash_a = reader.fixed_bytes<kDigestBytes>();
    result.hash_b = reader.fixed_bytes<kDigestBytes>();
    result.commitment_b = reader.fixed_bytes<kDigestBytes>();
    result.commitment_a = reader.fixed_bytes<kDigestBytes>();
    result.jackpot = reader.fixed_bytes<kDigestBytes>();
    result.target = reader.fixed_bytes<kDigestBytes>();
    result.m = reader.u32();
    result.n = reader.u32();
    result.k = reader.u32();
    result.rank = reader.u32();
    result.t_rows = reader.u32();
    result.t_cols = reader.u32();
    result.a_opening = deserialize_matrix_opening(reader);
    result.bt_opening = deserialize_matrix_opening(reader);
    const std::size_t trace_count = reader.u32();
    require(trace_count != 0U && trace_count <= 4096U,
            ErrorCode::InvalidLength,
            "PlainProof transcript trace count is invalid");
    result.transcript.trace.reserve(trace_count);
    for (std::size_t index = 0U; index < trace_count; ++index) {
        TranscriptStep step;
        step.reduction_index = reader.u32();
        step.combined_xor = reader.u32();
        for (std::uint32_t& word : step.state) {
            word = reader.u32();
        }
        result.transcript.trace.push_back(step);
    }
    for (std::uint32_t& word : result.transcript.words) {
        word = reader.u32();
    }
    require_no_remaining(reader, "PlainProof");
    validate_plain_proof(result);
    const std::vector<std::uint8_t> canonical = result.serialize();
    require(canonical.size() == bytes.size()
                && std::equal(canonical.begin(), canonical.end(), bytes.begin(), bytes.end()),
            ErrorCode::NonCanonical,
            "PlainProof does not round-trip canonically");
    return result;
}

std::vector<std::uint8_t> serialize_public_data(const MiningConfiguration& config,
                                                const Digest& hash_a,
                                                const Digest& hash_b,
                                                const Digest& jackpot,
                                                std::uint32_t m,
                                                std::uint32_t n,
                                                std::uint32_t t_rows,
                                                std::uint32_t t_cols)
{
    std::vector<std::uint8_t> output;
    output.reserve(164U);
    const auto config_bytes = config.to_bytes();
    output.insert(output.end(), config_bytes.begin(), config_bytes.end());
    append_digest(output, hash_a);
    append_digest(output, hash_b);
    append_digest(output, jackpot);
    append_u32(output, m);
    append_u32(output, n);
    append_u32(output, t_rows);
    append_u32(output, t_cols);
    require(output.size() == 164U, ErrorCode::InvalidLength, "public data has unexpected length");
    return output;
}

PublicData deserialize_public_data(std::span<const std::uint8_t> bytes)
{
    require(bytes.size() == 164U, ErrorCode::InvalidLength, "dense public data must be 164 bytes");
    Reader reader(bytes);
    PublicData result;
    result.config = MiningConfiguration::from_bytes(reader.bytes(kMiningConfigBytes));
    result.hash_a = reader.fixed_bytes<kDigestBytes>();
    result.hash_b = reader.fixed_bytes<kDigestBytes>();
    result.jackpot = reader.fixed_bytes<kDigestBytes>();
    result.m = reader.u32();
    result.n = reader.u32();
    result.t_rows = reader.u32();
    result.t_cols = reader.u32();
    require_no_remaining(reader, "dense public data");
    validate_configuration(result.config, result.m, result.n, result.t_rows, result.t_cols);
    const auto canonical = serialize_public_data(result.config,
                                                 result.hash_a,
                                                 result.hash_b,
                                                 result.jackpot,
                                                 result.m,
                                                 result.n,
                                                 result.t_rows,
                                                 result.t_cols);
    require(std::equal(canonical.begin(), canonical.end(), bytes.begin(), bytes.end()),
            ErrorCode::NonCanonical,
            "dense public data does not round-trip canonically");
    return result;
}

} // namespace xdna::pearl
