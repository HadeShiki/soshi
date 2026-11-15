"""Audio: a queued WAV blip on a timer and on SPACE."""

import argparse
import pathlib

import soshi

ASSETS = pathlib.Path(__file__).with_name("assets")


def main():
    parser = argparse.ArgumentParser(description="Soshi audio demo")
    parser.add_argument("--frames", type=int, default=0, help="stop after N frames (smoke tests)")
    arguments = parser.parse_args()

    soshi.init(width=480, height=320, title="Soshi audio")
    screen = soshi.display.get_surface()

    blip = soshi.Sound(ASSETS / "blip.wav")
    soshi.audio.set_volume(0.7)

    clock = soshi.Clock(60)
    since_play = 0.0
    plays = 0
    frames = 0

    while soshi.running:
        dt = clock.tick()
        soshi.event.pump()
        if soshi.event.key_pressed(soshi.K_ESCAPE):
            break

        since_play += dt
        if since_play >= 0.5 or soshi.event.key_released(soshi.K_SPACE):
            since_play = 0.0
            blip.play()
            plays += 1

        screen.clear((16, 14, 22))
        soshi.draw.circle(screen, (screen.width / 2, screen.height / 2), 40 + plays % 24, (240, 180, 90))
        soshi.display.flip()

        frames += 1
        if arguments.frames and frames >= arguments.frames:
            break

    print(f"ran {frames} frames, played {plays} blips, audio available: {soshi.audio.available()}")
    soshi.quit()


if __name__ == "__main__":
    main()
