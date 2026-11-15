import ctypes

from ._native import check, lib

_mouse_scratch = (ctypes.c_float * 2)()


class _Event:
    __slots__ = ()

    def pump(self):
        """Drain the OS event queue into the engine's input state."""
        lib.soshi_pump_events()

    def key_pressed(self, keycode):
        return bool(lib.soshi_key_pressed(int(keycode)))

    def key_released(self, keycode):
        return bool(lib.soshi_key_released(int(keycode)))


event = _Event()


def mouse_pos():
    check(lib.soshi_mouse_position(_mouse_scratch))
    return (_mouse_scratch[0], _mouse_scratch[1])
