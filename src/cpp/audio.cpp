#include "audio.h"

#include "decoder.h"
#include "handles.h"

#include <algorithm>
#include <cmath>

namespace soshi {
namespace {

constexpr float kInt16ToFloat = 1.0f / 32768.0f;
constexpr float kMaxPitch = 8.0f;
constexpr float kMinPitch = 0.05f;

// A volume or pan change without an explicit fade is smoothed over this long, so
// a slider or a pan sweep never clicks.
constexpr float kSmoothingSeconds = 0.005f;

float clamp_unit(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

// Constant-power pan: a centered voice keeps its loudness and a hard-panned one
// keeps its power, so panning sweeps do not dip in the middle.
void pan_gains(float pan, float& left, float& right) {
    const float angle = (std::clamp(pan, -1.0f, 1.0f) + 1.0f) * 0.25f * 3.14159265358979f;
    left = std::cos(angle);
    right = std::sin(angle);
}

}  // namespace

bool Audio::ensure_device() {
    if (stream_ != nullptr) return true;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;

    SDL_AudioSpec device{};
    if (!SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &device, nullptr)) return false;
    if (device.freq <= 0 or device.channels <= 0) return false;

    // NULL spec: the stream takes the device format, so SDL does no conversion at
    // all when we hand it float32 stereo at the device rate.
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr, &Audio::callback, this);
    if (stream_ == nullptr) return false;

    if (!SDL_GetAudioStreamFormat(stream_, &native_, nullptr)) {
        SDL_DestroyAudioStream(stream_);
        stream_ = nullptr;
        return false;
    }

    rate_ = native_.freq > 0 ? native_.freq : device.freq;
    mix_spec_ = SDL_AudioSpec{SDL_AUDIO_F32, kChannels, rate_};
    SDL_ResumeAudioStreamDevice(stream_);
    return true;
}

bool Audio::available() {
    return ensure_device();
}

int Audio::sample_rate() {
    return ensure_device() ? rate_ : 0;
}

void Audio::shutdown() {
    // Destroying the stream stops the callback before anything it reads is freed.
    if (stream_ != nullptr) {
        SDL_DestroyAudioStream(stream_);
        stream_ = nullptr;
    }

    for (Sound& sound : sounds_) {
        SDL_free(sound.samples);
        sound.samples = nullptr;
        sound.frames = 0;
        ++sound.generation;
    }

    for (Voice& voice : voices_) {
        voice = Voice{};
    }
    for (VoiceState& state : state_) {
        state.generation.store(0);
        state.claimed.store(false);
        state.active.store(false);
        state.progress.store(0.0f);
    }

    command_read_.store(0);
    command_write_.store(0);
    rate_ = 0;
}

std::uint64_t Audio::load(const char* path) {
    if (path == nullptr or !ensure_device()) return 0;

    DecodedAudio decoded;
    if (!decode_audio(path, decoded)) return 0;

    // The mixer always wants s16 stereo at the device rate; a mono, surround or
    // 48 kHz file is converted once, here, and never while mixing.
    std::int16_t* samples = decoded.samples;
    std::uint64_t frames = decoded.frames;

    if (decoded.channels != kChannels or decoded.rate != rate_) {
        const std::uint64_t bytes =
            frames * static_cast<std::uint64_t>(decoded.channels) * sizeof(std::int16_t);
        if (bytes > static_cast<std::uint64_t>(INT32_MAX)) {
            SDL_free(decoded.samples);
            SDL_SetError("sound is too long to convert");
            return 0;
        }

        const SDL_AudioSpec source{SDL_AUDIO_S16, decoded.channels, decoded.rate};
        const SDL_AudioSpec target{SDL_AUDIO_S16, kChannels, rate_};
        Uint8* converted = nullptr;
        Sint32 converted_length = 0;
        const bool ok = SDL_ConvertAudioSamples(&source, reinterpret_cast<const Uint8*>(decoded.samples),
                                                static_cast<int>(bytes), &target, &converted, &converted_length);
        SDL_free(decoded.samples);
        if (!ok or converted == nullptr) return 0;

        samples = reinterpret_cast<std::int16_t*>(converted);
        frames = static_cast<std::uint64_t>(converted_length) / (sizeof(std::int16_t) * kChannels);
    }

    if (frames == 0 or frames > UINT32_MAX) {
        SDL_free(samples);
        SDL_SetError("sound has no playable frames");
        return 0;
    }

    for (std::uint32_t index = kFirstHandleIndex; index < static_cast<std::uint32_t>(kSoundCapacity); ++index) {
        Sound& sound = sounds_[index];
        if (sound.samples != nullptr) continue;

        sound.samples = samples;
        sound.frames = static_cast<std::uint32_t>(frames);
        return handle_of(index, sound.generation);
    }

    SDL_free(samples);
    SDL_SetError("sound pool is full (%d sounds)", kSoundCapacity);
    return 0;
}

