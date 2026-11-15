import ctypes

from ._native import check, lib, to_rgba

RECT_STRIDE = 8
CIRCLE_STRIDE = 7

_BATCH_CAPACITY = 4096
_SIZE_SCRATCH = (ctypes.c_float * 2)()

_rect_buffer = (ctypes.c_float * (RECT_STRIDE * _BATCH_CAPACITY))()
_circle_buffer = (ctypes.c_float * (CIRCLE_STRIDE * _BATCH_CAPACITY))()


class Surface:
    """The window drawing target; it also carries the current window size."""

    __slots__ = ("width", "height")

    def __init__(self, width=0, height=0):
        self.width = width
        self.height = height

    @property
    def rect(self):
        return (0.0, 0.0, self.width, self.height)

    def clear(self, color=(0, 0, 0)):
        red, green, blue, alpha = to_rgba(color)
        lib.soshi_clear(int(red), int(green), int(blue), int(alpha))


_surface = Surface()


class _Display:
    __slots__ = ()

    def get_surface(self):
        check(lib.soshi_display_size(_SIZE_SCRATCH))
        _surface.width = _SIZE_SCRATCH[0]
        _surface.height = _SIZE_SCRATCH[1]
        return _surface

    def flip(self):
        lib.soshi_present()

    def set_vsync(self, enabled=True):
        lib.soshi_set_vsync(1 if enabled else 0)


class _Draw:
    __slots__ = ()

    def rect(self, surface, rect, color):
        self.rects(surface, [(rect[0], rect[1], rect[2], rect[3], color)])

    def rects(self, surface, items):
        base = 0
        buffered = 0
        for x, y, width, height, color in items:
            red, green, blue, alpha = to_rgba(color)
            _rect_buffer[base + 0] = x
            _rect_buffer[base + 1] = y
            _rect_buffer[base + 2] = width
            _rect_buffer[base + 3] = height
            _rect_buffer[base + 4] = red
            _rect_buffer[base + 5] = green
            _rect_buffer[base + 6] = blue
            _rect_buffer[base + 7] = alpha
            base += RECT_STRIDE
            buffered += 1
            if buffered == _BATCH_CAPACITY:
                lib.soshi_draw_rects(_rect_buffer, buffered)
                base = 0
                buffered = 0
        if buffered:
            lib.soshi_draw_rects(_rect_buffer, buffered)

    def circle(self, surface, center, radius, color):
        self.circles(surface, [(center[0], center[1], radius, color)])

    def circles(self, surface, items):
        base = 0
        buffered = 0
        for x, y, radius, color in items:
            red, green, blue, alpha = to_rgba(color)
            _circle_buffer[base + 0] = x
            _circle_buffer[base + 1] = y
            _circle_buffer[base + 2] = radius
            _circle_buffer[base + 3] = red
            _circle_buffer[base + 4] = green
            _circle_buffer[base + 5] = blue
            _circle_buffer[base + 6] = alpha
            base += CIRCLE_STRIDE
            buffered += 1
            if buffered == _BATCH_CAPACITY:
                lib.soshi_draw_circles(_circle_buffer, buffered)
                base = 0
                buffered = 0
        if buffered:
            lib.soshi_draw_circles(_circle_buffer, buffered)

    def world(self):
        """Draw every entity in one native call."""
        lib.soshi_draw_world()


display = _Display()
draw = _Draw()
