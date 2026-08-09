#pragma once

#include "xdna/m4.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace xdna::runtime {

// M5 batches complete, independent M4 window operations. The per-item byte
// layout intentionally remains the audited M4 layout so every item has an
// explicit reset state, candidate LUT, topology, input sequence, and target
// sequence. The batch layer adds only deterministic item/result strides.
constexpr std::size_t kM5DefaultBatchSize = 1U;
constexpr std::size_t kM5MaximumBatchSize = 16U;
// M4's logical packed fields occupy 15457 bytes; M5 rounds each item to the
// audited 15488-byte device arena used by the generated AIE artifact. The
// final 31 bytes are explicit padding and are never read by the kernel.
constexpr std::size_t kM5InputItemStrideBytes = 15488U;
constexpr std::size_t kM5OutputItemStrideBytes = kM4OutputBufferBytes;
constexpr std::size_t kM5OutputErrorOffset = 124U;
constexpr std::uint32_t kM5OutputMagic = 0x3152354DU; // little-endian "M5R1"

enum class M5ItemError : std::uint32_t {
    None = 0U,
    InvalidMode = 1U,
    InvalidInput = 2U,
};

struct M5DeviceLayout {
    std::size_t batch_size = kM5DefaultBatchSize;
    std::size_t input_item_stride_bytes = kM5InputItemStrideBytes;
    std::size_t output_item_stride_bytes = kM5OutputItemStrideBytes;
    std::size_t input_buffer_bytes = kM5InputItemStrideBytes;
    std::size_t output_buffer_bytes = kM5OutputItemStrideBytes;
};

[[nodiscard]] constexpr M5DeviceLayout m5_default_layout(
    std::size_t batch_size = kM5DefaultBatchSize) noexcept
{
    return M5DeviceLayout{
        batch_size,
        kM5InputItemStrideBytes,
        kM5OutputItemStrideBytes,
        batch_size * kM5InputItemStrideBytes,
        batch_size * kM5OutputItemStrideBytes,
    };
}

struct M5WorkItem {
    std::uint32_t candidate_index = 0U;
    std::uint64_t window_index = 0U;
    M4LogicalInput input;
};

struct M5ItemDescriptor {
    std::size_t item_index = 0U;
    std::uint32_t candidate_index = 0U;
    std::uint64_t window_index = 0U;
    std::size_t input_offset = 0U;
    std::size_t output_offset = 0U;
    std::size_t state_offset = 0U;
    std::size_t lut_offset = 0U;
    std::size_t topology_offset = 0U;
    std::size_t input_sequence_offset = 0U;
    std::size_t target_offset = 0U;
    std::size_t score_offset = 0U;
    std::size_t status_offset = 0U;
    std::size_t error_offset = 0U;
};

struct M5BatchSchema {
    std::size_t batch_size = 0U;
    std::size_t input_item_stride_bytes = 0U;
    std::size_t output_item_stride_bytes = 0U;
    std::vector<M5ItemDescriptor> items;
};

struct M5PackedBatch {
    M5BatchSchema schema;
    std::vector<bpp9000::Byte> input;
    std::vector<bpp9000::Byte> output;
};

struct M5ItemResult {
    M5ItemDescriptor descriptor;
    M4DeviceResult device;
    M5ItemError error = M5ItemError::None;
};

class M5ContractError final : public std::runtime_error {
public:
    explicit M5ContractError(const char* message)
        : std::runtime_error(message)
    {
    }

    explicit M5ContractError(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

void validate_m5_device_layout(const M5DeviceLayout& layout);
void validate_m5_packed_input(const M5PackedBatch& batch,
                              const M5DeviceLayout& layout = {});
void validate_m5_output(std::span<const bpp9000::Byte> output);

[[nodiscard]] M5PackedBatch pack_m5(std::span<const M5WorkItem> items,
                                    const M5DeviceLayout& layout = {});
[[nodiscard]] std::vector<M5ItemResult> unpack_m5_results(const M5PackedBatch& batch,
                                                           const M5DeviceLayout& layout = {});

} // namespace xdna::runtime