Audio::Sound* Audio::find_sound(std::uint64_t value) {
    const std::uint32_t index = handle_index(value);
    if (index < kFirstHandleIndex or index >= static_cast<std::uint32_t>(kSoundCapacity)) return nullptr;

    Sound& sound = sounds_[index];
    if (sound.samples == nullptr or sound.generation != handle_generation(value)) return nullptr;
    return &sound;
}

void Audio::destroy(std::uint64_t handle) {
    Sound* sound = find_sound(handle);
    if (sound == nullptr) return;

    // The mixer may still be reading these samples, so it frees them: the Free
    // command also stops every voice that was playing them.
    Command command;
    command.kind = CommandKind::Free;
    command.samples = sound->samples;

    sound->samples = nullptr;
    sound->frames = 0;
    ++sound->generation;

    // Only reachable when the queue is saturated; retry briefly rather than
    // freeing memory the mixer could still touch.
    for (int attempt = 0; attempt < 500; ++attempt) {
        if (push(command)) return;
        SDL_DelayNS(20000);
    }
}

std::uint32_t Audio::sound_frames(std::uint64_t handle) {
    Sound* sound = find_sound(handle);
    return sound != nullptr ? sound->frames : 0;
}

float Audio::sound_seconds(std::uint64_t handle) {
    Sound* sound = find_sound(handle);
    if (sound == nullptr) return 0.0f;
    const int rate = rate_ > 0 ? rate_ : 48000;
    return static_cast<float>(sound->frames) / static_cast<float>(rate);
}

bool Audio::push(const Command& command) {
    const std::uint32_t write = command_write_.load(std::memory_order_relaxed);
    const std::uint32_t read = command_read_.load(std::memory_order_acquire);
    if (write - read >= kCommandCapacity) return false;

    commands_[write % kCommandCapacity] = command;
    command_write_.store(write + 1, std::memory_order_release);
    return true;
}

bool Audio::pop(Command& command) {
    const std::uint32_t read = command_read_.load(std::memory_order_relaxed);
    if (read == command_write_.load(std::memory_order_acquire)) return false;

    command = commands_[read % kCommandCapacity];
    command_read_.store(read + 1, std::memory_order_release);
    return true;
}

std::uint32_t Audio::next_generation() {
    ++generation_counter_;
    if (generation_counter_ == 0) generation_counter_ = 1;
    return generation_counter_;
}

int Audio::claim_voice(std::uint32_t generation) {
    for (int index = 0; index < kVoiceCapacity; ++index) {
        VoiceState& state = state_[index];
        if (state.claimed.load(std::memory_order_relaxed)) continue;

        state.claimed.store(true);
        state.generation.store(generation);
        state.active.store(false);
        state.progress.store(0.0f);
        state.pending.store(0);
        return index;
    }

    // Pool is full: steal the oldest voice, exactly like pygame's mixer.
    int oldest = 0;
    for (int index = 1; index < kVoiceCapacity; ++index) {
        if (serials_[index] < serials_[oldest]) oldest = index;
    }

    VoiceState& state = state_[oldest];
    state.generation.store(generation);
    state.active.store(false);
    state.progress.store(0.0f);
    state.pending.store(0);
    return oldest;
}

