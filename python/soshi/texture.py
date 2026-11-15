"""Images and sprite sheets."""

import ctypes

from ._native import SoshiError, check, last_error, lib

_SIZE_SCRATCH = (ctypes.c_float * 2)()


class Texture:
    """A decoded image on the renderer. Load once, reuse for every sprite."""

    __slots__ = ("handle", "width", "height")

    def __init__(self, path):
        self.handle = lib.soshi_texture_load(str(path).encode("utf-8"))
        if self.handle == 0:
            raise SoshiError(f"cannot load texture {path}: {last_error()}")
        check(lib.soshi_texture_size(self.handle, _SIZE_SCRATCH))
        self.width = _SIZE_SCRATCH[0]
        self.height = _SIZE_SCRATCH[1]

    def destroy(self):
        lib.soshi_texture_destroy(self.handle)
        self.handle = 0

    def __enter__(self):
        return self

    def __exit__(self, *_exception):
        self.destroy()
