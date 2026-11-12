#!/usr/bin/env python3
"""Writes the placeholder package images in tools/xbox/assets/ (standard library only).

The manifest references these exact file names. Without resources.pri there is no scale-qualified lookup
(Square150x150Logo.scale-200.png etc.), so each image exists once at its base size.

    python3 tools/xbox/make_placeholder_logos.py [output-dir]
"""
import os
import struct
import sys
import zlib

SIZES = {
    "StoreLogo.png": (50, 50),
    "Square44x44Logo.png": (44, 44),
    "Square150x150Logo.png": (150, 150),
    "Wide310x150Logo.png": (310, 150),
    "SplashScreen.png": (620, 300),
}

BACKGROUND = (0x1B, 0x2A, 0x4A)
ACCENT = (0x7C, 0x4D, 0xD8)
TEXT = (0xFF, 0xD2, 0x3F)

# 5x7 glyphs, one string per row.
GLYPHS = {
    "S": ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
}


def render(width, height, text="SSA"):
    pixels = [[BACKGROUND] * width for _ in range(height)]
    # Accent band across the middle third.
    for y in range(height // 3, 2 * height // 3):
        pixels[y] = [ACCENT] * width
    # Text: glyph cells of 6 columns (5 + 1 spacing), scaled to fit 70% of the width and 50% of the height.
    columns = len(text) * 6 - 1
    scale = max(1, min(int(width * 0.7) // columns, int(height * 0.5) // 7))
    x0 = (width - columns * scale) // 2
    y0 = (height - 7 * scale) // 2
    for index, char in enumerate(text):
        for row, bits in enumerate(GLYPHS[char]):
            for col, bit in enumerate(bits):
                if bit != "1":
                    continue
                for dy in range(scale):
                    for dx in range(scale):
                        pixels[y0 + row * scale + dy][x0 + (index * 6 + col) * scale + dx] = TEXT
    return pixels


def write_png(path, pixels):
    height, width = len(pixels), len(pixels[0])
    raw = b"".join(b"\x00" + bytes(c for px in row for c in px) for row in pixels)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "assets")
    os.makedirs(out, exist_ok=True)
    for name, (width, height) in SIZES.items():
        write_png(os.path.join(out, name), render(width, height))
        print(f"{name}: {width}x{height}")


if __name__ == "__main__":
    main()
