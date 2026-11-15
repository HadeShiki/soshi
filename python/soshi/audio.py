"""WAV playback through one shared output stream."""

from ._native import SoshiError, check, last_error, lib


class Sound:
    """A decoded WAV ready to play. Playback queues on the shared stream."""

    __slots__ = ("handle",)

    def __init__(self, path):
        self.handle = lib.soshi_sound_load(str(path).encode("utf-8"))
        if self.handle == 0:
            raise SoshiError(f"cannot load sound {path}: {last_error()}")

    def play(self):
        check(lib.soshi_sound_play(self.handle))

    def destroy(self):
        lib.soshi_sound_destroy(self.handle)
        self.handle = 0

    def __enter__(self):
        return self

    def __exit__(self, *_exception):
        self.destroy()


class _Audio:
    __slots__ = ()

    def available(self):
        return bool(lib.soshi_audio_available())

    def set_volume(self, volume):
        lib.soshi_audio_set_volume(float(volume))


audio = _Audio()
