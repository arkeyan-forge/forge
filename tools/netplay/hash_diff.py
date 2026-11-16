#!/usr/bin/env python3
"""Compare probe totals and optional SKYBLK01 sidecars; no guest data is read."""

import argparse
import io
import os
import struct
import tempfile
from pathlib import Path

HEAPS = {1: "virtual4K", 2: "virtual64K", 3: "physical", 4: "image"}
MAX_BLOCKS = 1 << 20


def totals(path):
    previous = -1
    with open(path, encoding="ascii") as file:
        for line in file:
            fields = line.split()
            if len(fields) != 2:
                raise ValueError("invalid totals line")
            frame, value = int(fields[0], 10), int(fields[1], 16)
            if frame <= previous or frame < 0 or not 0 <= value < 1 << 64:
                raise ValueError("invalid frame order or hash")
            previous = frame
            yield frame, value


def read_exact(file, count):
    data = file.read(count)
    if len(data) != count:
        raise ValueError("truncated block sidecar")
    return data


def block_frames(file):
    if read_exact(file, 12) != b"SKYBLK01" + struct.pack("<I", 1):
        raise ValueError("invalid block magic or version")
    previous = -1
    while True:
        header = file.read(12)
        if not header:
            return
        if len(header) != 12:
            raise ValueError("truncated block frame header")
        frame, count = struct.unpack("<QI", header)
        if count > MAX_BLOCKS or frame <= previous:
            raise ValueError("oversize block count or invalid frame order")
        previous = frame
        blocks = {}
        for _ in range(count):
            address, heap, value = struct.unpack("<IIQ", read_exact(file, 16))
            key = (address, heap)
            if heap not in HEAPS or key in blocks:
                raise ValueError("unknown heap or duplicate block")
            blocks[key] = value
        yield frame, blocks


def first_difference(a, b):
    left, right = iter(totals(a)), iter(totals(b))
    while True:
        x, y = next(left, None), next(right, None)
        if x is None and y is None:
            return None
        if x != y:
            return min(v[0] for v in (x, y) if v is not None), x, y


def blocks_at(path, frame):
    with open(path, "rb") as file:
        for current, blocks in block_frames(file):
            if current == frame:
                return blocks
            if current > frame:
                break
    return {}


def compare(a, b, output=print):
    difference = first_difference(a, b)
    if difference is None:
        output("All frame totals match")
        return False
    frame, left, right = difference
    output(f"First differing frame: {frame}")
    for label, item in (("a", left), ("b", right)):
        value = f"{item[1]:016x}" if item and item[0] == frame else "missing frame"
        output(f"  {label}: {value}")
    if os.path.isfile(str(a) + ".blocks") and os.path.isfile(str(b) + ".blocks"):
        x = blocks_at(str(a) + ".blocks", frame)
        y = blocks_at(str(b) + ".blocks", frame)
        differing = sorted(key for key in x.keys() | y.keys() if x.get(key) != y.get(key))
        for address, heap in differing[:16]:
            key = address, heap
            av = f"{x[key]:016x}" if key in x else "missing"
            bv = f"{y[key]:016x}" if key in y else "missing"
            output(f"  0x{address:08x} {HEAPS[heap]}: {av} / {bv}")
        if not differing:
            output("  No differing blocks in the available sidecar frames")
        elif len(differing) > 16:
            output(f"  {len(differing) - 16} further differing blocks")
    return True


def self_test():
    scratch = Path(".scratch")
    scratch.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="hash-diff-", dir=scratch) as folder:
        a, b = Path(folder) / "a", Path(folder) / "b"
        a.write_text("1 0001\n2 0002\n", encoding="ascii")
        b.write_text("1 0001\n2 0003\n", encoding="ascii")
        for path, value in ((a, 2), (b, 3)):
            with open(str(path) + ".blocks", "wb") as file:
                file.write(b"SKYBLK01" + struct.pack("<I", 1))
                file.write(struct.pack("<QI", 1, 0))
                file.write(struct.pack("<QI", 2, 1))
                file.write(struct.pack("<IIQ", 0xA0010000, 3, value))
        output = []
        assert compare(a, b, output.append)
        assert "First differing frame: 2" in output
        assert any("0xa0010000 physical" in line for line in output)
        b.write_text(a.read_text(encoding="ascii"), encoding="ascii")
        assert not compare(a, b, lambda _: None)
        b.write_text("1 0001\n", encoding="ascii")
        assert first_difference(a, b)[0] == 2
        b.write_text("2 0002\n", encoding="ascii")
        assert first_difference(a, b)[0] == 1
        for data in (b"", b"wrongmagic!!", b"SKYBLK01" + struct.pack("<I", 2),
                     b"SKYBLK01" + struct.pack("<I", 1) + b"x",
                     b"SKYBLK01" + struct.pack("<IQI", 1, 1, MAX_BLOCKS + 1),
                     b"SKYBLK01" + struct.pack("<IQI", 1, 1, 1)):
            try:
                list(block_frames(io.BytesIO(data)))
            except ValueError:
                pass
            else:
                raise AssertionError("malformed sidecar accepted")
    print("hash_diff: self-test passed")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("a", nargs="?")
    parser.add_argument("b", nargs="?")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    if not args.a or not args.b:
        parser.error("provide two totals files, or --self-test")
    try:
        return 1 if compare(args.a, args.b) else 0
    except (OSError, UnicodeError, ValueError) as error:
        print(f"hash_diff: refused: {error}")
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
