#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace soshi {

inline constexpr std::uint32_t kPitchUnit = 1u << 16;  // 1.0 as 16.16 fixed point

// One device stream, a fixed pool of voices, mixed in float32 inside the device
// callback.
//
// The game thread never touches mixer state: play/stop/volume/pan/pitch are
// published as commands through a single-producer / single-consumer queue and
// applied by the mixer at the start of the next block. No lock is taken on the
// game thread, so a crowded frame never waits for the audio callback.
//
// Sounds are decoded once to the *device* sample rate, so nothing is resampled
// while mixing, and the mixed block is handed to SDL in the device's own format
// when it is already float32 stereo (no conversion pass at all).
class Audio {
public:
    static constexpr int kChannels = 2;
    static constexpr int kSoundCapacity = 64;
    static constexpr int kVoiceCapacity = 64;

    void shutdown();

    std::uint64_t load(const char* path);
    void destroy(std::uint64_t handle);
    std::uint32_t sound_frames(std::uint64_t handle);
    float sound_seconds(std::uint64_t handle);

    bool available();
    int sample_rate();
    int channels() const { return kChannels; }

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
    float master_volume() const;
    void pause(bool paused);
    bool paused() const;

private:
    struct Sound {
        std::int16_t* samples = nullptr;
        std::uint32_t frames = 0;
        std::uint32_t generation = 1;
    };

    // Mixer-owned. Only the audio callback reads or writes these fields.
    struct Voice {
        const std::int16_t* samples = nullptr;
        std::uint32_t frames = 0;
        std::uint32_t position = 0;  // frame position, 16.16 fixed point
        std::uint32_t step = kPitchUnit;
        std::uint32_t generation = 0;
        std::int32_t loops_left = 0;  // extra repeats, -1 plays forever
        float volume = 1.0f;
        float pan = 0.0f;
        float gain_left = 0.0f;
        float gain_right = 0.0f;
        float target_left = 0.0f;
        float target_right = 0.0f;
        float ramp_left = 0.0f;  // gain change per frame, 0 when settled
        float ramp_right = 0.0f;
        bool active = false;
        bool dying = false;
    };

    // Game-thread view of a voice slot, so queries never race the mixer.
    struct VoiceState {
        std::atomic<std::uint32_t> generation{0};
        std::atomic<bool> claimed{false};
        std::atomic<bool> active{false};
        std::atomic<float> progress{0.0f};
    };

    enum class CommandKind : std::uint8_t { Play, Stop, StopAll, Volume, Pan, Pitch, Free };

    struct Command {
        CommandKind kind = CommandKind::StopAll;
        std::uint32_t index = 0;
        std::uint32_t generation = 0;
        const std::int16_t* samples = nullptr;
        std::uint32_t frames = 0;
        std::uint32_t step = kPitchUnit;
        float volume = 1.0f;
        float pan = 0.0f;
        float fade = 0.0f;
        std::int32_t loops = 0;
    };

    static constexpr std::uint32_t kCommandCapacity = 1024;
    static constexpr int kMixFrames = 512;

    static void SDLCALL callback(void* userdata, SDL_AudioStream* stream, int additional, int total);

    bool ensure_device();
    bool push(const Command& command);
    bool pop(Command& command);
    void apply(const Command& command);
    void mix(int frames);
    template <bool kRamp>
    void mix_voice(Voice& voice, int frames);
    void deliver(int frames);
    void publish();

    void set_ramp(Voice& voice, float left, float right, float seconds);
    void release(int index);
    int claim_voice(std::uint32_t generation);
    std::uint32_t next_generation();
    bool live_voice(std::uint64_t handle, int& index) const;
    Sound* find_sound(std::uint64_t handle);

    SDL_AudioStream* stream_ = nullptr;
    SDL_AudioSpec native_{};  // the stream's input format, i.e. what we hand SDL
    SDL_AudioSpec mix_spec_{SDL_AUDIO_F32, kChannels, 48000};
    int rate_ = 0;

    std::array<Sound, kSoundCapacity> sounds_{};
    std::array<Voice, kVoiceCapacity> voices_{};
    std::array<VoiceState, kVoiceCapacity> state_{};
    std::array<std::uint32_t, kVoiceCapacity> serials_{};
    std::array<Command, kCommandCapacity> commands_{};
    std::array<float, kMixFrames * kChannels> mix_{};
    std::array<std::int16_t, kMixFrames * kChannels> pcm_{};

    std::atomic<std::uint32_t> command_write_{0};
    std::atomic<std::uint32_t> command_read_{0};
    std::atomic<float> master_target_{1.0f};
    std::atomic<bool> paused_{false};

    float master_applied_ = 1.0f;  // mixer-owned, smoothed toward master_target_
    std::uint32_t generation_counter_ = 0;
    std::uint32_t serial_ = 0;
};

}  // namespace soshi
