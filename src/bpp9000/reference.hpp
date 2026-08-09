#pragma once

#include "bpp9000/random.hpp"
#include "bpp9000/task.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace xdna::bpp9000 {

[[nodiscard]] std::uint32_t lut_index(Trit first, Trit second, Trit third);

class Lut {
public:
    explicit Lut(const Task& task);

    [[nodiscard]] std::size_t rows() const noexcept
    {
        return updated_neurons_.size();
    }

    [[nodiscard]] std::size_t population() const noexcept
    {
        return row_for_neuron_.size();
    }

    [[nodiscard]] const std::vector<std::uint32_t>& updated_neurons() const noexcept
    {
        return updated_neurons_;
    }

    [[nodiscard]] Trit at_row(std::size_t row, std::size_t entry) const;
    [[nodiscard]] Trit at_neuron(std::size_t neuron, std::size_t entry) const;
    void set_row(std::size_t row, std::size_t entry, Trit value);
    void set_neuron(std::size_t neuron, std::size_t entry, Trit value);
    void fill(Trit value);

    [[nodiscard]] const std::vector<Byte>& storage() const noexcept
    {
        return storage_;
    }

    [[nodiscard]] std::vector<Byte>& storage() noexcept
    {
        return storage_;
    }

    void initialize_from_root_bytes(std::span<const Byte> root_bytes);

private:
    std::vector<std::uint32_t> updated_neurons_;
    std::vector<std::int32_t> row_for_neuron_;
    std::vector<Byte> storage_;
};

class RecurrentState {
public:
    explicit RecurrentState(const Task& task);

    void reset_unknown();
    void load_current(std::span<const Byte> state);
    void set_input(std::size_t neuron, Trit value);
    void set_all_inputs_unknown();
    void tick(const Task& task, const Lut& lut);

    [[nodiscard]] const std::vector<Byte>& current() const noexcept
    {
        return current_;
    }

private:
    std::vector<Byte> current_;
    std::vector<Byte> next_;
    std::vector<bool> input_mask_;
};

// Primitive-level CPU oracle used by later device differential tests. This is
// the same double-buffered RecurrentState::tick path used by the M1 scorer;
// it only exposes arbitrary previous-state loading for an isolated tick.
[[nodiscard]] std::vector<Byte> recurrent_tick(const Task& task,
                                               std::span<const Byte> previous_state,
                                               const Lut& lut);

struct WindowResult {
    ScoreResult score{};
    Trit predicted = Trit::Unknown;
    Trit expected = Trit::Unknown;
    std::uint32_t feed_count = 0U;
};

[[nodiscard]] WindowResult score_window(const Task& task,
                                        const Lut& lut,
                                        std::uint64_t window_start,
                                        const ReferenceConfig& config = {});

[[nodiscard]] ScoreResult score_lut(const Task& task,
                                    const Lut& lut,
                                    const ReferenceConfig& config = {});

struct MutationRecord {
    std::size_t row = 0U;
    std::uint32_t neuron = 0U;
    std::size_t entry = 0U;
    Trit old_value = Trit::Unknown;
    Trit new_value = Trit::Unknown;
};

[[nodiscard]] MutationRecord mutate_lut(Lut& lut, std::uint64_t mutation_seed);
void rollback_mutation(Lut& lut, const MutationRecord& record);

struct MutationAttempt {
    std::vector<MutationRecord> mutations;
    ScoreResult measured{};
    bool accepted = false;
};

struct CandidateResult {
    ScoreResult initial{};
    ScoreResult best{};
    Lut best_lut;
    Lut current_lut;
    std::uint32_t score_calls = 0U;
    std::vector<MutationAttempt> attempts;
};

// Materializes the deterministic candidate inputs once so a compute backend
// can score the same root LUT and mutation stream while the CPU retains all
// candidate-control authority. The random provider remains an injected seam;
// this helper does not implement K12/random2.
struct CandidateMaterial {
    std::vector<Byte> root_bytes;
    std::vector<std::uint64_t> mutation_words;
};

[[nodiscard]] CandidateMaterial make_candidate_material(const Task& task,
                                                         const PublicKey& public_key,
                                                         const MiningSeed& mining_seed,
                                                         const Nonce& nonce,
                                                         const CandidateRandomSource& random_source,
                                                         const ReferenceConfig& config = {});

[[nodiscard]] CandidateResult score_candidate(const Task& task,
                                              const PublicKey& public_key,
                                              const MiningSeed& mining_seed,
                                              const Nonce& nonce,
                                              const CandidateRandomSource& random_source,
                                              const ReferenceConfig& config = {});

} // namespace xdna::bpp9000
