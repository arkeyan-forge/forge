#!/usr/bin/env python3
"""Compare probe totals, SKYBLK01 sidecars and event logs of two replays; no guest data is read."""

import argparse
import io
import os
import struct
import tempfile
from pathlib import Path

HEAPS = {1: "virtual4K", 2: "virtual64K", 3: "physical", 4: "image"}
MAX_BLOCKS = 1 << 20
COUNTERS = ("vblank", "gfxint", "audio", "timebase")
SUMMARY_LINES = 40


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


def summary(a, b, output=print):
    """Differing blocks per heap in every frame both sidecars hold: whether a divergence grows or stays put."""
    rows = []
    with open(str(a) + ".blocks", "rb") as left, open(str(b) + ".blocks", "rb") as right:
        for (frame, x), (other, y) in zip(block_frames(left), block_frames(right)):
            if frame != other:
                raise ValueError("block sidecars hold different frames")
            per_heap = {heap: 0 for heap in HEAPS}
            for key in x.keys() | y.keys():
                if x.get(key) != y.get(key):
                    per_heap[key[1]] += 1
            rows.append((frame, len(x), per_heap))
    if not rows:
        output("No common sidecar frames")
        return False
    step = max(1, len(rows) // SUMMARY_LINES)
    shown = rows[::step] + ([rows[-1]] if (len(rows) - 1) % step else [])
    for frame, count, per_heap in shown:
        heaps = ", ".join(f"{HEAPS[h]} {n}" for h, n in per_heap.items() if n)
        output(f"frame {frame}: {sum(per_heap.values())} of {count} blocks differ" + (f" ({heaps})" if heaps else ""))
    worst = max(rows, key=lambda row: sum(row[2].values()))
    output(f"{len(rows)} frames; most at frame {worst[0]}: {sum(worst[2].values())}")
    return any(sum(row[2].values()) for row in rows)


def events(path):
    """Per-frame counters and the notes in order, from a SKY_NETPLAY_EVENTS log."""
    counts, notes = {}, []
    with open(path, encoding="utf-8", errors="replace") as file:
        for line in file:
            frame, _, rest = line.rstrip("\n").partition(" ")
            kind, _, detail = rest.partition(" ")
            if not frame.isdigit() or not kind:
                raise ValueError("invalid event line")
            if kind == "counts":
                fields = dict(item.split("=", 1) for item in detail.split())
                if set(fields) != set(COUNTERS) or not all(v.isdigit() for v in fields.values()):
                    raise ValueError("invalid counts line")
                counts[int(frame)] = {name: int(fields[name]) for name in COUNTERS}
            else:
                notes.append((int(frame), kind, detail))
    return counts, notes


def compare_events(a, b, output=print):
    (x_counts, x_notes), (y_counts, y_notes) = events(a), events(b)
    differ = False
    x_seq, y_seq = [n[1:] for n in x_notes], [n[1:] for n in y_notes]
    if x_seq != y_seq:
        differ = True
        index = next((i for i, (p, q) in enumerate(zip(x_seq, y_seq)) if p != q), min(len(x_seq), len(y_seq)))
        output(f"Notes differ in order from note {index} ({len(x_seq)} / {len(y_seq)} notes)")
        for label, notes in (("a", x_notes), ("b", y_notes)):
            item = notes[index] if index < len(notes) else None
            output(f"  {label}: " + (f"frame {item[0]} {item[1]} {item[2]}" if item else "none"))
    else:
        shifted = [(p, q) for p, q in zip(x_notes, y_notes) if p[0] != q[0]]
        output(f"Notes: same {len(x_seq)} in the same order; {len(shifted)} at another frame")
        if shifted:
            differ = True
            p, q = shifted[0]
            worst = max(abs(p[0] - q[0]) for p, q in shifted)
            output(f"  first: {p[1]} {p[2]} at frame {p[0]} / {q[0]}; largest shift {worst} frames")
    frames = sorted(x_counts.keys() & y_counts.keys())
    for name in COUNTERS:
        unequal = [f for f in frames if x_counts[f][name] != y_counts[f][name]]
        total_x = sum(x_counts[f][name] for f in frames)
        total_y = sum(y_counts[f][name] for f in frames)
        line = f"{name}: {total_x} / {total_y} over {len(frames)} frames, {len(unequal)} frames differ"
        if unequal:
            differ = True
            f = unequal[0]
            line += f" (first frame {f}: {x_counts[f][name]} / {y_counts[f][name]})"
        output(line)
    return differ


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
        with open(str(a) + ".blocks", "wb") as file:
            file.write(b"SKYBLK01" + struct.pack("<I", 1))
            file.write(struct.pack("<QI", 30, 2) + struct.pack("<IIQ", 0x10000, 2, 1) + struct.pack("<IIQ", 0x20000, 4, 1))
        with open(str(b) + ".blocks", "wb") as file:
            file.write(b"SKYBLK01" + struct.pack("<I", 1))
            file.write(struct.pack("<QI", 30, 2) + struct.pack("<IIQ", 0x10000, 2, 1) + struct.pack("<IIQ", 0x20000, 4, 2))
        output = []
        assert summary(a, b, output.append)
        assert "frame 30: 1 of 2 blocks differ (image 1)" in output
        ea, eb = Path(folder) / "a.events", Path(folder) / "b.events"
        ea.write_text("0 thread 0x82000000\n0 counts vblank=2 gfxint=0 audio=3 timebase=9\n"
                      "1 open game:\\a b\n1 counts vblank=1 gfxint=1 audio=3 timebase=4\n", encoding="utf-8")
        eb.write_text(ea.read_text(encoding="utf-8"), encoding="utf-8")
        assert not compare_events(ea, eb, lambda _: None)
        eb.write_text("0 thread 0x82000000\n0 counts vblank=3 gfxint=0 audio=3 timebase=9\n"
                      "1 counts vblank=1 gfxint=1 audio=3 timebase=4\n2 open game:\\a b\n", encoding="utf-8")
        output = []
        assert compare_events(ea, eb, output.append)
        assert any("1 at another frame" in line for line in output)
        assert any(line.startswith("vblank: 3 / 4 over 2 frames, 1 frames differ (first frame 0: 2 / 3)") for line in output)
        eb.write_text("0 open game:\\other\n", encoding="utf-8")
        output = []
        assert compare_events(ea, eb, output.append)
        assert "Notes differ in order from note 0 (2 / 1 notes)" in output
        for text in ("x counts\n", "1 counts vblank=1\n", "1\n"):
            eb.write_text(text, encoding="utf-8")
            try:
                events(eb)
            except ValueError:
                pass
            else:
                raise AssertionError("malformed event log accepted")
    print("hash_diff: self-test passed")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("a", nargs="?")
    parser.add_argument("b", nargs="?")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--summary", action="store_true", help="differing blocks per heap in every sidecar frame")
    parser.add_argument("--events", action="store_true", help="compare two SKY_NETPLAY_EVENTS logs")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    if not args.a or not args.b:
        parser.error("provide two totals files (or event logs with --events), or --self-test")
    try:
        if args.events:
            return 1 if compare_events(args.a, args.b) else 0
        if args.summary:
            return 1 if summary(args.a, args.b) else 0
        return 1 if compare(args.a, args.b) else 0
    except (OSError, UnicodeError, ValueError) as error:
        print(f"hash_diff: refused: {error}")
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
