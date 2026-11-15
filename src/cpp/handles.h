#pragma once

#include <cstdint>

namespace soshi {

// Handles pack a slot index (low 32 bits) with a generation counter (high 32 bits), so a
// stale handle can never reach a recycled slot. Slot 0 is reserved as "none".
constexpr std::uint32_t kFirstHandleIndex = 1;

constexpr std::uint64_t handle_of(std::uint32_t index, std::uint32_t generation) {
    return (static_cast<std::uint64_t>(generation) << 32) | index;
}

constexpr std::uint32_t handle_index(std::uint64_t handle) {
    return static_cast<std::uint32_t>(handle & 0xffffffffu);
}

constexpr std::uint32_t handle_generation(std::uint64_t handle) {
    return static_cast<std::uint32_t>(handle >> 32);
}

}  // namespace soshi
