"""WAV playback: one device, a fixed pool of voices mixed by the native callback."""

from ._native import SoshiError, check, last_error, lib

_SAMPLE_RATE = lib.soshi_audio_sample_rate()


class Voice:
    """One playing instance of a sound.

    Voices are independent: several can play the same sound at once, each with its
    own volume, pan, pitch and loop count.
    """

    __slots__ = ("handle",)

    def __init__(self, handle):
        self.handle = handle

    def stop(self, fade=0.05):
        """Stop now, or fade out over ``fade`` seconds."""
        check(lib.soshi_voice_stop(self.handle, float(fade)))

    def set_volume(self, volume, fade=0.0):
        check(lib.soshi_voice_set_volume(self.handle, float(volume), float(fade)))

    def set_pan(self, pan):
        """-1 is hard left, 0 center, 1 hard right."""
        check(lib.soshi_voice_set_pan(self.handle, float(pan)))

    def set_pitch(self, pitch):
        """1.0 is the original speed; 2.0 is twice as fast and an octave up."""
        check(lib.soshi_voice_set_pitch(self.handle, float(pitch)))

    @property
    def playing(self):
        return bool(lib.soshi_voice_playing(self.handle))

    @property
    def progress(self):
        """How far through the current pass, 0.0 to 1.0."""
        return float(lib.soshi_voice_progress(self.handle))


class Sound:
    """A decoded WAV. Loading happens once; playing costs a voice from the pool."""

    __slots__ = ("handle",)

    def __init__(self, path):
        self.handle = lib.soshi_sound_load(str(path).encode("utf-8"))
        if self.handle == 0:
            raise SoshiError(f"cannot load sound {path}: {last_error()}")

    @property
    def seconds(self):
        return lib.soshi_sound_frames(self.handle) / _SAMPLE_RATE

    def play(self, volume=1.0, pan=0.0, pitch=1.0, loops=0, fade_in=0.0):
        """Start a voice and return it.

        ``loops`` counts repeats: 0 plays once, -1 loops forever.
        """
        handle = lib.soshi_sound_play(
            self.handle, float(volume), float(pan), float(pitch), int(loops), float(fade_in)
        )
        if handle == 0:
            raise SoshiError(last_error())
        return Voice(handle)

    def destroy(self):
        lib.soshi_sound_destroy(self.handle)
        self.handle = 0

    def __enter__(self):
        return self

    def __exit__(self, *_exception):
        self.destroy()


class _Audio:
    """Master volume, pause, and the shared voice pool."""

    __slots__ = ()

    def available(self):
        return bool(lib.soshi_audio_available())

    def set_volume(self, volume):
        lib.soshi_audio_set_volume(float(volume))

    def stop_all(self, fade=0.05):
        lib.soshi_audio_stop_all(float(fade))

    def pause(self, paused=True):
        lib.soshi_audio_pause(1 if paused else 0)

    @property
    def playing(self):
        """How many voices are active right now."""
        return int(lib.soshi_audio_voice_count())

    @property
    def capacity(self):
        """How many voices can play at once."""
        return int(lib.soshi_audio_voice_capacity())


audio = _Audio()
