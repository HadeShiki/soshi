#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>

namespace soshi {

// Textures live on the renderer, so they are owned here rather than in the Zig core;
// entities only carry the slot index.
class TextureBank {
    struct Slot {
        SDL_Texture* texture = nullptr;
        std::uint32_t generation = 1;
    };

public:
    static constexpr int kCapacity = 256;

    std::uint64_t load(SDL_Renderer* renderer, const char* path);
    void destroy(std::uint64_t handle);
    bool size_of(std::uint64_t handle, float& width, float& height);
    void reset();

    // Snapshot path: the world hands out slot indices, not handles.
    SDL_Texture* raw(std::uint32_t index) const;

private:
    Slot* find(std::uint64_t handle);

    std::array<Slot, kCapacity> slots_{};
};

}  // namespace soshi