void Audio::release(int index) {
    state_[index].claimed.store(false);
    state_[index].active.store(false);
    state_[index].progress.store(0.0f);
}

bool Audio::live_voice(std::uint64_t handle, int& index) const {
    const std::uint32_t slot = handle_index(handle);
    if (slot >= static_cast<std::uint32_t>(kVoiceCapacity)) return false;
    if (state_[slot].generation.load(std::memory_order_acquire) != handle_generation(handle)) return false;

    index = static_cast<int>(slot);
    return true;
}

std::uint64_t Audio::play(std::uint64_t sound_handle, float volume, float pan, float pitch, int loops, float fade_in) {
    Sound* sound = find_sound(sound_handle);
    if (sound == nullptr or !ensure_device()) return 0;

    const std::uint32_t generation = next_generation();
    const int index = claim_voice(generation);

    Command command;
    command.kind = CommandKind::Play;
    command.index = static_cast<std::uint32_t>(index);
    command.generation = generation;
    command.samples = sound->samples;
    command.frames = sound->frames;
    command.step = static_cast<std::uint32_t>(std::clamp(pitch, kMinPitch, kMaxPitch) * static_cast<float>(kPitchUnit));
    command.volume = volume;
    command.pan = pan;
    command.fade = fade_in;
    command.loops = loops;

    if (!push(command)) {
        release(index);
        return 0;
    }

    serials_[index] = ++serial_;
    state_[index].active.store(true);
    return handle_of(static_cast<std::uint32_t>(index), generation);
}

bool Audio::stop(std::uint64_t handle, float fade_seconds) {
    int index = 0;
    if (!live_voice(handle, index)) return false;

    Command command;
    command.kind = CommandKind::Stop;
    command.index = static_cast<std::uint32_t>(index);
    command.generation = handle_generation(handle);
    command.fade = fade_seconds;
    if (!push(command)) return false;

    if (fade_seconds <= 0.0f) state_[index].active.store(false);
    return true;
}

bool Audio::set_volume(std::uint64_t handle, float volume, float fade_seconds) {
    int index = 0;
    if (!live_voice(handle, index)) return false;

    VoiceState& state = state_[index];
    state.pending_volume.store(clamp_unit(volume), std::memory_order_relaxed);
    state.pending_fade.store(std::max(fade_seconds, 0.0f), std::memory_order_relaxed);
    state.pending.fetch_or(kPendingVolume, std::memory_order_release);
    return true;
}

bool Audio::set_pan(std::uint64_t handle, float pan) {
    int index = 0;
    if (!live_voice(handle, index)) return false;

    VoiceState& state = state_[index];
    state.pending_pan.store(std::clamp(pan, -1.0f, 1.0f), std::memory_order_relaxed);
    state.pending.fetch_or(kPendingPan, std::memory_order_release);
    return true;
}

bool Audio::set_pitch(std::uint64_t handle, float pitch) {
    int index = 0;
    if (!live_voice(handle, index)) return false;

    VoiceState& state = state_[index];
    state.pending_pitch.store(std::clamp(pitch, kMinPitch, kMaxPitch), std::memory_order_relaxed);
    state.pending.fetch_or(kPendingPitch, std::memory_order_release);
    return true;
}

bool Audio::playing(std::uint64_t handle) {
    int index = 0;
    if (!live_voice(handle, index)) return false;
    return state_[index].active.load(std::memory_order_acquire);
}

float Audio::progress(std::uint64_t handle) {
    int index = 0;
    if (!live_voice(handle, index)) return 0.0f;
    return state_[index].progress.load(std::memory_order_acquire);
}

