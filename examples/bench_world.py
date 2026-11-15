"""Measure step and draw cost for a crowded world."""

import argparse
import random
import time

import soshi


def main():
    parser = argparse.ArgumentParser(description="Soshi world benchmark")
    parser.add_argument("--entities", type=int, default=5000)
    parser.add_argument("--frames", type=int, default=300)
    parser.add_argument("--gravity", type=float, default=0.0)
    arguments = parser.parse_args()

    soshi.init(width=800, height=600, title="Soshi bench")
    soshi.display.set_vsync(False)
    screen = soshi.display.get_surface()

    soshi.world.set_bounds((0.0, 0.0, screen.width, screen.height))
    soshi.world.set_gravity(arguments.gravity)

    random.seed(7)
    for _ in range(arguments.entities):
        soshi.Entity(
            pos=(random.uniform(20.0, screen.width - 20.0), random.uniform(20.0, screen.height - 20.0)),
            vel=(random.uniform(-90.0, 90.0), random.uniform(-90.0, 90.0)),
            radius=random.uniform(3.0, 9.0),
            color=(120, 200, 255),
        )

    dt = 1.0 / 60.0
    step_seconds = 0.0
    draw_seconds = 0.0

    for _ in range(arguments.frames):
        started = time.perf_counter()
        soshi.world.step(dt)
        stepped = time.perf_counter()

        screen.clear((10, 10, 16))
        soshi.draw.world()
        soshi.display.flip()
        draw_seconds += time.perf_counter() - stepped
        step_seconds += stepped - started

    frames = arguments.frames
    total = step_seconds + draw_seconds
    print(f"entities     : {soshi.world.count()}")
    print(f"frames       : {frames}")
    print(f"step         : {step_seconds / frames * 1000:.3f} ms/frame")
    print(f"draw+present : {draw_seconds / frames * 1000:.3f} ms/frame")
    print(f"cpu frame    : {total / frames * 1000:.3f} ms")
    print(f"cpu fps      : {frames / total:.0f}")

    soshi.quit()


if __name__ == "__main__":
    main()
