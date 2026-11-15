import ctypes

from ._native import SoshiError, check, lib, to_rgba

_position_scratch = (ctypes.c_float * 2)()


class Entity:
    """A body living in the native world; drawable as a circle or a sprite."""

    __slots__ = ("handle", "radius", "color", "texture")

    def __init__(
        self,
        *,
        pos=(0.0, 0.0),
        vel=(0.0, 0.0),
        radius=10.0,
        color=(255, 255, 255),
        texture=None,
        frame_size=None,
        frames=1,
        columns=None,
        fps=0.0,
        loop=True,
        rotation=0.0,
        parent=None,
    ):
        red, green, blue, alpha = to_rgba(color)
        self.handle = lib.soshi_entity_create(
            float(pos[0]),
            float(pos[1]),
            float(vel[0]),
            float(vel[1]),
            float(radius),
            red,
            green,
            blue,
            alpha,
        )
        if self.handle == 0:
            raise SoshiError(f"world is full ({world.capacity()} entities)")

        self.radius = float(radius)
        self.color = tuple(color)
        self.texture = texture

        if texture is not None:
            frame_width = int(frame_size[0]) if frame_size else int(texture.width)
            frame_height = int(frame_size[1]) if frame_size else int(texture.height)
            column_count = int(columns) if columns else max(1, int(texture.width) // frame_width)
            check(
                lib.soshi_entity_set_sprite(
                    self.handle,
                    texture.handle,
                    frame_width,
                    frame_height,
                    int(frames),
                    column_count,
                    float(fps),
                    1 if loop else 0,
                )
            )
            if frame_size is None:
                check(lib.soshi_entity_set_draw_size(self.handle, texture.width, texture.height))

        if rotation:
            self.set_rotation(rotation)
        if parent is not None:
            self.set_parent(parent)

    @property
    def pos(self):
        check(lib.soshi_entity_position(self.handle, _position_scratch))
        return (_position_scratch[0], _position_scratch[1])

    @pos.setter
    def pos(self, value):
        check(lib.soshi_entity_set_position(self.handle, float(value[0]), float(value[1])))

    def set_velocity(self, velocity):
        check(lib.soshi_entity_set_velocity(self.handle, float(velocity[0]), float(velocity[1])))

    def set_rotation(self, radians):
        check(lib.soshi_entity_set_rotation(self.handle, float(radians)))

    def set_size(self, width, height):
        check(lib.soshi_entity_set_draw_size(self.handle, float(width), float(height)))

    def set_parent(self, parent):
        """Attach to another entity; this entity's position becomes an offset from it."""
        parent_handle = 0 if parent is None else parent.handle
        check(lib.soshi_entity_set_parent(self.handle, parent_handle))

    def play(self):
        check(lib.soshi_entity_play_animation(self.handle))

    def stop(self):
        check(lib.soshi_entity_stop_animation(self.handle))

    def set_frame(self, index):
        check(lib.soshi_entity_set_frame(self.handle, int(index)))

    def update(self, dt, screen_rect=None):
        """Single-body convenience: sets the bounds, then steps the whole world.

        Scenes with many entities should call ``soshi.world.step(dt)`` once per frame
        instead of calling this per entity.
        """
        if screen_rect is not None:
            world.set_bounds(screen_rect)
        world.step(dt)

    def destroy(self):
        check(lib.soshi_entity_destroy(self.handle))
        self.handle = 0


class _World:
    __slots__ = ()

    def step(self, dt):
        lib.soshi_world_step(float(dt))

    def set_bounds(self, rect):
        lib.soshi_world_set_bounds(float(rect[0]), float(rect[1]), float(rect[2]), float(rect[3]))

    def set_gravity(self, value):
        lib.soshi_world_set_gravity(float(value))

    def set_restitution(self, value):
        lib.soshi_world_set_restitution(float(value))

    def count(self):
        return int(lib.soshi_world_count())

    def capacity(self):
        return int(lib.soshi_world_capacity())


world = _World()