void Audio::stop_all(float fade_seconds) {
    Command command;
    command.kind = CommandKind::StopAll;
    command.fade = fade_seconds;
    if (!push(command)) return;

    if (fade_seconds <= 0.0f) {
        for (VoiceState& state : state_) state.active.store(false);
    }
}

int Audio::active_voices() {
    int count = 0;
    for (const VoiceState& state : state_) {
        if (state.active.load(std::memory_order_relaxed)) ++count;
    }
    return count;
}

void Audio::set_master_volume(float volume) {
    master_target_.store(clamp_unit(volume), std::memory_order_relaxed);
}

float Audio::master_volume() const {
    return master_target_.load(std::memory_order_relaxed);
}

void Audio::pause(bool paused) {
    if (!ensure_device()) return;
    paused_.store(paused, std::memory_order_relaxed);
    if (paused) {
        SDL_PauseAudioStreamDevice(stream_);
    } else {
        SDL_ResumeAudioStreamDevice(stream_);
    }
}

bool Audio::paused() const {
    return paused_.load(std::memory_order_relaxed);
}

void Audio::set_ramp(Voice& voice, float left, float right, float seconds) {
    voice.target_left = left;
    voice.target_right = right;

    const float frames = std::max(1.0f, seconds * static_cast<float>(rate_));
    voice.ramp_left = (left - voice.gain_left) / frames;
    voice.ramp_right = (right - voice.gain_right) / frames;
}

void Audio::apply(const Command& command) {
    if (command.kind == CommandKind::Free) {
        for (Voice& voice : voices_) {
            if (voice.samples != command.samples) continue;
            voice.active = false;
            voice.samples = nullptr;
            voice.frames = 0;
            voice.position = 0;
        }
        SDL_free(const_cast<std::int16_t*>(command.samples));
        return;
    }

    if (command.kind == CommandKind::StopAll) {
        for (Voice& voice : voices_) {
            if (!voice.active) continue;
            if (command.fade <= 0.0f) {
                voice.active = false;
                continue;
            }
            voice.dying = true;
            set_ramp(voice, 0.0f, 0.0f, command.fade);
        }
        return;
    }

    if (command.index >= static_cast<std::uint32_t>(kVoiceCapacity)) return;

    Voice& voice = voices_[command.index];
    VoiceState& state = state_[command.index];
    // A stale command (the slot was recycled or the sound was freed) is dropped.
    if (state.generation.load(std::memory_order_relaxed) != command.generation) return;

    switch (command.kind) {
        case CommandKind::Play: {
            float pan_left = 1.0f;
            float pan_right = 1.0f;
            pan_gains(command.pan, pan_left, pan_right);

            voice.samples = command.samples;
            voice.frames = command.frames;
            voice.position = 0;
            voice.step = command.step;
            voice.generation = command.generation;
            voice.loops_left = command.loops < 0 ? -1 : command.loops;
            voice.volume = clamp_unit(command.volume);
            voice.pan = command.pan;
            voice.dying = false;
            voice.active = true;

            const float left = voice.volume * pan_left * kInt16ToFloat;
            const float right = voice.volume * pan_right * kInt16ToFloat;
            if (command.fade > 0.0f) {
                voice.gain_left = 0.0f;
                voice.gain_right = 0.0f;
                set_ramp(voice, left, right, command.fade);
            } else {
                voice.gain_left = left;
                voice.gain_right = right;
                voice.target_left = left;
                voice.target_right = right;
                voice.ramp_left = 0.0f;
                voice.ramp_right = 0.0f;
            }
            break;
        }
        case CommandKind::Stop: {
            if (command.fade <= 0.0f) {
                voice.active = false;
                break;
            }
            voice.dying = true;
            set_ramp(voice, 0.0f, 0.0f, command.fade);
            break;
        }
        case CommandKind::StopAll:
        case CommandKind::Free:
            break;
    }
}

