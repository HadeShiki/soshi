"""Generate the example assets (a 4-frame sprite sheet and a short WAV) with no dependencies."""

import math
import pathlib
import struct
import wave
import zlib

ASSETS = pathlib.Path(__file__).resolve().parent.parent / "examples" / "assets"

FRAME = 32
FRAMES = 4
COLORS = [(90, 210, 255), (120, 160, 255), (180, 120, 255), (255, 220, 160)]


def write_png(path, width, height, rows):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    raw = b"".join(b"\x00" + bytes(row) for row in rows)
    payload = b"\x89PNG\r\n\x1a\n"
    payload += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    payload += chunk(b"IDAT", zlib.compress(raw, 9))
    payload += chunk(b"IEND", b"")
    path.write_bytes(payload)


def frame_rows(frame_index):
    red, green, blue = COLORS[frame_index]
    radius = FRAME * (0.24 + 0.05 * frame_index)
    center = FRAME / 2
    rows = []
    for y in range(FRAME):
        row = bytearray()
        for x in range(FRAME):
            distance = math.hypot(x + 0.5 - center, y + 0.5 - center)
            if distance > radius:
                row += bytes((0, 0, 0, 0))
                continue
            highlight = max(0.0, 1.0 - math.hypot(x + 0.5 - center + radius * 0.3, y + 0.5 - center + radius * 0.3) / radius)
            shade = 0.75 + 0.25 * highlight
            row += bytes((int(red * shade), int(green * shade), int(blue * shade), 255))
        rows.append(row)
    return rows


def build_sheet():
    width = FRAME * FRAMES
    rows = [bytearray() for _ in range(FRAME)]
    for frame in range(FRAMES):
        for y, frame_row in enumerate(frame_rows(frame)):
            rows[y] += frame_row
    path = ASSETS / "sheet.png"
    write_png(path, width, FRAME, rows)
    print(f"wrote {path} ({width}x{FRAME})")


def build_blip():
    sample_rate = 44100
    duration = 0.09
    samples = []
    total = int(sample_rate * duration)
    for index in range(total):
        seconds = index / sample_rate
        envelope = 1.0 - (index / total)
        value = math.sin(2 * math.pi * 880 * seconds) * envelope * 0.35
        samples.append(int(value * 32767))

    path = ASSETS / "blip.wav"
    with wave.open(str(path), "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(2)
        handle.setframerate(sample_rate)
        handle.writeframes(struct.pack(f"<{len(samples)}h", *samples))
    print(f"wrote {path} ({duration:.2f}s)")


if __name__ == "__main__":
    ASSETS.mkdir(parents=True, exist_ok=True)
    build_sheet()
    build_blip()
