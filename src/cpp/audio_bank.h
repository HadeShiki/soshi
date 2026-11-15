#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>

namespace soshi {

// One output stream, sounds converted to its format at load time so playback is a
// single buffer append.
class AudioBank {
    struct Sound {
        Uint8* samples = nullptr;
        Uint32 length = 0;
        std::uint32_t generation = 1;
    };

public:
    static constexpr int kCapacity = 64;

    void shutdown();

    std::uint64_t load(const char* path);
    void destroy(std::uint64_t handle);
    bool play(std::uint64_t handle);
    void set_volume(float volume);
    bool available();

private:
    bool ensure_device();
    Sound* find(std::uint64_t handle);

    SDL_AudioStream* output_ = nullptr;
    SDL_AudioSpec output_spec_{};
    std::array<Sound, kCapacity> sounds_{};
    float volume_ = 1.0f;
};

}  // namespace soshi
