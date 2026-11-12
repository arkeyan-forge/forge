#!/usr/bin/env python3
"""Compares two shader_cache.cpp files made by XenosRecomp, entry by entry: the DXIL, SPIR-V and AIR of every shader.

    python3 -I tools/shaders/compare_cache.py a.cpp b.cpp

Exit code 0 when both files hold the same shaders with the same bytes. Plain `cmp` cannot tell: the Metal compiler
records the random name of its temporary source file in every AIR (and a hash over it), so two runs of the same program
never match byte for byte, and a few shaders get an AIR of another size from run to run. The AIR is therefore compared
by size only; the DXIL and SPIR-V byte for byte. A difference is listed per shader and per stream. Needs the `zstd` program (the caches are zstd frames). Class [Xbox].
"""
import re
import subprocess
import sys

STREAMS = ("Dxil", "Spirv", "Air")


def parse(path):
    text = open(path, encoding="utf-8").read()
    entries = {}
    for m in re.finditer(r"^\t\{ 0x([0-9A-F]+), (\d+), (\d+), (\d+), (\d+), (\d+), (\d+), (\d+), \"([^\"]*)\" \},$", text, re.M):
        h, do, ds, so, ss, ao, a_s, mask, name = m.groups()
        entries[h] = {"Dxil": (int(do), int(ds)), "Spirv": (int(so), int(ss)), "Air": (int(ao), int(a_s)), "mask": int(mask), "file": name}
    blobs = {}
    for stream in STREAMS:
        m = re.search(r"g_compressed%sCache\[\] = \{([0-9,]*)\}" % stream, text)
        if not m:
            blobs[stream] = None
            continue
        packed = bytes(int(v) for v in m.group(1).split(",") if v)
        blobs[stream] = subprocess.run(["zstd", "-d", "-c", "-q"], input=packed, stdout=subprocess.PIPE, check=True).stdout
    return entries, blobs


def main():
    a_entries, a_blobs = parse(sys.argv[1])
    b_entries, b_blobs = parse(sys.argv[2])
    bad = 0
    for h in sorted(set(a_entries) ^ set(b_entries)):
        print("only in %s: %s" % (sys.argv[1] if h in a_entries else sys.argv[2], h))
        bad += 1
    for h in sorted(set(a_entries) & set(b_entries)):
        a, b = a_entries[h], b_entries[h]
        if a["mask"] != b["mask"] or a["file"] != b["file"]:
            print("%s: mask/file differ" % h)
            bad += 1
        for stream in STREAMS:
            if a_blobs[stream] is None or b_blobs[stream] is None:
                continue
            x = a_blobs[stream][a[stream][0]:a[stream][0] + a[stream][1]]
            y = b_blobs[stream][b[stream][0]:b[stream][0] + b[stream][1]]
            if stream == "Air":
                # The metallib holds the random name of the temporary source file and a hash over it: two runs of the
                # same program never match, so only the sizes are compared (a rerun from the cache is byte for byte).
                x, y = len(x), len(y)
            if x != y:
                print("%s: %s differs" % (h, stream))
                bad += 1
    for stream in STREAMS:
        if (a_blobs[stream] is None) != (b_blobs[stream] is None):
            print("%s stream present in one file only" % stream)
            bad += 1
    print("%d shaders compared, %d differences" % (len(set(a_entries) & set(b_entries)), bad))
    return 1 if bad else 0


sys.exit(main())
