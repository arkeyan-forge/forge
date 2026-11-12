"""Draws the neutral placeholder app icon (a generic disc, no game artwork) and builds ArkeyanRuntime.icns.

usage: python3 make_icon.py <output.icns>   (macOS: needs sips and iconutil)
"""
import math
import os
import struct
import subprocess
import sys
import tempfile
import zlib

SIZE = 1024


def coverage(distance):
    # Signed distance in pixels -> antialiased coverage.
    return min(1.0, max(0.0, 0.5 - distance))


def rounded_square(x, y):
    # macOS Big Sur icon grid: 824 px tile centred on the 1024 canvas, 185 px corners.
    half, radius = 412.0, 185.0
    qx = abs(x - 512) - (half - radius)
    qy = abs(y - 512) - (half - radius)
    outside = math.hypot(max(qx, 0.0), max(qy, 0.0))
    return outside + min(max(qx, qy), 0.0) - radius


def mix(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def over(dst, color, alpha):
    r, g, b, a = dst
    out_a = alpha + a * (1 - alpha)
    if out_a == 0:
        return (0.0, 0.0, 0.0, 0.0)
    return tuple((color[i] * alpha + dst[i] * a * (1 - alpha)) / out_a for i in range(3)) + (out_a,)


def pixel(x, y):
    px, py = x + 0.5, y + 0.5
    result = (0.0, 0.0, 0.0, 0.0)

    tile = coverage(rounded_square(px, py))
    if tile > 0:
        result = over(result, mix((44, 52, 120), (24, 120, 140), py / SIZE), tile)

    r = math.hypot(px - 512, py - 512)
    disc = coverage(r - 300) * (1 - coverage(r - 62))
    if disc > 0:
        angle = math.atan2(py - 512, px - 512)
        shade = 0.5 + 0.5 * math.cos(2 * (angle + 0.6))  # soft reflection band across the disc
        color = mix((196, 206, 226), (244, 247, 252), shade)
        if r < 130:
            color = mix(color, (150, 162, 190), 0.6)
        result = over(result, color, disc)

        ring = coverage(abs(r - 130) - 3) * disc
        result = over(result, (120, 132, 165), ring * 0.8)
    return result


def write_png(path, rows):
    raw = b"".join(b"\x00" + row for row in rows)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


def main():
    output = os.path.abspath(sys.argv[1])
    rows = []
    for y in range(SIZE):
        row = bytearray()
        for x in range(SIZE):
            r, g, b, a = pixel(x, y)
            row += bytes((round(r), round(g), round(b), round(a * 255)))
        rows.append(bytes(row))

    with tempfile.TemporaryDirectory() as work:
        master = os.path.join(work, "icon.png")
        write_png(master, rows)
        iconset = os.path.join(work, "ArkeyanRuntime.iconset")
        os.mkdir(iconset)
        for points in (16, 32, 128, 256, 512):
            for scale in (1, 2):
                name = "icon_%dx%d%s.png" % (points, points, "@2x" if scale == 2 else "")
                pixels = str(points * scale)
                subprocess.run(["sips", "-z", pixels, pixels, master, "--out", os.path.join(iconset, name)],
                               check=True, stdout=subprocess.DEVNULL)
        subprocess.run(["iconutil", "-c", "icns", iconset, "-o", output], check=True)


if __name__ == "__main__":
    main()
