#include "audio.h"

#include "handles.h"

#include <algorithm>

namespace soshi {
namespace {

constexpr int kSampleRate = 44100;
constexpr int kChannels = 2;
constexpr int kFrameBytes = static_cast<int>(sizeof(std::int16_t)) * kChannels;
constexpr int kMixFrames = 2048;
constexpr float kPitchUnitF = static_cast<float>(kPitchUnit);

float clamp_unit(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float fade_step(float delta, float seconds) {
    return delta / std::max(1.0f, seconds * static_cast<float>(kSampleRate));
}

}  // namespace

bool Audio::ensure_device() {
    if (stream_ != nullptr) return true;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;

    spec_ = SDL_AudioSpec{SDL_AUDIO_S16, kChannels, kSampleRate};
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec_, &Audio::callback, this);
    if (stream_ == nullptr) return false;

    SDL_ResumeAudioStreamDevice(stream_);
    return true;
}

bool Audio::available() {
    return ensure_device();
}

int Audio::sample_rate() const {
    return kSampleRate;
}

void Audio::shutdown() {
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
        voice.active = false;
        voice.samples = nullptr;
        ++voice.generation;
    }
}

void Audio::lock() {
    if (stream_ != nullptr) SDL_LockAudioStream(stream_);
}

void Audio::unlock() {
    if (stream_ != nullptr) SDL_UnlockAudioStream(stream_);
}

std::uint64_t Audio::load(const char* path) {
    if (path == nullptr or !ensure_device()) return 0;

    SDL_AudioSpec source_spec{};
    Uint8* source_data = nullptr;
    Uint32 source_length = 0;
    if (!SDL_LoadWAV(path, &source_spec, &source_data, &source_length)) return 0;

    Uint8* converted = nullptr;
    Sint32 converted_length = 0;
    const bool converted_ok =
        SDL_ConvertAudioSamples(&source_spec, source_data, static_cast<int>(source_length), &spec_, &converted,
                                &converted_length);
    SDL_free(source_data);
    if (!converted_ok) return 0;

    const auto frames = static_cast<std::uint32_t>(converted_length) / static_cast<std::uint32_t>(kFrameBytes);
    if (frames == 0) {
        SDL_free(converted);
        return 0;
    }

    for (std::uint32_t index = kFirstHandleIndex; index < static_cast<std::uint32_t>(kSoundCapacity); ++index) {
        Sound& sound = sounds_[index];
        if (sound.samples != nullptr) continue;

        sound.samples = reinterpret_cast<std::int16_t*>(converted);
        sound.frames = frames;
        return handle_of(index, sound.generation);
    }

    SDL_free(converted);
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
    lock();
    Sound* sound = find_sound(handle);
    if (sound != nullptr) {
        SDL_free(sound->samples);
        sound->samples = nullptr;
        sound->frames = 0;
        ++sound->generation;
    }
    unlock();
}

std::uint32_t Audio::sound_frames(std::uint64_t handle) {
    Sound* sound = find_sound(handle);
    return sound != nullptr ? sound->frames : 0;
}

Audio::Voice* Audio::find_voice(std::uint64_t value) {
    const std::uint32_t index = handle_index(value);
    if (index >= static_cast<std::uint32_t>(kVoiceCapacity)) return nullptr;

    Voice& voice = voices_[index];
    if (voice.generation != handle_generation(value)) return nullptr;
    return &voice;
}

std::uint64_t Audio::voice_handle(const Voice* voice) const {
    const auto index = static_cast<std::uint32_t>(voice - voices_.data());
    return handle_of(index, voice->generation);
}

Audio::Voice* Audio::claim_voice() {
    Voice* chosen = nullptr;
    for (Voice& voice : voices_) {
        if (voice.active) continue;
        chosen = &voice;
        break;
    }

    if (chosen == nullptr) {
        for (Voice& voice : voices_) {
            if (chosen == nullptr or voice.serial < chosen->serial) chosen = &voice;
        }
    }

    ++chosen->generation;
    return chosen;
}

std::uint64_t Audio::play(std::uint64_t sound_handle, float volume, float pan, float pitch, int loops, float fade_in) {
    Sound* sound = find_sound(sound_handle);
    if (sound == nullptr or !ensure_device()) return 0;

    std::uint64_t handle = 0;
    lock();
    Voice* voice = claim_voice();
    if (voice != nullptr) {
        voice->samples = sound->samples;
        voice->frames = sound->frames;
        voice->position = 0;
        voice->step = static_cast<std::uint32_t>(std::clamp(pitch, 0.05f, 8.0f) * kPitchUnitF);
        voice->pan = static_cast<std::int32_t>(std::clamp(pan, -1.0f, 1.0f) * 128.0f);
        voice->loops_left = loops < 0 ? -1 : loops;
        voice->target = clamp_unit(volume);
        voice->gain = fade_in > 0.0f ? 0.0f : voice->target;
        voice->ramp = fade_in > 0.0f ? fade_step(voice->target, fade_in) : 0.0f;
        voice->serial = ++serial_;
        voice->active = true;
        handle = voice_handle(voice);
    }
    unlock();
    return handle;
}

bool Audio::stop(std::uint64_t handle, float fade_seconds) {
    lock();
    Voice* voice = find_voice(handle);
    if (voice == nullptr) {
        unlock();
        return false;
    }

    if (fade_seconds <= 0.0f) {
        voice->active = false;
        voice->ramp = 0.0f;
    } else {
        voice->target = 0.0f;
        voice->ramp = -fade_step(voice->gain, fade_seconds);
    }
    unlock();
    return true;
}

