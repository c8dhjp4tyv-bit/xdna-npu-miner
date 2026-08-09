#pragma once

#include "bpp9000/types.hpp"

#include <optional>
#include <stdexcept>
#include <string>

namespace xdna::bpp9000 {

enum class TaskErrorCode : Byte {
    Truncated,
    TrailingBytes,
    BadMagic,
    BadVersion,
    BadDimensions,
    BadLength,
    MissingDigestProvider,
    HashMismatch,
    InvalidTopology,
    InvalidPackedTrit,
    InvalidDomainValue,
};

class TaskError final : public std::runtime_error {
public:
    TaskError(TaskErrorCode code, const std::string& message)
        : std::runtime_error(message), code_(code)
    {
    }

    [[nodiscard]] TaskErrorCode code() const noexcept
    {
        return code_;
    }

private:
    TaskErrorCode code_;
};

class BlockDigestProvider {
public:
    virtual ~BlockDigestProvider() = default;
    [[nodiscard]] virtual Digest32 digest(std::span<const Byte> block) const = 0;
};

// This provider is intentionally non-cryptographic and exists only for
// deterministic M1 fixtures. Production parsing must supply a reviewed
// KangarooTwelve implementation at this boundary.
class DeterministicFixtureDigest final : public BlockDigestProvider {
public:
    explicit DeterministicFixtureDigest(std::uint64_t seed = 0xD1CEB00C5EED1234ULL)
        : seed_(seed)
    {
    }

    [[nodiscard]] Digest32 digest(std::span<const Byte> block) const override;

private:
    std::uint64_t seed_;
};

struct TaskParseOptions {
    const BlockDigestProvider* digest_provider = nullptr;
    std::optional<TaskShape> expected_shape;
    bool require_hash_metadata = true;
};

[[nodiscard]] std::size_t packed_bytes_for_trits(std::uint64_t trit_count);
[[nodiscard]] std::size_t topology_bytes_for_shape(const TaskShape& shape);
[[nodiscard]] std::size_t data_bytes_for_shape(const TaskShape& shape);

[[nodiscard]] std::vector<Byte> pack_trits(std::span<const Trit> trits);
[[nodiscard]] std::vector<Trit> unpack_trits(std::span<const Byte> packed, std::size_t trit_count);

[[nodiscard]] std::vector<Byte> serialize_topology(const TaskShape& shape, const Topology& topology);
[[nodiscard]] std::vector<Byte> serialize_data(const TaskShape& shape,
                                               std::span<const Trit> inputs,
                                               std::span<const Trit> outputs);
[[nodiscard]] std::vector<Byte> serialize_task(const Task& task);

[[nodiscard]] Task parse_task(std::span<const Byte> bytes, const TaskParseOptions& options = {});
[[nodiscard]] Task parse_production_task(std::span<const Byte> bytes, const BlockDigestProvider& digest_provider);

} // namespace xdna::bpp9000
