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
blip = soshi.Sound("assets/blip.wav")   # WAV, converted to the device format at load
bed = blip.play(loops=-1, volume=0.25, pitch=0.5)   # looping bed
voice = blip.play(volume=0.6, pan=-0.5, pitch=1.25, fade_in=0.01)  # overlapping voice
voice.set_pan(0.5)
voice.set_pitch(0.8)
voice.stop(fade=0.05)
soshi.audio.set_volume(0.8)          # master
soshi.audio.pause(True)              # pause the device
soshi.audio.stop_all(fade=0.05)     # fade everything out
```

Audio is mixed natively: up to 32 voices, each with its own volume, pan, pitch and
loop count, plus fade in/out and master volume. Sounds are decoded to 16-bit stereo
at 44.1 kHz once at load and kept in RAM (no streaming); `Sound.seconds` reports
the length. `Sound.play` returns a `Voice` you can drive; voices are pooled, so
starting a 33rd steals the oldest one.

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
checkout at `vcpkg/` with `sdl3:x64-windows` installed.

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
`~/.pypirc`. Notes:

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
python examples\bench_world.py --entities 5000 --frames 300
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
* One renderer, 256 texture slots, 64 sound slots, and up to 32 distinct textures drawn per frame.
* Children inherit position and rotation from their parent; depth is capped at 8 levels.
* Physics covers gravity, wall bounces, and circle/circle impulses with a grid broad phase.
  Sprites use the same circular body.
* Audio: 32 mixed voices, each with volume/pan/pitch/loops/fades and a master volume; sounds are decoded to 16-bit stereo 44.1 kHz and held in RAM (no streaming), so total audio memory scales with the loaded files.
* Verified on Windows only. The C++ layer contains no OS-specific logic beyond a CRT shim
  for `sincosf`, so other platforms need only a rebuild, but they are untested here.
* Release binaries are stripped: no debug info, no PDB, no Python bytecode.
