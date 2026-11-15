#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>

namespace soshi {

inline constexpr std::uint32_t kPitchUnit = 1u << 16;  // 1.0 as 16.16 fixed point

// WAV data converted once at load, plus a fixed pool of voices mixed in the audio
// callback. No thread of our own, and nothing is allocated while mixing.
class Audio {
    struct Sound {
        std::int16_t* samples = nullptr;
        std::uint32_t frames = 0;
        std::uint32_t generation = 1;
    };

    struct Voice {
        const std::int16_t* samples = nullptr;
        std::uint32_t frames = 0;
        std::uint32_t position = 0;  // frame position, 16.16 fixed point
        std::uint32_t step = kPitchUnit;
        std::uint32_t serial = 0;     // for stealing the oldest voice
        std::uint32_t generation = 1;
        std::int32_t pan = 0;         // -128 hard left, 128 hard right
        std::int32_t loops_left = 0;  // extra repeats, -1 plays forever
        float gain = 1.0f;
        float target = 1.0f;
        float ramp = 0.0f;           // gain change per frame, 0 when settled
        bool active = false;
    };

public:
    static constexpr int kSoundCapacity = 64;
    static constexpr int kVoiceCapacity = 32;

    void shutdown();

    std::uint64_t load(const char* path);
    void destroy(std::uint64_t handle);
    std::uint32_t sound_frames(std::uint64_t handle);
    int sample_rate() const;
    bool available();

    std::uint64_t play(std::uint64_t sound, float volume, float pan, float pitch, int loops, float fade_in);
    bool stop(std::uint64_t voice, float fade_seconds);
    bool set_volume(std::uint64_t voice, float volume, float fade_seconds);
    bool set_pan(std::uint64_t voice, float pan);
    bool set_pitch(std::uint64_t voice, float pitch);
    bool playing(std::uint64_t voice);
    float progress(std::uint64_t voice);
    void stop_all(float fade_seconds);
    int active_voices();

    void set_master_volume(float volume);
    void pause(bool paused);

private:
    static void SDLCALL callback(void* userdata, SDL_AudioStream* stream, int additional, int total);

    bool ensure_device();
    Sound* find_sound(std::uint64_t handle);
    Voice* find_voice(std::uint64_t handle);
    Voice* claim_voice();
    std::uint64_t voice_handle(const Voice* voice) const;
    void mix_frames(int frames);
    void lock();
    void unlock();

    SDL_AudioStream* stream_ = nullptr;
    SDL_AudioSpec spec_{};
    std::array<Sound, kSoundCapacity> sounds_{};
    std::array<Voice, kVoiceCapacity> voices_{};
    std::array<std::int32_t, 2048 * 2> accumulator_{};
    std::array<std::int16_t, 2048 * 2> output_{};
    std::uint32_t serial_ = 0;
    float master_ = 1.0f;
};

}  // namespace soshi
