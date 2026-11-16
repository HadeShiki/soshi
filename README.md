# Soshi

A small, fast 2D engine for Python with a pygame-shaped API. Game scripts call a native
shared library through `ctypes`, so nothing is compiled on the player's machine: no C++
toolchain, no Python headers, and no build per Python version.

```
Python script  ->  soshi.dll  (C++20 + SDL3)  ->  soshi_core.lib  (Zig 0.17.1)
```

* C++ owns SDL3: window, renderer, textures, audio, events, clock, draw batches.
* Zig owns the engine core: math, entity table, physics, animation, scene graph.
* Only `extern "C"` functions taking plain C types cross the boundary.

## Using it

```
pip install soshi
```

```python
import soshi

soshi.init(width=800, height=600, title="Soshi Demo")
screen = soshi.display.get_surface()

ball = soshi.Entity(pos=(120, 120), vel=(260, 190), radius=24, color=(255, 90, 90))
clock = soshi.Clock(60)

while soshi.running:
    dt = clock.tick()
    soshi.event.pump()
    if soshi.event.key_pressed(soshi.K_ESCAPE):
        break

    ball.update(dt, screen_rect=screen.rect)
    screen.clear((18, 18, 28))
    soshi.draw.world()
    soshi.display.flip()

soshi.quit()
```

### Sprites and animation

```python
sheet = soshi.Texture("assets/sheet.png")               # PNG or BMP, loaded once

ship = soshi.Entity(pos=(320, 240), texture=sheet, frame_size=(32, 32), frames=1)
ship.set_size(64, 64)                                   # draw size, physics radius unchanged

thruster = soshi.Entity(
    pos=(320, 240 - 46),
    texture=sheet,
    frame_size=(32, 32),
    frames=4,        # four frames in the sheet
    fps=10,          # animated from spawn
)
thruster.set_parent(ship)                               # follows the ship, including rotation
```

Entities are drawn as circles while they have no texture and as sprites once they do;
`draw.world()` batches both (one geometry call for circles, one per texture).

### Audio

```python
blip = soshi.Sound("assets/blip.wav")               # WAV, decoded to the device rate once
bed = blip.play(loops=-1, volume=0.25, pitch=0.5)   # looping bed
voice = blip.play(volume=0.6, pan=-0.5, pitch=1.25, fade_in=0.01)  # overlapping voice
voice.set_pan(0.5)                   # constant-power pan, smoothed over 5 ms
voice.set_volume(0.3)                # ...so a slider move never clicks
voice.set_pitch(0.8)
voice.progress                       # 0.0 - 1.0 through the current pass
voice.busy                           # pygame-style alias of .playing
voice.stop(fade=0.05)
soshi.audio.master_volume = 0.8      # master gain, smoothed; set_volume(0.8) too
soshi.audio.sample_rate              # the device rate every sound is decoded to
soshi.audio.pause(True)              # pause the device
soshi.audio.stop_all(fade=0.05)      # fade everything out
```

Audio is mixed natively: 64 voices, each with its own volume, pan, pitch, loop
count and fades, plus a master gain. Two choices keep it off the frame budget:

* The game thread never waits on the audio device. `play` and `stop` post to a
  single-producer command queue, and volume/pan/pitch go to a per-voice mailbox
  the mixer reads once per block, so a pan tweak every frame costs one store and
  can never backlog. Measured 1.2 us median and 2.3 us p99 per call, nearly all of
  it the Python-to-C crossing.
* Nothing is resampled or converted while mixing. Each WAV is decoded once to
  16-bit stereo at the device's own rate, voices are summed in float32 (so a quiet
  voice keeps its resolution instead of being truncated per voice), and the
  finished block reaches SDL in the device's native format when that is float32
  stereo. 64 looping voices cost about 1.2% of one core.

`Sound` reads **WAV, FLAC, OGG Vorbis and MP3**. The format comes from the file's own
bytes, not the extension, so a mislabelled file still loads; a corrupt one fails with
a message that says so. The four decoders are vendored single-file libraries
(`dr_wav`, `dr_flac`, `dr_mp3`, `stb_vorbis`) compiled into `soshi.dll`, about 240 KB
of it, so a wheel still has no extra runtime dependency. Ogg Opus, M4A/AAC, WMA and
tracker modules are rejected with a clear error. See `src/cpp/vendor/README.md`.

