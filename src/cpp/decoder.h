#pragma once

#include <cstdint>

namespace soshi {

// One decoded sound: interleaved 16-bit frames in the file's own channel layout and
// sample rate, allocated with SDL_malloc. Free it with SDL_free.
struct DecodedAudio {
    std::int16_t* samples = nullptr;
    std::uint64_t frames = 0;
    int channels = 0;
    int rate = 0;
};

// Decodes WAV, FLAC, OGG Vorbis or MP3 from disk. The format is detected from the
// file's magic bytes, not its extension, and every decoder is tried as a fallback.
// The whole file is read into memory first, so UTF-8 paths work everywhere and the
// decoders never touch the filesystem.
bool decode_audio(const char* path, DecodedAudio& out);

}  // namespace soshi
