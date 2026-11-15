from ._native import check, lib


def init(width=800, height=600, title="Soshi"):
    """Open the window and reset the engine world."""
    check(lib.soshi_init(int(width), int(height), str(title).encode("utf-8")))


def quit():
    """Close the window and release SDL."""
    lib.soshi_quit()


def is_running():
    return bool(lib.soshi_is_running())


class Clock:
    """Frame limiter. ``tick()`` sleeps as needed and returns the delta in seconds."""

    __slots__ = ("fps",)

    def __init__(self, fps=60):
        self.fps = float(fps)

    def tick(self):
        return float(lib.soshi_clock_tick(self.fps))
