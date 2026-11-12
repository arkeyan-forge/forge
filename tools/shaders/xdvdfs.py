"""List (and optionally extract) files from an Xbox 360 XDVDFS ISO. Read-only on the ISO."""
import os
import struct
import sys

SECTOR = 2048
MAGIC = b"MICROSOFT*XBOX*MEDIA"
PARTITION_OFFSETS = [0x0, 0x2080000, 0xFD90000, 0x18300000]


def find_partition(f):
    for base in PARTITION_OFFSETS:
        f.seek(base + 32 * SECTOR)
        if f.read(20) == MAGIC:
            return base
    raise SystemExit("XDVDFS magic not found")


def walk(f, base, sector, size, prefix, out):
    f.seek(base + sector * SECTOR)
    data = f.read(size)
    stack = [0]
    seen = set()
    while stack:
        off = stack.pop()
        if off in seen or off + 14 > len(data):
            continue
        seen.add(off)
        left, right, start, length, attr, nlen = struct.unpack_from("<HHIIBB", data, off)
        if left == 0xFFFF and right == 0xFFFF:
            continue
        name = data[off + 14: off + 14 + nlen].decode("latin-1")
        path = f"{prefix}/{name}"
        if left:
            stack.append(left * 4)
        if right:
            stack.append(right * 4)
        if attr & 0x10:
            out.append((path, None, start))
            if length:
                walk(f, base, start, length, path, out)
        else:
            out.append((path, length, start))


def main():
    iso = sys.argv[1]
    extract = sys.argv[2:]  # optional: dest_dir followed by paths to extract
    with open(iso, "rb") as f:
        base = find_partition(f)
        f.seek(base + 32 * SECTOR + 20)
        root_sector, root_size = struct.unpack("<II", f.read(8))
        entries = []
        walk(f, base, root_sector, root_size, "", entries)
        if not extract:
            print(f"partition base: {hex(base)}")
            for path, length, start in sorted(entries):
                print(f"{'<DIR>' if length is None else length:>12}  {path}")
            return
        dest, wanted = extract[0], set(extract[1:])
        for path, length, start in entries:
            if length is None or path not in wanted:
                continue
            target = os.path.join(dest, path.lstrip("/"))
            os.makedirs(os.path.dirname(target), exist_ok=True)
            f.seek(base + start * SECTOR)
            with open(target, "wb") as o:
                remaining = length
                while remaining:
                    chunk = f.read(min(remaining, 1 << 20))
                    o.write(chunk)
                    remaining -= len(chunk)
            print(f"extracted {path} -> {target}")


if __name__ == "__main__":
    main()