bool Audio::set_volume(std::uint64_t handle, float volume, float fade_seconds) {
    lock();
    Voice* voice = find_voice(handle);
    if (voice == nullptr) {
        unlock();
        return false;
    }

    voice->target = clamp_unit(volume);
    voice->ramp = fade_seconds <= 0.0f ? 0.0f : fade_step(voice->target - voice->gain, fade_seconds);
    if (fade_seconds <= 0.0f) voice->gain = voice->target;
    unlock();
    return true;
}

bool Audio::set_pan(std::uint64_t handle, float pan) {
    lock();
    Voice* voice = find_voice(handle);
    if (voice == nullptr) {
        unlock();
        return false;
    }

    voice->pan = static_cast<std::int32_t>(std::clamp(pan, -1.0f, 1.0f) * 128.0f);
    unlock();
    return true;
}

bool Audio::set_pitch(std::uint64_t handle, float pitch) {
    lock();
    Voice* voice = find_voice(handle);
    if (voice == nullptr) {
        unlock();
        return false;
    }

    voice->step = static_cast<std::uint32_t>(std::clamp(pitch, 0.05f, 8.0f) * kPitchUnitF);
    unlock();
    return true;
}

bool Audio::playing(std::uint64_t handle) {
    lock();
    Voice* voice = find_voice(handle);
    const bool active = voice != nullptr and voice->active;
    unlock();
    return active;
}

float Audio::progress(std::uint64_t handle) {
    lock();
    Voice* voice = find_voice(handle);
    float value = 0.0f;
    if (voice != nullptr and voice->frames != 0) {
        const std::uint32_t end = voice->frames * kPitchUnit;
        value = static_cast<float>(voice->position) / static_cast<float>(end);
        value = std::clamp(value, 0.0f, 1.0f);
    }
    unlock();
    return value;
}

void Audio::stop_all(float fade_seconds) {
    lock();
    for (Voice& voice : voices_) {
        if (!voice.active) continue;
        if (fade_seconds <= 0.0f) {
            voice.active = false;
            voice.ramp = 0.0f;
        } else {
            voice.target = 0.0f;
            voice.ramp = -fade_step(voice.gain, fade_seconds);
        }
    }
    unlock();
}

int Audio::active_voices() {
    lock();
    int count = 0;
    for (const Voice& voice : voices_) {
        if (voice.active) ++count;
    }
    unlock();
    return count;
}

void Audio::set_master_volume(float volume) {
    master_ = clamp_unit(volume);
}

void Audio::pause(bool paused) {
    if (!ensure_device()) return;
    if (paused) {
        SDL_PauseAudioStreamDevice(stream_);
    } else {
        SDL_ResumeAudioStreamDevice(stream_);
    }
}

void Audio::mix_frames(int frames) {
    std::fill_n(accumulator_.begin(), static_cast<std::size_t>(frames) * kChannels, 0);

    const float master = master_;
    for (Voice& voice : voices_) {
        if (!voice.active or voice.frames == 0) continue;

        const float pan = static_cast<float>(voice.pan) / 128.0f;
        const float left_gain = (pan > 0.0f ? 1.0f - pan : 1.0f) * master;
        const float right_gain = (pan < 0.0f ? 1.0f + pan : 1.0f) * master;
        const std::uint32_t end = voice.frames * kPitchUnit;

        for (int frame = 0; frame < frames; ++frame) {
            const std::uint32_t index = voice.position >> 16;
            const std::uint32_t next = index + 1 < voice.frames ? index + 1 : index;
            const float fraction = static_cast<float>(voice.position & 0xffff) / kPitchUnitF;

            const std::int16_t* samples = voice.samples;
            const float left =
                static_cast<float>(samples[index * kChannels]) +
                (static_cast<float>(samples[next * kChannels]) - static_cast<float>(samples[index * kChannels])) *
                    fraction;
            const float right =
                static_cast<float>(samples[index * kChannels + 1]) +
                (static_cast<float>(samples[next * kChannels + 1]) -
                 static_cast<float>(samples[index * kChannels + 1])) *
                    fraction;

            accumulator_[frame * kChannels] += static_cast<std::int32_t>(left * voice.gain * left_gain);
            accumulator_[frame * kChannels + 1] += static_cast<std::int32_t>(right * voice.gain * right_gain);

            voice.position += voice.step;
            while (voice.position >= end) {
                if (voice.loops_left == 0) {
                    voice.active = false;
                    break;
                }
                if (voice.loops_left > 0) --voice.loops_left;
                voice.position -= end;
            }

            if (voice.ramp != 0.0f) {
                voice.gain += voice.ramp;
                const bool reached = voice.ramp > 0.0f ? voice.gain >= voice.target : voice.gain <= voice.target;
                if (!reached) continue;

                voice.gain = voice.target;
                voice.ramp = 0.0f;
                if (voice.gain <= 0.0f) voice.active = false;
            }
        }
    }

    for (std::size_t index = 0; index < static_cast<std::size_t>(frames) * kChannels; ++index) {
        output_[index] = static_cast<std::int16_t>(std::clamp(accumulator_[index], -32768, 32767));
    }
}

void SDLCALL Audio::callback(void* userdata, SDL_AudioStream* stream, int additional, int total) {
    (void)total;
    auto* self = static_cast<Audio*>(userdata);

    int remaining = additional / kFrameBytes;
    while (remaining > 0) {
        const int frames = std::min(remaining, kMixFrames);
        self->mix_frames(frames);
        SDL_PutAudioStreamData(stream, self->output_.data(), frames * kFrameBytes);
        remaining -= frames;
    }
}

}  // namespace soshi