Sounds live in RAM (no streaming), so `Sound.seconds` / `get_length()` reports the
length and memory grows with the loaded files. `Sound.play` returns a `Voice`; voices
are pooled, so starting the 65th steals the oldest one. Parameter changes land on
the next mix block, one device buffer away (typically 5 - 20 ms).

### Text

```python
font = soshi.Font("assets/Inter.ttf", 24)        # TTF, OTF or TTC at a pixel size
heading = soshi.Font("assets/Inter-Bold.ttf", 48)

soshi.draw.text(font, "Score: 120", (16, 12), (240, 240, 255))
soshi.draw.text(heading, "GAME OVER", (64, 40), (255, 120, 120))
soshi.draw.text(font, "line one\nline two", (16, 80), (200, 200, 200))

font.measure("Score: 120")   # -> (width, height) in pixels
font.height                  # line height in pixels
font.ascent                  # baseline offset from the top of the text
```

A glyph is rasterized the first time it is drawn and cached in a texture atlas per
(font, size), so creating a font is instant and the atlas only holds the characters a
game actually shows (256x256 to start, doubling up to 4096x4096). One native call
draws a whole string, with kerning, `\n` and UTF-8 handled natively, and the color you
pass tints the glyphs. The rasterizer is `stb_truetype`, vendored like the audio
decoders, so there is still no extra dependency.

It is not a full text shaper: scripts that need reordering or mark stacking (Thai
vowels and tone marks, Arabic, Indic) draw glyph by glyph in code point order. Pick a
font whose combining marks have zero advance for usable results; proper shaping needs
a shaper such as HarfBuzz, which is not included.

### Manual drawing

```python
soshi.draw.rect(screen, (10, 10, 200, 40), (60, 64, 92))
soshi.draw.circle(screen, (400, 300), 30, (255, 80, 80))
soshi.draw.circles(screen, [(x, y, radius, (120, 200, 255)) for x, y in positions])
```

## Frame cost

A frame crosses into native code a fixed number of times: `clock.tick()`,
`event.pump()`, one `world.step()`, one draw call, one `flip()`. Entity and primitive
counts never add crossings, and nothing allocates per frame.

Measured on this machine (Release build, 5000 entities, 800x600, 300 frames, windowed,
two runs):

| stage | ms/frame |
|---|---|
| world step (integrate, wall bounce, grid collisions, scene resolve, animation) | 1.7 |
| draw + present (5000 circles, one geometry call) | 6.2 - 6.5 |
| CPU total | 7.9 - 8.2 (about 122 - 126 fps) |

Headless (`SDL_VIDEODRIVER=dummy`) uses SDL's software rasterizer, so its draw cost is
not representative of a real window.

## Roadmap

| phase | scope | state |
|---|---|---|
| 0 | DLL load, window | done |
| 1 | clear, present, batched rects | done |
| 2 | events, keys, clock | done |
| 3 | entities, physics, bouncing ball | done |
| 4 | circles, textures, sprites | done |
| 5 | animation, scene graph, audio | done |
| 6 | wheel, docs, examples | wheel, sdist + PyPI packaging done; multi-platform CI is not |

## Building from source (Windows, verified)

Requirements: Visual Studio with the C++ toolset, CMake + Ninja, Zig 0.17.1, and a vcpkg
checkout at `vcpkg/` with `sdl3:x64-windows` installed. The audio decoders are vendored
in `src/cpp/vendor/`, so no extra package is needed for format support.

```
scripts\build.cmd
```

Outputs `build/soshi.dll` and copies it with `SDL3.dll` into `python/soshi/`.
Zig is looked up on `PATH`, then in the WinGet package folder; override with
`-DZIG_EXECUTABLE=<path to zig.exe>`. Set `VCVARS` to pin a specific `vcvars64.bat`.

The Zig core can also be built alone:

```
zig build-lib src/zig/soshi_core.zig -target native -O ReleaseFast -mcpu=baseline -fstrip -femit-bin=build/soshi_core.lib
```

