#pragma once

#include "bpp9000/reference.hpp"
#include "xdna/k1.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace k1_test {

struct K1Vector {
    std::string id;
    std::uint64_t generator_seed = 0U;
    std::uint64_t case_index = 0U;
    std::array<xdna::bpp9000::Byte, xdna::runtime::kK1StateLogicalBytes> state{};
    std::array<xdna::bpp9000::Byte, xdna::runtime::kK1LutLogicalBytes> lut{};
    std::array<std::uint32_t, xdna::runtime::kK1NeighborsLogicalCount> neighbors{};
    std::array<std::uint32_t, xdna::runtime::kK1UpdatedLogicalCount> updated_neurons{};
};

[[nodiscard]] K1Vector make_random_vector(std::uint64_t seed,
                                          std::uint64_t case_index,
                                          std::string id);
[[nodiscard]] std::vector<K1Vector> make_edge_vectors();
[[nodiscard]] std::vector<K1Vector> make_deterministic_vectors(std::uint64_t seed,
                                                               std::size_t count);

[[nodiscard]] xdna::bpp9000::Task make_task(const K1Vector& vector);
[[nodiscard]] xdna::runtime::K1LogicalInput logical_input(const K1Vector& vector);
[[nodiscard]] std::vector<xdna::bpp9000::Byte> cpu_expected(const K1Vector& vector);

} // namespace k1_test