template <bool kRamp>
void Audio::mix_voice(Voice& voice, int frames) {
    const std::int16_t* samples = voice.samples;
    const std::uint32_t last = voice.frames - 1;
    const std::uint64_t end = static_cast<std::uint64_t>(voice.frames) * kPitchUnit;

    float left = voice.gain_left;
    float right = voice.gain_right;

    float low_left = left;
    float high_left = left;
    float low_right = right;
    float high_right = right;
    float step_left = 0.0f;
    float step_right = 0.0f;
    if constexpr (kRamp) {
        step_left = voice.ramp_left;
        step_right = voice.ramp_right;
        // The ramp stops at its target, so clamping needs no per-sample branch.
        low_left = std::min(left, voice.target_left);
        high_left = std::max(left, voice.target_left);
        low_right = std::min(right, voice.target_right);
        high_right = std::max(right, voice.target_right);
    }

    for (int frame = 0; frame < frames; ++frame) {
        const std::uint64_t position = voice.position;
        const auto index = static_cast<std::uint32_t>(position >> 16);
        const std::uint32_t next = index < last ? index + 1 : last;
        const float blend = static_cast<float>(position & 0xffff) * (1.0f / 65536.0f);

        const float left0 = static_cast<float>(samples[index * kChannels]);
        const float right0 = static_cast<float>(samples[index * kChannels + 1]);
        const float left1 = static_cast<float>(samples[next * kChannels]);
        const float right1 = static_cast<float>(samples[next * kChannels + 1]);

        mix_[frame * kChannels] += (left0 + (left1 - left0) * blend) * left;
        mix_[frame * kChannels + 1] += (right0 + (right1 - right0) * blend) * right;

        if constexpr (kRamp) {
            left = std::clamp(left + step_left, low_left, high_left);
            right = std::clamp(right + step_right, low_right, high_right);
        }

        voice.position += voice.step;
        while (voice.position >= end) {
            if (voice.loops_left == 0) {
                voice.active = false;
                break;
            }
            if (voice.loops_left > 0) --voice.loops_left;
            voice.position -= end;
        }
        if (!voice.active) break;
    }

    voice.gain_left = left;
    voice.gain_right = right;
    if constexpr (kRamp) {
        if (step_left > 0.0f ? left >= voice.target_left : left <= voice.target_left) {
            voice.gain_left = voice.target_left;
            voice.ramp_left = 0.0f;
        }
        if (step_right > 0.0f ? right >= voice.target_right : right <= voice.target_right) {
            voice.gain_right = voice.target_right;
            voice.ramp_right = 0.0f;
        }
    }
}

void Audio::apply_pending() {
    for (int index = 0; index < kVoiceCapacity; ++index) {
        VoiceState& state = state_[index];
        const std::uint32_t pending = state.pending.exchange(0, std::memory_order_acquire);
        if (pending == 0) continue;

        Voice& voice = voices_[index];
        if (!voice.active or state.generation.load(std::memory_order_relaxed) != voice.generation) continue;

        if (pending & kPendingPitch) {
            voice.step = static_cast<std::uint32_t>(state.pending_pitch.load(std::memory_order_relaxed) *
                                                    static_cast<float>(kPitchUnit));
        }

        if (pending & kPendingPan) voice.pan = state.pending_pan.load(std::memory_order_relaxed);
        if (pending & kPendingVolume) voice.volume = state.pending_volume.load(std::memory_order_relaxed);

        if (pending & (kPendingPan | kPendingVolume)) {
            float pan_left = 1.0f;
            float pan_right = 1.0f;
            pan_gains(voice.pan, pan_left, pan_right);

            float seconds = 0.0f;
            if (pending & kPendingVolume) seconds = state.pending_fade.load(std::memory_order_relaxed);
            if (seconds <= 0.0f) seconds = kSmoothingSeconds;

            set_ramp(voice, voice.volume * pan_left * kInt16ToFloat, voice.volume * pan_right * kInt16ToFloat,
                     seconds);
        }
    }
}

