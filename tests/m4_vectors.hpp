#pragma once

#include "bpp9000/reference.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace m4_test {

struct M4Case {
    std::string id;
    std::uint64_t seed = 0U;
    std::uint64_t case_index = 0U;
    xdna::bpp9000::Task task;
    xdna::bpp9000::Lut lut;
    xdna::bpp9000::ReferenceConfig config;

    M4Case(std::string case_id,
           std::uint64_t case_seed,
           std::uint64_t index,
           xdna::bpp9000::Task case_task,
           xdna::bpp9000::Lut case_lut,
           xdna::bpp9000::ReferenceConfig case_config)
        : id(std::move(case_id)),
          seed(case_seed),
          case_index(index),
          task(std::move(case_task)),
          lut(std::move(case_lut)),
          config(case_config)
    {
    }
};

[[nodiscard]] M4Case make_case(std::uint64_t seed,
                               std::uint64_t case_index,
                               std::uint64_t sequence_length = 8U,
                               std::uint32_t window_width = 2U,
                               bool timeout = false,
                               unsigned int output_mode = 0U);

[[nodiscard]] std::vector<M4Case> make_fixed_cases(std::size_t count);
[[nodiscard]] std::vector<M4Case> make_random_cases(std::uint64_t seed, std::size_t count);

} // namespace m4_test
