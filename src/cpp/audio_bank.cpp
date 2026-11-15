#include "audio_bank.h"

#include "handles.h"

namespace soshi {
namespace {

constexpr int kSampleRate = 44100;

}  // namespace

bool AudioBank::ensure_device() {
    if (output_ != nullptr) return true;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;

    output_spec_ = SDL_AudioSpec{SDL_AUDIO_S16, 2, kSampleRate};
    output_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &output_spec_, nullptr, nullptr);
    if (output_ == nullptr) return false;

    SDL_SetAudioStreamGain(output_, volume_);
    return true;
}

bool AudioBank::available() {
    return ensure_device();
}

void AudioBank::shutdown() {
    for (Sound& sound : sounds_) {
        SDL_free(sound.samples);
        sound.samples = nullptr;
        sound.length = 0;
    }

    if (output_ != nullptr) {
        SDL_DestroyAudioStream(output_);
        output_ = nullptr;
    }
}

std::uint64_t AudioBank::load(const char* path) {
    if (path == nullptr or !ensure_device()) return 0;

    SDL_AudioSpec source_spec{};
    Uint8* source_data = nullptr;
    Uint32 source_length = 0;
    if (!SDL_LoadWAV(path, &source_spec, &source_data, &source_length)) return 0;

    Uint8* converted = nullptr;
    Sint32 converted_length = 0;
    const bool converted_ok =
        SDL_ConvertAudioSamples(&source_spec, source_data, static_cast<int>(source_length), &output_spec_, &converted,
                                &converted_length);
    SDL_free(source_data);
    if (!converted_ok) return 0;

    for (std::uint32_t index = kFirstHandleIndex; index < static_cast<std::uint32_t>(kCapacity); ++index) {
        Sound& sound = sounds_[index];
        if (sound.samples != nullptr) continue;

        sound.samples = converted;
        sound.length = static_cast<Uint32>(converted_length);
        return handle_of(index, sound.generation);
    }

    SDL_free(converted);
    return 0;
}

AudioBank::Sound* AudioBank::find(std::uint64_t handle) {
    const std::uint32_t index = handle_index(handle);
    if (index < kFirstHandleIndex or index >= static_cast<std::uint32_t>(kCapacity)) return nullptr;

    Sound& sound = sounds_[index];
    if (sound.samples == nullptr or sound.generation != handle_generation(handle)) return nullptr;
    return &sound;
}

void AudioBank::destroy(std::uint64_t handle) {
    Sound* sound = find(handle);
    if (sound == nullptr) return;

    SDL_free(sound->samples);
    sound->samples = nullptr;
    sound->length = 0;
    ++sound->generation;
}

bool AudioBank::play(std::uint64_t handle) {
    Sound* sound = find(handle);
    if (sound == nullptr or output_ == nullptr) return false;

    SDL_PutAudioStreamData(output_, sound->samples, static_cast<int>(sound->length));
    SDL_ResumeAudioStreamDevice(output_);
    return true;
}

void AudioBank::set_volume(float volume) {
    volume_ = volume < 0.0f ? 0.0f : volume;
    if (output_ != nullptr) {
        SDL_SetAudioStreamGain(output_, volume_);
    }
}

}  // namespace soshi
