"""Merges pipeline lists (pipelines.bin, the baseline) into one, in the order their records first appear.

    python3 -I tools/pipelines/merge_lists.py <out.bin> <list> [<list>...]

A list is pipelines.bin's format (runtime/gpu/draw.cpp): a 12-byte header (magic "SSAP", version, record size) and
records of a PipelineKey (render states, the shaders by hash) and its XXH3; `.zst` lists (the baseline) are read
through the zstd command. Lists of another version or record size are refused. Then compress the result:
zstd -19 --rm <out.bin> -o games/<game>/pipelines/baseline.zst
"""
import struct
import subprocess
import sys


def read_list(path):
    if path.endswith(".zst"):
        return subprocess.run(["zstd", "-dc", path], check=True, capture_output=True).stdout
    with open(path, "rb") as f:
        return f.read()


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    header = None
    records = {}
    for path in sys.argv[2:]:
        data = read_list(path)
        if len(data) < 12:
            print(f"{path}: empty, skipped")
            continue
        if header is None:
            header = data[:12]
        magic, version, size = struct.unpack("<III", data[:12])
        if data[:12] != header:
            sys.exit(f"{path}: version {version} record size {size}, not the first list's")
        before = len(records)
        for offset in range(12, len(data) - size + 1, size):
            records.setdefault(data[offset:offset + size], None)
        print(f"{path}: {(len(data) - 12) // size} records, {len(records) - before} new")
    with open(sys.argv[1], "wb") as f:
        f.write(header)
        f.writelines(records)
    print(f"{sys.argv[1]}: {len(records)} records")


main()
