"""Bouncing ball: the smallest complete Soshi scene."""

import argparse

import soshi


def main():
    parser = argparse.ArgumentParser(description="Soshi bouncing ball demo")
    parser.add_argument("--frames", type=int, default=0, help="stop after N frames (smoke tests)")
    arguments = parser.parse_args()

    soshi.init(width=800, height=600, title="Soshi Demo")
    screen = soshi.display.get_surface()
    bounds = (0.0, 0.0, screen.width, screen.height)

    ball = soshi.Entity(pos=(120, 120), vel=(260, 190), radius=24, color=(255, 90, 90))
    clock = soshi.Clock(60)
    frames = 0

    while soshi.running:
        dt = clock.tick()
        soshi.event.pump()
        if soshi.event.key_pressed(soshi.K_ESCAPE):
            break

        ball.update(dt, screen_rect=bounds)

        screen.clear((18, 18, 28))
        soshi.draw.rect(screen, (0, screen.height - 4, screen.width, 4), (60, 64, 92))
        soshi.draw.world()
        soshi.display.flip()

        frames += 1
        if arguments.frames and frames >= arguments.frames:
            break

    position = ball.pos
    print(f"ran {frames} frames, ball at ({position[0]:.1f}, {position[1]:.1f})")
    soshi.quit()


if __name__ == "__main__":
    main()
