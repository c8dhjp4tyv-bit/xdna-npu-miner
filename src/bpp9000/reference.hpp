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

[[nodiscard]] CandidateResult score_candidate(const Task& task,
                                              const PublicKey& public_key,
                                              const MiningSeed& mining_seed,
                                              const Nonce& nonce,
                                              const CandidateRandomSource& random_source,
                                              const ReferenceConfig& config = {});

} // namespace xdna::bpp9000
