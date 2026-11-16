"""Measure audio cost: mixing load and the cost of voice control calls."""

import argparse
import pathlib
import time

import soshi

ASSETS = pathlib.Path(__file__).resolve().parent.parent / "examples" / "assets"


def percentile(sorted_values, fraction):
    if not sorted_values:
        return 0.0
    index = min(len(sorted_values) - 1, int(len(sorted_values) * fraction))
    return sorted_values[index]


def main():
    parser = argparse.ArgumentParser(description="Soshi audio benchmark")
    parser.add_argument("--voices", type=int, default=32)
    parser.add_argument("--seconds", type=float, default=4.0)
    parser.add_argument("--ops", type=int, default=20000)
    arguments = parser.parse_args()

    soshi.init(width=160, height=120, title="Soshi audio bench")
    soshi.display.set_vsync(False)

    blip = soshi.Sound(ASSETS / "blip.wav")
    voices = []
    while len(voices) < arguments.voices:
        voices.append(blip.play(loops=-1, volume=0.2, pan=0.0, pitch=1.0))
        if len(voices) % 8 == 0:
            time.sleep(0.01)  # let the mixer pick them up before the pool wraps

    print(f"capacity     : {soshi.audio.capacity} voices")
    print(f"device       : {soshi.audio.sample_rate} Hz, {len(voices)} voices, {soshi.audio.playing} active")
    print(f"clip length  : {blip.seconds * 1000:.1f} ms")

    # Control-call cost: the old path took the audio device lock on every call.
    latencies = []
    for index in range(arguments.ops):
        started = time.perf_counter_ns()
        voices[index % len(voices)].set_pan(((index % 21) - 10) / 10.0)
        latencies.append(time.perf_counter_ns() - started)
    latencies.sort()
    print(f"set_pan      : median {percentile(latencies, 0.50) / 1000:.2f} us, "
          f"p99 {percentile(latencies, 0.99) / 1000:.2f} us, max {latencies[-1] / 1000:.2f} us "
          f"({arguments.ops} calls)")

    # Mixing load: the game thread sleeps, so the CPU time is the mixer thread.
    time.sleep(0.5)
    wall_start = time.perf_counter()
    cpu_start = time.process_time()
    time.sleep(arguments.seconds)
    wall = time.perf_counter() - wall_start
    cpu = time.process_time() - cpu_start
    mixed = wall * soshi.audio.sample_rate * len(voices)
    print(f"mix load     : {cpu:.3f} s cpu over {wall:.2f} s wall ({cpu / wall * 100:.1f}% of one core, "
          f"{len(voices)} looping voices, {cpu / max(mixed, 1) * 1e9:.1f} ns per voice-frame)")

    print(f"progress     : {voices[0].progress:.2f} (looping bed)")
    print(f"still playing: {soshi.audio.playing}")
    soshi.audio.stop_all(fade=0.0)
    soshi.quit()


if __name__ == "__main__":
    main()
