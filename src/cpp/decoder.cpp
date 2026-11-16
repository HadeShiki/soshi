// Audio file decoding: one small vendored decoder per format, no external libraries.
//   WAV   dr_wav    (also handles 8/24/32-bit and ADPCM variants)
//   FLAC  dr_flac
//   MP3   dr_mp3    (includes ID3 tags and VBR headers)
//   OGG   stb_vorbis (Vorbis only; Ogg Opus is reported as unsupported)
// Everything is decoded to interleaved 16-bit frames; Audio::load converts the
// channel count and sample rate once, at load time.

#include "decoder.h"

#include <SDL3/SDL.h>

#define DR_WAV_IMPLEMENTATION
#include "vendor/dr_wav.h"

#define DR_MP3_IMPLEMENTATION
#include "vendor/dr_mp3.h"

#define DR_FLAC_IMPLEMENTATION
#include "vendor/dr_flac.h"

#define STB_VORBIS_HEADER_ONLY
#include "vendor/stb_vorbis.c"

#include <algorithm>
#include <cstring>

namespace soshi {
namespace {

// Frames decoded per read; also the growth step for the output buffer.
constexpr std::uint64_t kChunkFrames = 4096;

enum class Format { unknown, wav, flac, ogg, mp3 };

bool starts_with(const unsigned char* data, std::size_t size, const char* tag, std::size_t length) {
    return size >= length and std::memcmp(data, tag, length) == 0;
}

bool contains(const unsigned char* data, std::size_t size, const char* tag) {
    const std::size_t length = std::strlen(tag);
    if (length == 0 or size < length) return false;
    return std::search(data, data + size, tag, tag + length) != data + size;
}

Format sniff(const unsigned char* data, std::size_t size) {
    if (starts_with(data, size, "RIFF", 4) or starts_with(data, size, "RF64", 4)) return Format::wav;
    if (starts_with(data, size, "fLaC", 4)) return Format::flac;
    if (starts_with(data, size, "OggS", 4)) return Format::ogg;
    if (starts_with(data, size, "ID3", 3)) return Format::mp3;
    if (size >= 2 and data[0] == 0xFF and (data[1] & 0xE0) == 0xE0) return Format::mp3;
    return Format::unknown;
}

// Growable interleaved s16 buffer; takes care of itself on every early return.
struct SampleBuffer {
    std::int16_t* data = nullptr;
    std::uint64_t capacity = 0;  // frames
    std::uint64_t frames = 0;

    ~SampleBuffer() {
        SDL_free(data);
    }

