"""Text: TTF/OTF fonts at several sizes, colors, measurement and multi-line output."""

import argparse
import pathlib
import sys

import soshi

# A font the game ships is the right answer; the demo looks for one on the system so
# it runs without adding a binary to the repository.
CANDIDATES = [
    "C:/Windows/Fonts/leelawui.ttf",  # Leelawadee UI: ships with Thai glyphs
    "C:/Windows/Fonts/tahoma.ttf",
    "C:/Windows/Fonts/segoeui.ttf",
    "C:/Windows/Fonts/arial.ttf",
    "/System/Library/Fonts/Supplemental/Arial.ttf",
    "/usr/share/fonts/truetype/noto/NotoSansThai-Regular.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
]


def find_font(explicit):
    if explicit:
        return pathlib.Path(explicit)
    for candidate in CANDIDATES:
        if pathlib.Path(candidate).exists():
            return pathlib.Path(candidate)
    return None


def main():
    parser = argparse.ArgumentParser(description="Soshi text demo")
    parser.add_argument("--font", help="path to a TTF, OTF or TTC file")
    parser.add_argument("--frames", type=int, default=0, help="stop after N frames (smoke tests)")
    arguments = parser.parse_args()

    path = find_font(arguments.font)
    if path is None:
        print("no system font found; pass --font <path to a ttf>")
        return 0

    soshi.init(width=640, height=360, title="Soshi text")
    screen = soshi.display.get_surface()

    title = soshi.Font(path, 40)
    body = soshi.Font(path, 20)
    small = soshi.Font(path, 14)
    print(f"{path.name}: line heights {title.height:.0f} / {body.height:.0f} / {small.height:.0f} px")

    clock = soshi.Clock(60)
    frames = 0
    score = 0

    while soshi.running:
        dt = clock.tick()
        soshi.event.pump()
        if soshi.event.key_pressed(soshi.K_ESCAPE):
            break

        score += int(dt * 100)

        screen.clear((16, 14, 22))
        soshi.draw.text(title, "SOSHI", (24, 18), (255, 210, 120))
        soshi.draw.text(body, f"score {score}", (24, 78), (230, 230, 246))
        soshi.draw.text(body, "ภาษาไทย: สวัสดีครับ", (24, 108), (140, 220, 255))
        soshi.draw.text(small, "multi-line text\nuses the font's line height", (24, 152), (150, 150, 170))

        measured = body.measure("score 99999")
        soshi.draw.text(small, "measured: %.0f x %.0f px" % measured, (24, 210), (110, 190, 130))

        width, _height = body.measure("right aligned")
        soshi.draw.text(body, "right aligned", (screen.width - width - 24, 300), (255, 130, 130))

        soshi.display.flip()

        frames += 1
        if arguments.frames and frames >= arguments.frames:
            break

    print(f"ran {frames} frames, score {score}")
    soshi.quit()
    return 0


if __name__ == "__main__":
    sys.exit(main())