void Audio::mix(int frames) {
    std::fill_n(mix_.begin(), static_cast<std::size_t>(frames) * kChannels, 0.0f);

    Command command;
    while (pop(command)) apply(command);
    apply_pending();

    for (int index = 0; index < kVoiceCapacity; ++index) {
        Voice& voice = voices_[index];
        if (!voice.active) continue;

        if (voice.ramp_left == 0.0f and voice.ramp_right == 0.0f) {
            mix_voice<false>(voice, frames);
        } else {
            mix_voice<true>(voice, frames);
        }
    }

    const float target = master_target_.load(std::memory_order_relaxed);
    master_applied_ += (target - master_applied_) * 0.25f;
    if (std::abs(target - master_applied_) < (1.0f / 4096.0f)) master_applied_ = target;

    if (master_applied_ != 1.0f) {
        const std::size_t count = static_cast<std::size_t>(frames) * kChannels;
        for (std::size_t sample = 0; sample < count; ++sample) mix_[sample] *= master_applied_;
    }

    for (int index = 0; index < kVoiceCapacity; ++index) {
        Voice& voice = voices_[index];
        if (voice.active and voice.dying and voice.ramp_left == 0.0f and voice.ramp_right == 0.0f) {
            voice.active = false;
        }
    }

    publish();
}

void Audio::publish() {
    for (int index = 0; index < kVoiceCapacity; ++index) {
        Voice& voice = voices_[index];
        VoiceState& state = state_[index];

        const std::uint32_t generation = state.generation.load(std::memory_order_relaxed);
        if (generation == 0 or generation != voice.generation) continue;

        state.active.store(voice.active, std::memory_order_relaxed);

        const std::uint64_t end = static_cast<std::uint64_t>(voice.frames) * kPitchUnit;
        const float value =
            end == 0 ? 0.0f
                     : std::clamp(static_cast<float>(static_cast<double>(voice.position) / static_cast<double>(end)),
                                  0.0f, 1.0f);
        state.progress.store(value, std::memory_order_relaxed);

        if (!voice.active) state.claimed.store(false, std::memory_order_release);
    }
}

void Audio::deliver(int frames) {
    if (stream_ == nullptr) return;

    const int samples = frames * kChannels;

    if (native_.format == SDL_AUDIO_F32 and native_.channels == kChannels) {
        SDL_PutAudioStreamData(stream_, mix_.data(), samples * static_cast<int>(sizeof(float)));
        return;
    }

    if (native_.format == SDL_AUDIO_S16 and native_.channels == kChannels) {
        for (int sample = 0; sample < samples; ++sample) {
            pcm_[sample] = static_cast<std::int16_t>(std::clamp(mix_[sample] * 32767.0f, -32768.0f, 32767.0f));
        }
        SDL_PutAudioStreamData(stream_, pcm_.data(), samples * static_cast<int>(sizeof(std::int16_t)));
        return;
    }

    // Unusual device formats (surround, 8-bit): SDL converts one block at a time.
    Uint8* converted = nullptr;
    int converted_length = 0;
    if (!SDL_ConvertAudioSamples(&mix_spec_, reinterpret_cast<const Uint8*>(mix_.data()),
                                 samples * static_cast<int>(sizeof(float)), &native_, &converted, &converted_length)) {
        return;
    }
    SDL_PutAudioStreamData(stream_, converted, converted_length);
    SDL_free(converted);
}

void SDLCALL Audio::callback(void* userdata, SDL_AudioStream* stream, int additional, int total) {
    (void)total;
    auto* self = static_cast<Audio*>(userdata);

    const int frame_bytes = SDL_AUDIO_BYTESIZE(self->native_.format) * self->native_.channels;
    if (additional <= 0 or frame_bytes <= 0) return;

    int remaining = additional / frame_bytes;
    while (remaining > 0) {
        const int frames = std::min(remaining, kMixFrames);
        self->mix(frames);
        self->deliver(frames);
        remaining -= frames;
    }
}

}  // namespace soshi
