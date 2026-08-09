#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace xdna::bpp9000 {

using Byte = std::uint8_t;

constexpr std::uint32_t kTaskMagic = 0x5454554CU;
constexpr std::uint32_t kTaskVersion = 1U;
constexpr std::size_t kTaskHeaderBytes = 96U;
constexpr std::size_t kHashBytes = 32U;
constexpr std::size_t kTritsPerPackedByte = 5U;
constexpr std::uint32_t kPackedByteLimit = 243U;
constexpr std::size_t kLutEntries = 27U;
constexpr std::size_t kLutStride = 32U;
constexpr std::size_t kMaxMutationsPerStep = 10U;
constexpr std::uint32_t kTimeoutScore = 0xFFFFFFFFU;

constexpr std::uint32_t kProductionInputTrits = 18U;
constexpr std::uint32_t kProductionOutputTrits = 1U;
constexpr std::uint64_t kProductionSequenceLength = 8760U;
constexpr std::uint64_t kProductionWindowWidth = 672U;
constexpr std::uint32_t kProductionPopulation = 64U;
constexpr std::uint32_t kProductionNeighbors = 3U;
constexpr std::uint32_t kProductionMutationSteps = 100U;
constexpr std::uint32_t kProductionMaxTicks = 100000U;
constexpr std::uint32_t kProductionThreshold = 3838U;

enum class Trit : Byte {
    Zero = 0,
    One = 1,
    Unknown = 2,
};

[[nodiscard]] constexpr bool is_valid_trit_byte(Byte value) noexcept
{
    return value <= static_cast<Byte>(Trit::Unknown);
}

[[nodiscard]] constexpr Trit trit_from_byte(Byte value)
{
    return static_cast<Trit>(value);
}

[[nodiscard]] constexpr Byte trit_to_byte(Trit value)
{
    return static_cast<Byte>(value);
}

template <std::size_t N>
struct FixedBytes {
    std::array<Byte, N> bytes{};

    [[nodiscard]] constexpr bool is_zero() const noexcept
    {
        for (const Byte value : bytes) {
            if (value != 0U) {
                return false;
            }
        }
        return true;
    }

    friend constexpr bool operator==(const FixedBytes&, const FixedBytes&) = default;
};

using PublicKey = FixedBytes<32U>;
using MiningSeed = FixedBytes<32U>;
using Nonce = FixedBytes<32U>;
using Digest32 = FixedBytes<kHashBytes>;

struct TaskShape {
    std::uint32_t input_trits = 0U;
    std::uint32_t output_trits = 0U;
    std::uint64_t sequence_length = 0U;
    std::uint32_t population = 0U;
    std::uint32_t neighbors = 0U;

    friend constexpr bool operator==(const TaskShape&, const TaskShape&) = default;
};

[[nodiscard]] constexpr TaskShape production_shape() noexcept
{
    return TaskShape{
        kProductionInputTrits,
        kProductionOutputTrits,
        kProductionSequenceLength,
        kProductionPopulation,
        kProductionNeighbors,
    };
}

struct TaskHeader {
    std::uint32_t magic = kTaskMagic;
    std::uint32_t version = kTaskVersion;
    TaskShape shape{};
    Digest32 topology_hash{};
    Digest32 data_hash{};
};

struct Topology {
    std::vector<std::uint32_t> input_neurons;
    std::vector<std::uint32_t> output_neurons;
    std::uint32_t signal_neuron = 0U;
    std::vector<std::uint32_t> neighbors;
};

struct Task {
    TaskHeader header{};
    Topology topology{};
    std::vector<Trit> inputs;
    std::vector<Trit> outputs;

    // These retain the exact validated blocks. Serialization from parsed data
    // regenerates canonical blocks and is independent of host struct padding.
    std::vector<Byte> packed_topology;
    std::vector<Byte> packed_data;

    [[nodiscard]] std::uint64_t number_of_windows(std::uint64_t window_width) const noexcept
    {
        return header.shape.sequence_length - window_width;
    }
};

struct ReferenceConfig {
    std::uint64_t window_width = kProductionWindowWidth;
    std::uint32_t max_ticks = kProductionMaxTicks;
    std::uint32_t mutation_steps = kProductionMutationSteps;
};

enum class ScoreStatus : Byte {
    Settled = 0,
    Timeout = 1,
};

struct ScoreResult {
    std::uint32_t score = kTimeoutScore;
    ScoreStatus status = ScoreStatus::Timeout;
    std::uint32_t windows_evaluated = 0U;
    std::uint32_t ticks = 0U;

    [[nodiscard]] bool timed_out() const noexcept
    {
        return status == ScoreStatus::Timeout;
    }
};

struct Threshold {
    std::uint32_t value = 0U;
};

[[nodiscard]] bool is_valid_score(const ScoreResult& result, std::uint64_t window_count) noexcept;
[[nodiscard]] bool is_good_score(const ScoreResult& result, Threshold threshold, std::uint64_t window_count) noexcept;
[[nodiscard]] bool is_canonical_nonce(const Nonce& nonce) noexcept;
[[nodiscard]] bool is_valid_mining_seed(const MiningSeed& seed) noexcept;

} // namespace xdna::bpp9000
