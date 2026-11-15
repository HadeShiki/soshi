"""The only module that touches the native library.

Every exported function gets an explicit signature here so calls stay cheap and
type-conversion surprises stay out of the game loop.
"""

import ctypes
import os
import sys

_LIBRARY_NAMES = {
    "win32": "soshi.dll",
    "darwin": "libsoshi.dylib",
}

_FLOAT = ctypes.c_float
_FLOAT_POINTER = ctypes.POINTER(ctypes.c_float)

_PACKAGE_DIR = os.path.dirname(os.path.abspath(__file__))
_dll_directory = None


class SoshiError(RuntimeError):
    """Raised when the native engine reports a failure."""


def _library_path():
    name = _LIBRARY_NAMES.get(sys.platform, "libsoshi.so")
    return os.path.join(_PACKAGE_DIR, name)


def _load():
    global _dll_directory

    path = _library_path()
    if not os.path.exists(path):
        raise ImportError(
            f"soshi native library is missing at {path}; "
            "build it with scripts/build.cmd or install a prebuilt wheel"
        )
    if sys.platform == "win32" and hasattr(os, "add_dll_directory"):
        _dll_directory = os.add_dll_directory(_PACKAGE_DIR)
    return ctypes.CDLL(path)


lib = _load()

lib.soshi_init.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_char_p]
lib.soshi_init.restype = ctypes.c_int
lib.soshi_quit.argtypes = []
lib.soshi_quit.restype = None
lib.soshi_is_running.argtypes = []
lib.soshi_is_running.restype = ctypes.c_int
lib.soshi_pump_events.argtypes = []
lib.soshi_pump_events.restype = None

lib.soshi_clear.argtypes = [ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8]
lib.soshi_clear.restype = None
lib.soshi_present.argtypes = []
lib.soshi_present.restype = None
lib.soshi_draw_rects.argtypes = [_FLOAT_POINTER, ctypes.c_int]
lib.soshi_draw_rects.restype = None
lib.soshi_draw_circles.argtypes = [_FLOAT_POINTER, ctypes.c_int]
lib.soshi_draw_circles.restype = None
lib.soshi_draw_world.argtypes = []
lib.soshi_draw_world.restype = None
lib.soshi_set_vsync.argtypes = [ctypes.c_int]
lib.soshi_set_vsync.restype = None
lib.soshi_display_size.argtypes = [_FLOAT_POINTER]
lib.soshi_display_size.restype = ctypes.c_int

lib.soshi_key_pressed.argtypes = [ctypes.c_int]
lib.soshi_key_pressed.restype = ctypes.c_int
lib.soshi_key_released.argtypes = [ctypes.c_int]
lib.soshi_key_released.restype = ctypes.c_int
lib.soshi_mouse_position.argtypes = [_FLOAT_POINTER]
lib.soshi_mouse_position.restype = ctypes.c_int
lib.soshi_clock_tick.argtypes = [_FLOAT]
lib.soshi_clock_tick.restype = _FLOAT

lib.soshi_world_set_bounds.argtypes = [_FLOAT, _FLOAT, _FLOAT, _FLOAT]
lib.soshi_world_set_bounds.restype = None
lib.soshi_world_set_gravity.argtypes = [_FLOAT]
lib.soshi_world_set_gravity.restype = None
lib.soshi_world_set_restitution.argtypes = [_FLOAT]
lib.soshi_world_set_restitution.restype = None
lib.soshi_world_step.argtypes = [_FLOAT]
lib.soshi_world_step.restype = None
lib.soshi_world_count.argtypes = []
lib.soshi_world_count.restype = ctypes.c_int
lib.soshi_world_capacity.argtypes = []
lib.soshi_world_capacity.restype = ctypes.c_int

lib.soshi_entity_create.argtypes = [_FLOAT] * 9
lib.soshi_entity_create.restype = ctypes.c_uint64
lib.soshi_entity_destroy.argtypes = [ctypes.c_uint64]
lib.soshi_entity_destroy.restype = ctypes.c_int
lib.soshi_entity_position.argtypes = [ctypes.c_uint64, _FLOAT_POINTER]
lib.soshi_entity_position.restype = ctypes.c_int
lib.soshi_entity_set_position.argtypes = [ctypes.c_uint64, _FLOAT, _FLOAT]
lib.soshi_entity_set_position.restype = ctypes.c_int
lib.soshi_entity_set_velocity.argtypes = [ctypes.c_uint64, _FLOAT, _FLOAT]
lib.soshi_entity_set_velocity.restype = ctypes.c_int
lib.soshi_entity_set_color.argtypes = [ctypes.c_uint64, _FLOAT, _FLOAT, _FLOAT, _FLOAT]
lib.soshi_entity_set_color.restype = ctypes.c_int

lib.soshi_entity_set_rotation.argtypes = [ctypes.c_uint64, _FLOAT]
lib.soshi_entity_set_rotation.restype = ctypes.c_int
lib.soshi_entity_set_parent.argtypes = [ctypes.c_uint64, ctypes.c_uint64]
lib.soshi_entity_set_parent.restype = ctypes.c_int
lib.soshi_entity_set_sprite.argtypes = [ctypes.c_uint64, ctypes.c_uint64, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, _FLOAT, ctypes.c_int]
lib.soshi_entity_set_sprite.restype = ctypes.c_int
lib.soshi_entity_set_draw_size.argtypes = [ctypes.c_uint64, _FLOAT, _FLOAT]
lib.soshi_entity_set_draw_size.restype = ctypes.c_int
lib.soshi_entity_set_frame.argtypes = [ctypes.c_uint64, ctypes.c_int]
lib.soshi_entity_set_frame.restype = ctypes.c_int
lib.soshi_entity_play_animation.argtypes = [ctypes.c_uint64]
lib.soshi_entity_play_animation.restype = ctypes.c_int
lib.soshi_entity_stop_animation.argtypes = [ctypes.c_uint64]
lib.soshi_entity_stop_animation.restype = ctypes.c_int

lib.soshi_texture_load.argtypes = [ctypes.c_char_p]
lib.soshi_texture_load.restype = ctypes.c_uint64
lib.soshi_texture_destroy.argtypes = [ctypes.c_uint64]
lib.soshi_texture_destroy.restype = None
lib.soshi_texture_size.argtypes = [ctypes.c_uint64, _FLOAT_POINTER]
lib.soshi_texture_size.restype = ctypes.c_int

lib.soshi_sound_load.argtypes = [ctypes.c_char_p]
lib.soshi_sound_load.restype = ctypes.c_uint64
lib.soshi_sound_destroy.argtypes = [ctypes.c_uint64]
lib.soshi_sound_destroy.restype = None
lib.soshi_sound_play.argtypes = [ctypes.c_uint64]
lib.soshi_sound_play.restype = ctypes.c_int
lib.soshi_audio_set_volume.argtypes = [_FLOAT]
lib.soshi_audio_set_volume.restype = None
lib.soshi_audio_available.argtypes = []
lib.soshi_audio_available.restype = ctypes.c_int

lib.soshi_last_error.argtypes = []
lib.soshi_last_error.restype = ctypes.c_char_p


def last_error():
    message = lib.soshi_last_error()
    return message.decode("utf-8", "replace") if message else "unknown engine error"


def check(result):
    """Turn the C convention (0 = ok) into an exception."""
    if result != 0:
        raise SoshiError(last_error())
    return result


def to_rgba(color):
    """Accept (r, g, b) or (r, g, b, a) with 0-255 components."""
    alpha = color[3] if len(color) > 3 else 255
    return float(color[0]), float(color[1]), float(color[2]), float(alpha)
