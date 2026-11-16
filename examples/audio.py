"""Audio: overlapping voices, pan/pitch, fade, and a looping bed."""
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
    bed = blip.play(loops=-1, volume=0.25, pitch=0.5)   # looping bed under everything
    soshi.audio.set_volume(0.8)

    clock = soshi.Clock(60)
    since_play = 0.0
    plays = 0
    voices = []
    frames = 0

    while soshi.running:
        dt = clock.tick()
        soshi.event.pump()
        if soshi.event.key_pressed(soshi.K_ESCAPE):
            break

        since_play += dt
        if since_play >= 0.35 or soshi.event.key_released(soshi.K_SPACE):
            since_play = 0.0
            voice = blip.play(
                volume=0.6,
                pan=(plays % 5 - 2) / 2.0,
                pitch=1.0 + (plays % 3) * 0.25,
                fade_in=0.01,
            )
            voices.append(voice)
            if len(voices) > 8:
                voices.pop(0).stop(fade=0.05)
            plays += 1

        screen.clear((16, 14, 22))
        soshi.draw.circle(screen, (screen.width / 2, screen.height / 2), 40 + plays % 24, (240, 180, 90))
        soshi.display.flip()

        frames += 1
        if arguments.frames and frames >= arguments.frames:
            break

    bed.stop(fade=0.05)
    soshi.audio.stop_all(fade=0.05)
    print(f"ran {frames} frames, played {plays} blips, active voices: {soshi.audio.playing}, audio available: {soshi.audio.available()}")
    soshi.quit()


if __name__ == "__main__":
    main()
