"""Sprites, sprite-sheet animation, rotation and a parented child."""

import argparse
import pathlib

import soshi

ASSETS = pathlib.Path(__file__).with_name("assets")


def main():
    parser = argparse.ArgumentParser(description="Soshi sprite demo")
    parser.add_argument("--frames", type=int, default=0, help="stop after N frames (smoke tests)")
    arguments = parser.parse_args()

    soshi.init(width=640, height=480, title="Soshi sprites")
    screen = soshi.display.get_surface()
    soshi.world.set_bounds((0.0, 0.0, screen.width, screen.height))

    sheet = soshi.Texture(ASSETS / "sheet.png")

    ship = soshi.Entity(pos=(320, 240), vel=(120, 90), radius=26, texture=sheet, frame_size=(32, 32), frames=1)
    ship.set_size(64, 64)

    thruster = soshi.Entity(
        pos=(320, 240 - 46),
        radius=12,
        texture=sheet,
        frame_size=(32, 32),
        frames=4,
        fps=10,
    )
    thruster.set_parent(ship)

    clock = soshi.Clock(60)
    angle = 0.0
    frames = 0

    while soshi.running:
        dt = clock.tick()
        soshi.event.pump()
        if soshi.event.key_pressed(soshi.K_ESCAPE):
            break

        angle += dt * 1.1
        ship.set_rotation(angle)
        soshi.world.step(dt)

        screen.clear((14, 16, 26))
        soshi.draw.rect(screen, (0, screen.height - 6, screen.width, 6), (48, 54, 78))
        soshi.draw.world()
        soshi.display.flip()

        frames += 1
        if arguments.frames and frames >= arguments.frames:
            break

    ship_position = ship.pos
    thruster_position = thruster.pos
    print(
        f"ran {frames} frames, ship at ({ship_position[0]:.1f}, {ship_position[1]:.1f}), "
        f"thruster at ({thruster_position[0]:.1f}, {thruster_position[1]:.1f})"
    )
    soshi.quit()


if __name__ == "__main__":
    main()
