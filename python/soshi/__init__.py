"""Soshi: a small, fast 2D engine with a pygame-shaped API."""

from . import entity, input, render, window
from ._native import SoshiError
from .audio import Sound, Voice, audio
from .entity import Entity, world
from .font import Font
from .input import event, mouse_pos
from .render import display, draw
from .texture import Texture
from .version import __version__
from .window import Clock, init, quit

K_ESCAPE = 27
K_RETURN = 13
K_SPACE = 32
K_LEFT = 1073741904
K_RIGHT = 1073741903
K_UP = 1073741906
K_DOWN = 1073741905

__all__ = [
    "Clock",
    "Entity",
    "Font",
    "K_DOWN",
    "K_ESCAPE",
    "K_LEFT",
    "K_RETURN",
    "K_RIGHT",
    "K_SPACE",
    "K_UP",
    "Sound",
    "SoshiError",
    "Texture",
    "Voice",
    "audio",
    "display",
    "draw",
    "event",
    "init",
    "mouse_pos",
    "quit",
    "world",
    "__version__",
]


def __getattr__(name):
    if name == "running":
        return window.is_running()
    raise AttributeError(f"module 'soshi' has no attribute {name!r}")
