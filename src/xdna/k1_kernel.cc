#include <stdint.h>

extern "C" void bpp9000_k1_tick(const uint8_t* device_input, uint8_t* next_state)
{
    // One input arena avoids exhausting the small one-column shim DMA
    // channel budget. Every sub-buffer starts on a 32-byte boundary.
    const uint8_t* previous_state = device_input;
    const uint8_t* lut = device_input + 96U;
    const uint32_t* neighbors = reinterpret_cast<const uint32_t*>(device_input + 1568U);
    const uint32_t* updated_neurons = reinterpret_cast<const uint32_t*>(device_input + 2336U);

    // The input state is immutable for this invocation. Copying it first
    // preserves all external input roles and makes the simultaneous commit
    // rule explicit in the device implementation.
    for (uint32_t neuron = 0U; neuron < 64U; ++neuron) {
        next_state[neuron] = previous_state[neuron];
    }

    for (uint32_t row = 0U; row < 46U; ++row) {
        const uint32_t neuron = updated_neurons[row];
        const uint32_t neighbor_base = neuron * 3U;
        const uint32_t first = previous_state[neighbors[neighbor_base]];
        const uint32_t second = previous_state[neighbors[neighbor_base + 1U]];
        const uint32_t third = previous_state[neighbors[neighbor_base + 2U]];
        const uint32_t index = first + 3U * second + 9U * third;
        next_state[neuron] = lut[row * 32U + index];
    }
}
