#!/usr/bin/env python3
"""Compare host-sourced load traces or their per-frame summaries."""

import argparse
import io
import struct
from collections import deque
from itertools import zip_longest

HEADER = b"SKYTRC01" + struct.pack("<I", 1)
RECORD = struct.Struct("<QIIBQQI")


def records(file):
    if file.read(len(HEADER)) != HEADER:
        raise ValueError("invalid trace magic or version")
    previous, previous_frame = 0, 0
    while data := file.read(RECORD.size):
        if len(data) != RECORD.size:
            raise ValueError("truncated trace record")
        row = RECORD.unpack(data)
        sequence, frame, address, size, value, before, _ = row
        if (sequence <= previous or frame < previous_frame or size not in (1, 2, 4, 8)
                or address + size > 1 << 32 or value >= 1 << (8 * size) or before >= 1 << (8 * size)):
            raise ValueError("invalid trace record or order")
        previous, previous_frame = sequence, frame
        yield row


def frames(file):
    previous = -1
    for line in file:
        fields = line.split()
        if len(fields) != 4 or fields[0] != "frame":
            raise ValueError("invalid trace frame line")
        frame, count, chain = int(fields[1], 10), int(fields[2], 10), int(fields[3], 16)
        if not previous < frame < 1 << 32 or not 0 <= count < 1 << 64 or not 0 <= chain < 1 << 64:
            raise ValueError("invalid trace frame order, count or hash")
        previous = frame
        yield frame, count, chain


def describe(row):
    sequence, frame, address, size, value, before, lr = row
    return (f"sequence={sequence} frame={frame} address=0x{address:08x} size={size} "
            f"value=0x{value:0{size * 2}x} shadow=0x{before:0{size * 2}x} lr=0x{lr:08x}")


def compare_records(left, right, output=print):
    history = deque(maxlen=10)
    for index, (a, b) in enumerate(zip_longest(records(left), records(right)), 1):
        if a != b:
            output(f"First differing record: {index}")
            for label, row in (("a", a), ("b", b)):
                output(f"{label}: " + (describe(row) if row is not None else "end of trace"))
            for label, column in (("a", 0), ("b", 1)):
                output(f"{label}: preceding {len(history)} records:")
                for pair in history:
                    output("  " + describe(pair[column]))
            return True
        history.append((a, b))
    output("No differing records")
    return False


def compare_frames(left, right, output=print):
    for a, b in zip_longest(frames(left), frames(right)):
        if a != b:
            frame = min(row[0] for row in (a, b) if row is not None)
            output(f"First differing frame: {frame}")
            for label, row in (("a", a), ("b", b)):
                output(f"{label}: " + (f"frame {row[0]} count={row[1]} hash={row[2]:016x}" if row else "end of frames"))
            return True
    output("No differing frames")
    return False


def self_test():
    rows = [(i + 1, 7, 0x1000 + i * 4, 4, i, 0, 0x12345678) for i in range(15)]
    a = HEADER + b"".join(RECORD.pack(*row) for row in rows)
    changed = list(rows)
    changed[12] = (*rows[12][:4], 99, *rows[12][5:])
    b = HEADER + b"".join(RECORD.pack(*row) for row in changed)
    output = []
    assert compare_records(io.BytesIO(a), io.BytesIO(b), output.append)
    assert "First differing record: 13" in output
    assert any("address=0x00001030" in line and "lr=0x12345678" in line for line in output)
    assert sum("sequence=" in line for line in output) == 22
    assert not compare_records(io.BytesIO(a), io.BytesIO(a), lambda _: None)
    assert compare_records(io.BytesIO(a), io.BytesIO(a[:-RECORD.size]), lambda _: None)
    assert compare_records(io.BytesIO(HEADER), io.BytesIO(a), lambda _: None)
    for data in (b"", b"wrongmagic!!", HEADER[:8] + struct.pack("<I", 2), HEADER + b"x",
                 HEADER + RECORD.pack(1, 0, 0, 3, 0, 0, 0),
                 HEADER + RECORD.pack(1, 0, 0, 1, 256, 0, 0),
                 HEADER + RECORD.pack(1, 0, 0, 1, 0, 0, 0) * 2,
                 HEADER + RECORD.pack(0, 0, 0, 1, 0, 0, 0),
                 HEADER + RECORD.pack(1, 0, 0xFFFFFFFF, 2, 0, 0, 0)):
        try:
            list(records(io.BytesIO(data)))
        except ValueError:
            pass
        else:
            raise AssertionError("malformed record stream accepted")
    for left, right, frame in (("frame 0 1 01\nframe 1 2 02\n", "frame 0 1 01\nframe 1 2 03\n", 1),
                               ("frame 0 1 01\n", "frame 0 2 01\n", 0),
                               ("frame 0 1 01\nframe 1 0 01\n", "frame 0 1 01\n", 1),
                               ("frame 1 1 01\n", "frame 0 1 01\n", 0)):
        output = []
        assert compare_frames(io.StringIO(left), io.StringIO(right), output.append)
        assert f"First differing frame: {frame}" in output
    assert not compare_frames(io.StringIO("frame 0 0 01\n"), io.StringIO("frame 0 0 01\n"), lambda _: None)
    for text in ("0 1 02\n", "frame 0 -1 02\n", "frame 0 1 xyz\n", "frame 0 1 10000000000000000\n",
                 "frame 0 1 01\nframe 0 1 01\n", "frame -1 0 01\n", "frame 4294967296 0 01\n"):
        try:
            list(frames(io.StringIO(text)))
        except ValueError:
            pass
        else:
            raise AssertionError("malformed frame stream accepted")
    print("trace_diff: self-test passed")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("a", nargs="?")
    parser.add_argument("b", nargs="?")
    parser.add_argument("--frames", action="store_true", help="compare two <trace>.frames files")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    if not args.a or not args.b:
        parser.error("provide two files, or --self-test")
    try:
        mode = "r" if args.frames else "rb"
        options = {"encoding": "ascii"} if args.frames else {}
        with open(args.a, mode, **options) as left, open(args.b, mode, **options) as right:
            compare = compare_frames if args.frames else compare_records
            return 1 if compare(left, right) else 0
    except (OSError, UnicodeError, ValueError) as error:
        print(f"trace_diff: refused: {error}")
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
