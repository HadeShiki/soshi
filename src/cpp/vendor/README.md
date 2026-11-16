# Vendored single-file libraries

Compiled into `soshi.dll`. There is no runtime dependency and nothing else to
install; the audio decoders cost about 240 KB of DLL size.

| file | format | upstream | license |
|---|---|---|---|
| `dr_wav.h` | WAV (PCM, float, ADPCM) | https://github.com/mackron/dr_libs | public domain (Unlicense) or MIT-0 |
| `dr_mp3.h` | MP3 (incl. ID3, VBR) | https://github.com/mackron/dr_libs | public domain (Unlicense) or MIT-0 |
| `dr_flac.h` | FLAC | https://github.com/mackron/dr_libs | public domain (Unlicense) or MIT-0 |
| `stb_vorbis.c` | OGG Vorbis | https://github.com/nothings/stb | public domain (Unlicense) or MIT |
| `stb_truetype.h` | TTF/OTF/TTC glyph rasterizer | https://github.com/nothings/stb | public domain (Unlicense) or MIT |

Each file keeps its own license header. They are used unmodified; only the
implementation macro (`*_IMPLEMENTATION`, `STB_VORBIS_HEADER_ONLY`) is set by
`src/cpp/decoder.cpp` and `src/cpp/font.cpp`. `stb_vorbis.c` is compiled as C by CMake,
the rest are included as C++.

Not supported: Ogg Opus, M4A/AAC, WMA and tracker modules (they would need libopus,
an AAC decoder and libxmp respectively, which are far larger than the four above).
Loading such a file fails with an error naming the format.
