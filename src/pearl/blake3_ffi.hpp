#pragma once

#include <cstddef>
#include <cstdint>

extern "C" {

int pearl_blake3_hash(const std::uint8_t* data,
                      std::size_t data_len,
                      std::uint8_t* output);

int pearl_blake3_hash_keyed(const std::uint8_t* key,
                            const std::uint8_t* data,
                            std::size_t data_len,
                            std::uint8_t* output);

int pearl_blake3_chunk_cv(const std::uint8_t* key,
                          const std::uint8_t* data,
                          std::size_t data_len,
                          std::uint64_t chunk_index,
                          std::uint8_t* output);

int pearl_blake3_parent_cv(const std::uint8_t* key,
                           const std::uint8_t* left,
                           const std::uint8_t* right,
                           bool root,
                           std::uint8_t* output);

}