Building a wheel (the binaries must already exist in `python/soshi/`):

```
py -3.12 -m pip wheel . -w dist --no-deps
```

## Publishing to PyPI

The version lives in one place, `python/soshi/version.py`; the wheel, the sdist and
`soshi.__version__` all read from it. To cut a release:

```
py -3.12 -m pip install setuptools twine
scripts\release.cmd
```

The script builds `dist/soshi-<version>-py3-none-win_amd64.whl` plus the sdist and runs
`twine check` on both. Upload to TestPyPI with a token first:

```
py -3.12 -m twine upload --repository testpypi dist/*
py -3.12 -m pip install --index-url https://test.pypi.org/simple soshi
```

When that installs and runs, upload for real:

```
py -3.12 -m twine upload dist/*
```

Username is `__token__` and the password is the API token, either typed or taken from
`~/.pypirc`.

### Releasing from a tag

`.github/workflows/release.yml` does the same build on `windows-latest` and publishes by
itself when a `v*` tag is pushed:

```
git tag v0.0.2
git push origin v0.0.2
```

The job builds the native libraries (zig + vcpkg `sdl3`), runs `scripts/release.cmd`, then
uploads `dist/*` with twine using the `PYPI_API_TOKEN` secret (repository secret, or an
environment secret on the `pypi` environment). Add a tag only after
`python/soshi/version.py` carries that version: `scripts/check_version.py` fails the run
when the two disagree, before anything reaches PyPI. A manual `workflow_dispatch` run
builds and keeps the artifacts without publishing.

Notes:

* `release.cmd` stops if `python/soshi/soshi.dll` or `SDL3.dll` is missing, and checks
  both artifacts again after the build, because a wheel without the DLLs cannot be
  replaced once it is on PyPI.
* The name comes from `project.name` in `pyproject.toml` and must be free on PyPI.
* Every upload needs a new version; PyPI rejects a re-upload of the same file.
* The wheel is tagged for the platform it was built on (Windows x64 here). The sdist
  carries the same binaries, so it installs without a toolchain on Windows, but a wheel
  per target platform is what other systems need.
* Do not run `python -m build` from the repository root: the local `build/` output folder
  shadows the `build` package. `scripts\release.cmd` avoids the problem.

## Examples

The demo assets are generated, not checked in as binaries of unknown origin:

```
python scripts/make_assets.py     # examples/assets/sheet.png + blip.wav
```

```
set PYTHONPATH=python
python examples\bouncing_ball.py
python examples\sprites.py
python examples\audio.py
python examples\text.py
python examples\bench_world.py --entities 5000 --frames 300
python scripts\bench_audio.py --voices 64 --seconds 8
```

Headless smoke tests:

```
set SDL_VIDEODRIVER=dummy
set SDL_AUDIODRIVER=dummy
python examples\bouncing_ball.py --frames 120
python examples\sprites.py --frames 120
python examples\audio.py --frames 60
```

## Limits

* One world per process with 8192 entity slots, all allocated up front.
* One renderer, 256 texture slots, 64 sound slots, 16 fonts, and up to 32 distinct textures drawn per frame.
* Children inherit position and rotation from their parent; depth is capped at 8 levels.
* Physics covers gravity, wall bounces, and circle/circle impulses with a grid broad phase.
  Sprites use the same circular body.
* Text: 16 fonts, one glyph atlas per (font, size) up to 4096x4096, glyphs cached on first use. No shaping (HarfBuzz) for complex scripts.
* Audio: 64 mixed voices, each with volume/pan/pitch/loops/fades and a master gain; sounds are decoded to 16-bit stereo at the device rate and held in RAM (no streaming), so total audio memory scales with the loaded files. Sources are WAV, FLAC, OGG Vorbis and MP3 (vendored decoders, ~240 KB of the DLL); Ogg Opus and M4A/AAC are not decoded.
* Verified on Windows only. The C++ layer contains no OS-specific logic beyond a CRT shim
  for `sincosf`, so other platforms need only a rebuild, but they are untested here.
* Release binaries are stripped: no debug info, no PDB, no Python bytecode.
