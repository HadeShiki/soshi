"""TrueType / OpenType fonts, rasterized into a texture atlas on demand."""

import ctypes

from ._native import SoshiError, check, last_error, lib

_SIZE_SCRATCH = (ctypes.c_float * 2)()


class Font:
    """A TTF, OTF or TTC file at one pixel size.

    Glyphs are rasterized the first time they are drawn and cached in a texture
    atlas, so creating a font is instant and memory only covers the characters the
    game actually shows.

    Text is laid out glyph by glyph with the font's advances and kerning: scripts
    that need reordering or mark stacking (Thai vowels/tones, Arabic, Indic) draw in
    sequence rather than stacking, which would need a full shaper such as HarfBuzz.
    """

    __slots__ = ("handle", "height", "ascent", "size")

    def __init__(self, path, size=24):
        self.size = float(size)
        self.handle = lib.soshi_font_load(str(path).encode("utf-8"), self.size)
        if self.handle == 0:
            raise SoshiError(f"cannot load font {path}: {last_error()}")
        self.height = float(lib.soshi_font_height(self.handle))
        self.ascent = float(lib.soshi_font_ascent(self.handle))

    def measure(self, text):
        """Return ``(width, height)`` in pixels, including newlines and kerning."""
        check(lib.soshi_font_measure(self.handle, str(text).encode("utf-8"), _SIZE_SCRATCH))
        return (_SIZE_SCRATCH[0], _SIZE_SCRATCH[1])

    def size_of(self, text):
        """pygame-style alias of :meth:`measure`."""
        return self.measure(text)

    def destroy(self):
        lib.soshi_font_destroy(self.handle)
        self.handle = 0

    def __enter__(self):
        return self

    def __exit__(self, *_exception):
        self.destroy()