    bool reserve(std::uint64_t wanted, int channels) {
        if (wanted <= capacity) return true;

        const std::uint64_t grown = std::max(wanted, capacity * 2);
        if (grown > (SIZE_MAX / static_cast<std::uint64_t>(channels)) / sizeof(std::int16_t)) return false;

        void* next = SDL_realloc(data, static_cast<std::size_t>(grown) * static_cast<std::size_t>(channels) *
                                           sizeof(std::int16_t));
        if (next == nullptr) return false;

        data = static_cast<std::int16_t*>(next);
        capacity = grown;
        return true;
    }
};

// Pulls s16 frames out of a decoder until it runs dry, growing the buffer as needed.
// ``estimate`` is only a hint: it avoids reallocations when the header knows the length.
template <typename Reader>
bool decode_loop(Reader& reader, int channels, int rate, std::uint64_t estimate, DecodedAudio& out) {
    if (channels <= 0 or rate <= 0) return false;

    SampleBuffer buffer;
    if (estimate > 0) buffer.reserve(estimate, channels);

    for (;;) {
        if (!buffer.reserve(buffer.frames + kChunkFrames, channels)) {
            SDL_SetError("out of memory while decoding audio");
            return false;
        }

        const std::uint64_t got =
            reader(buffer.data + buffer.frames * static_cast<std::uint64_t>(channels), kChunkFrames);
        buffer.frames += got;
        if (got < kChunkFrames) break;
    }

    if (buffer.frames == 0) {
        SDL_SetError("audio file contains no samples");
        return false;
    }

    out.samples = buffer.data;
    out.frames = buffer.frames;
    out.channels = channels;
    out.rate = rate;
    buffer.data = nullptr;  // ownership moves to the caller
    return true;
}

bool decode_wav(const void* data, std::size_t size, DecodedAudio& out) {
    drwav wav;
    if (!drwav_init_memory(&wav, data, size, nullptr)) return false;

    auto reader = [&wav](std::int16_t* destination, std::uint64_t frames) -> std::uint64_t {
        return drwav_read_pcm_frames_s16(&wav, frames, destination);
    };

    const bool ok = decode_loop(reader, static_cast<int>(wav.channels), static_cast<int>(wav.sampleRate),
                                wav.totalPCMFrameCount, out);
    drwav_uninit(&wav);
    return ok;
}

bool decode_flac(const void* data, std::size_t size, DecodedAudio& out) {
    drflac* flac = drflac_open_memory(data, size, nullptr);
    if (flac == nullptr) return false;

    auto reader = [flac](std::int16_t* destination, std::uint64_t frames) -> std::uint64_t {
        return drflac_read_pcm_frames_s16(flac, frames, destination);
    };

    const bool ok = decode_loop(reader, static_cast<int>(flac->channels), static_cast<int>(flac->sampleRate),
                                flac->totalPCMFrameCount, out);
    drflac_close(flac);
    return ok;
}

bool decode_mp3(const void* data, std::size_t size, DecodedAudio& out) {
    drmp3 mp3;
    if (!drmp3_init_memory(&mp3, data, size, nullptr)) return false;

    auto reader = [&mp3](std::int16_t* destination, std::uint64_t frames) -> std::uint64_t {
        return drmp3_read_pcm_frames_s16(&mp3, frames, destination);
    };

    const std::uint64_t estimate = mp3.totalPCMFrameCount == DRMP3_UINT64_MAX ? 0 : mp3.totalPCMFrameCount;
    const bool ok =
        decode_loop(reader, static_cast<int>(mp3.channels), static_cast<int>(mp3.sampleRate), estimate, out);
    drmp3_uninit(&mp3);
    return ok;
}

bool decode_ogg(const void* data, std::size_t size, DecodedAudio& out) {
    if (size > static_cast<std::size_t>(INT32_MAX)) return false;

    int error = 0;
    stb_vorbis* vorbis = stb_vorbis_open_memory(static_cast<const unsigned char*>(data), static_cast<int>(size), &error,
                                                nullptr);
    if (vorbis == nullptr) return false;

    const stb_vorbis_info info = stb_vorbis_get_info(vorbis);

    // Two output channels: stb_vorbis duplicates mono and folds the rest, so the
    // result always matches the mixer's stereo input.
    auto reader = [vorbis](std::int16_t* destination, std::uint64_t frames) -> std::uint64_t {
        const int got = stb_vorbis_get_samples_short_interleaved(vorbis, 2, destination,
                                                                 static_cast<int>(frames) * 2);
        return got > 0 ? static_cast<std::uint64_t>(got) : 0;
    };

    const bool ok = decode_loop(reader, 2, static_cast<int>(info.sample_rate),
                                stb_vorbis_stream_length_in_samples(vorbis), out);
    stb_vorbis_close(vorbis);
    return ok;
}

}  // namespace

bool decode_audio(const char* path, DecodedAudio& out) {
    out = DecodedAudio{};
    if (path == nullptr) {
        SDL_SetError("missing sound path");
        return false;
    }

    std::size_t size = 0;
    void* data = SDL_LoadFile(path, &size);
    if (data == nullptr) return false;  // SDL already set a useful error

    const auto* bytes = static_cast<const unsigned char*>(data);
    bool ok = false;

    switch (sniff(bytes, size)) {
        case Format::wav:
            ok = decode_wav(data, size, out);
            break;
        case Format::flac:
            ok = decode_flac(data, size, out);
            break;
        case Format::ogg:
            ok = decode_ogg(data, size, out);
            break;
        case Format::mp3:
            ok = decode_mp3(data, size, out);
            break;
        case Format::unknown:
            break;
    }

    // Sniffing can miss (a WAV behind a junk chunk, an MP3 with no ID3 tag), so every
    // decoder gets a turn before the file is rejected.
    if (!ok and sniff(bytes, size) != Format::wav) ok = decode_wav(data, size, out);
    if (!ok) ok = decode_flac(data, size, out);
    if (!ok) ok = decode_ogg(data, size, out);
    if (!ok) ok = decode_mp3(data, size, out);

    if (!ok) {
        if (contains(bytes, std::min<std::size_t>(size, 512), "OpusHead")) {
            SDL_SetError("Ogg Opus is not supported; use Vorbis, MP3, FLAC or WAV");
        } else {
            SDL_SetError("unsupported or corrupt audio file: %s", path);
        }
    }

    SDL_free(data);
    return ok;
}

}  // namespace soshi
