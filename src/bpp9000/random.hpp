#pragma once

#include "bpp9000/types.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace xdna::bpp9000 {

enum class RandomDrawKind : Byte {
    RootLut = 0,
    MutationWords = 1,
};

struct RandomDrawEvent {
    RandomDrawKind kind = RandomDrawKind::RootLut;
    std::size_t byte_count = 0U;
};

// This is the production seam. A later integration may implement these two
// draws with K12 plus random2; the reference never owns a global RNG.
class CandidateRandomSource {
public:
    virtual ~CandidateRandomSource() = default;
    virtual void fill_root_lut(const MiningSeed& mining_seed,
                               const PublicKey& public_key,
                               std::span<Byte> destination) const = 0;
    virtual void fill_mutation_words(const MiningSeed& mining_seed,
                                     const PublicKey& public_key,
                                     const Nonce& nonce,
                                     std::span<std::uint64_t> destination) const = 0;
};

// Deterministic fixture source. It is not a cryptographic implementation and
// must not be used as a production random2/K12 replacement.
class DeterministicFixtureRandom final : public CandidateRandomSource {
public:
    explicit DeterministicFixtureRandom(std::uint64_t seed)
        : seed_(seed)
    {
    }

    void fill_root_lut(const MiningSeed& mining_seed,
                       const PublicKey& public_key,
                       std::span<Byte> destination) const override;
    void fill_mutation_words(const MiningSeed& mining_seed,
                             const PublicKey& public_key,
                             const Nonce& nonce,
                             std::span<std::uint64_t> destination) const override;

    [[nodiscard]] const std::vector<RandomDrawEvent>& events() const noexcept
    {
        return events_;
    }

    void clear_events() const
    {
        events_.clear();
    }

private:
    std::uint64_t seed_;
    mutable std::vector<RandomDrawEvent> events_;
};

class ScriptedRandom final : public CandidateRandomSource {
public:
    ScriptedRandom(std::vector<Byte> root_bytes, std::vector<std::uint64_t> mutation_words)
        : root_bytes_(std::move(root_bytes)), mutation_words_(std::move(mutation_words))
    {
    }

    void fill_root_lut(const MiningSeed&, const PublicKey&, std::span<Byte> destination) const override;
    void fill_mutation_words(const MiningSeed&,
                             const PublicKey&,
                             const Nonce&,
                             std::span<std::uint64_t> destination) const override;

    [[nodiscard]] const std::vector<RandomDrawEvent>& events() const noexcept
    {
        return events_;
    }

private:
    std::vector<Byte> root_bytes_;
    std::vector<std::uint64_t> mutation_words_;
    mutable std::vector<RandomDrawEvent> events_;
};

} // namespace xdna::bpp9000
