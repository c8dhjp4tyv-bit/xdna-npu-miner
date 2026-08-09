#include "bpp9000/random.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace xdna::bpp9000 {
namespace {

[[nodiscard]] std::uint64_t mix_key(std::uint64_t state, std::span<const Byte> bytes)
{
    for (std::size_t index = 0U; index < bytes.size(); ++index) {
        state ^= static_cast<std::uint64_t>(bytes[index]) + (static_cast<std::uint64_t>(index) << 8U);
        state ^= state >> 30U;
        state *= 0xBF58476D1CE4E5B9ULL;
        state ^= state >> 27U;
        state *= 0x94D049BB133111EBULL;
        state ^= state >> 31U;
    }
    return state;
}

[[nodiscard]] std::uint64_t next_word(std::uint64_t& state)
{
    state += 0x9E3779B97F4A7C15ULL;
    std::uint64_t value = state;
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
}

void record_event(std::vector<RandomDrawEvent>& events, RandomDrawKind kind, std::size_t bytes)
{
    events.push_back(RandomDrawEvent{kind, bytes});
}

} // namespace

void DeterministicFixtureRandom::fill_root_lut(const MiningSeed& mining_seed,
                                               const PublicKey& public_key,
                                               std::span<Byte> destination) const
{
    record_event(events_, RandomDrawKind::RootLut, destination.size());
    std::uint64_t state = mix_key(seed_ ^ 0x524F4F545F4C5554ULL, mining_seed.bytes);
    state = mix_key(state, public_key.bytes);
    for (Byte& value : destination) {
        value = static_cast<Byte>(next_word(state));
    }
}

void DeterministicFixtureRandom::fill_mutation_words(const MiningSeed& mining_seed,
                                                     const PublicKey& public_key,
                                                     const Nonce& nonce,
                                                     std::span<std::uint64_t> destination) const
{
    if (destination.size() > std::numeric_limits<std::size_t>::max() / sizeof(std::uint64_t)) {
        throw std::length_error("mutation draw is too large");
    }
    record_event(events_, RandomDrawKind::MutationWords, destination.size() * sizeof(std::uint64_t));
    std::uint64_t state = mix_key(seed_ ^ 0x4D55544154494F4EULL, mining_seed.bytes);
    state = mix_key(state, public_key.bytes);
    // The first three nonce bytes are algorithm/search knobs and are excluded
    // from candidate-specific random material at this boundary.
    state = mix_key(state, std::span<const Byte>(nonce.bytes).subspan(3U));
    for (std::uint64_t& value : destination) {
        value = next_word(state);
    }
}

void ScriptedRandom::fill_root_lut(const MiningSeed&, const PublicKey&, std::span<Byte> destination) const
{
    record_event(events_, RandomDrawKind::RootLut, destination.size());
    if (destination.size() > root_bytes_.size()) {
        throw std::out_of_range("scripted root draw is shorter than requested");
    }
    std::copy(root_bytes_.begin(), root_bytes_.begin() + static_cast<std::ptrdiff_t>(destination.size()), destination.begin());
}

void ScriptedRandom::fill_mutation_words(const MiningSeed&,
                                         const PublicKey&,
                                         const Nonce&,
                                         std::span<std::uint64_t> destination) const
{
    if (destination.size() > std::numeric_limits<std::size_t>::max() / sizeof(std::uint64_t)) {
        throw std::length_error("mutation draw is too large");
    }
    record_event(events_, RandomDrawKind::MutationWords, destination.size() * sizeof(std::uint64_t));
    if (destination.size() > mutation_words_.size()) {
        throw std::out_of_range("scripted mutation draw is shorter than requested");
    }
    std::copy(mutation_words_.begin(),
              mutation_words_.begin() + static_cast<std::ptrdiff_t>(destination.size()),
              destination.begin());
}

} // namespace xdna::bpp9000
