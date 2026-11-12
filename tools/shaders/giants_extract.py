#!/usr/bin/env python3
"""Extracts every Xenos shader container of an Alchemy game (Skylanders: Giants) from its disc, statically.

    python3 -I tools/shaders/giants_extract.py <game disc .iso> <output dir> [<runtime dump dir to compare with>]

Writes one file per distinct shader, <sha1 prefix>.vs / .ps, holding the container exactly as the game hands it to
D3DVertexShader_Init / D3DPixelShader_Init: the virtual part followed by the physical part (the microcode). That is
what XenosRecomp reads, and what d3d/shaders.cpp hashes at run time. Needs only the Python standard library.
Also writes <output dir>/manifest.txt (one line per container: archive, entry, offset). See games/giants/docs/shaders.md.

Formats (reverse-engineered from the disc; see the doc):
  IGA v8, little-endian: 0x34 = hashes[n], then n x (offset, size, flags); chunked LZMA when flags != 0xFFFFFFFF.
  IGZ v6, big-endian: sections at 0x10 (offset, size, align, id); section 0 holds the object tables, in which the
  physical (microcode) blocks of a shader are listed as (0x18000000 | size, section<<24 | offset) pairs.
"""
import hashlib
import lzma
import os
import re
import struct
import sys
from multiprocessing import Pool

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xdvdfs  # noqa: E402

SECTOR = xdvdfs.SECTOR
LZMA = [{'id': lzma.FILTER_LZMA1, 'lc': 3, 'lp': 0, 'pb': 2, 'dict_size': 0x8000}]
LZMA_PROPS = bytes.fromhex('5d00800000')
CHUNK = 0x8000
MAGIC = re.compile(rb'\x10\*\x11[\x00\x01]')


def iga_entries(head):
    """(offset, size, flags) of every file of an IGA v8 archive, given the first bytes of the file."""
    if head[:4] != b'IGA\x1a' or struct.unpack_from('<I', head, 4)[0] != 8:
        return None
    n = struct.unpack_from('<I', head, 0x0C)[0]
    return [struct.unpack_from('<3I', head, 0x34 + 4 * n + 12 * i) for i in range(n)]


def unpack(read, offset, size):
    """Decodes a chunked entry: 0x8000-byte chunks, each [u16 n][LZMA props 5][n bytes], or stored; 0x100-aligned."""
    out = bytearray()
    p = offset
    while len(out) < size:
        want = min(CHUNK, size - len(out))
        head = read(p, 7)
        n = struct.unpack_from('<H', head, 0)[0]
        if head[2:7] == LZMA_PROPS:
            body = read(p + 7, n + 8)
            out += lzma.LZMADecompressor(lzma.FORMAT_RAW, filters=LZMA).decompress(body, want)
            p = (p + 7 + n + 0xFF) & ~0xFF
        else:
            out += read(p, want)
            p = (p + want + 0xFF) & ~0xFF
    return bytes(out)


def containers(igz):
    """Yields (offset, bytes) of the shader containers in an IGZ v6 (big-endian) file."""
    sections = [s for s in (struct.unpack_from('>4I', igz, 0x10 + 16 * i) for i in range(16)) if s[1]]
    if not sections:
        return
    blocks = {}  # size -> set of absolute file offsets of the physical blocks
    start, length = sections[0][0], sections[0][1]
    for i in range(start, min(start + length, len(igz)) - 8, 4):
        word, rel = struct.unpack_from('>II', igz, i)
        index = (rel >> 24) + 1
        if word >> 24 == 0x18 and 0 < (word & 0xFFFFFF) < 0x100000 and index < len(sections):
            blocks.setdefault(word & 0xFFFFFF, set()).add(sections[index][0] + (rel & 0xFFFFFF))
    for m in MAGIC.finditer(igz):
        p = m.start()
        virtual, physical = struct.unpack_from('>II', igz, p + 4)
        if not 0x40 <= virtual <= 0x4000 or virtual % 4 or physical not in blocks:
            continue
        near = [a for a in blocks[physical] if p + virtual <= a < p + virtual + 0x40]
        if len(near) == 1 and near[0] + physical <= len(igz):
            yield p, igz[p:p + virtual] + igz[near[0]:near[0] + physical]


def scan_archive(args):
    iso, base, path, start, length = args
    found = []
    with open(iso, 'rb') as f:
        def read(off, n):
            f.seek(base + start * SECTOR + off)
            return f.read(n)

        count = struct.unpack_from('<I', read(0, 0x10), 0x0C)[0]
        entries = iga_entries(read(0, 0x34 + 16 * count))
        if entries is None:
            return path, 0, []
        for offset, size, flags in entries:
            if flags == 0xFFFFFFFF:
                if read(offset, 4) != b'IGZ\x01':
                    continue
                data = read(offset, size)
            else:
                first = unpack(read, offset, min(size, 4))
                if first != b'IGZ\x01':
                    continue
                data = unpack(read, offset, size)
            if data[:4] != b'IGZ\x01' or struct.unpack_from('>I', data, 4)[0] != 6:
                continue
            for p, blob in containers(data):
                found.append((offset, p, blob))
    return path, len(entries), found


def main():
    iso, out = sys.argv[1], sys.argv[2]
    compare = sys.argv[3] if len(sys.argv) > 3 else None
    os.makedirs(out, exist_ok=True)
    with open(iso, 'rb') as f:
        base = xdvdfs.find_partition(f)
        f.seek(base + 32 * SECTOR + 20)
        root, size = struct.unpack('<II', f.read(8))
        files = []
        xdvdfs.walk(f, base, root, size, '', files)
    jobs = [(iso, base, path, start, length) for path, length, start in sorted(files)
            if length and path.endswith(('.arc', '.bld'))]
    seen = {}
    manifest = []
    with Pool(int(os.environ.get('JOBS', '4'))) as pool:
        for path, n, found in pool.imap_unordered(scan_archive, jobs):
            for offset, p, blob in found:
                key = hashlib.sha1(blob).hexdigest()
                manifest.append(f'{path} {offset:#x} {p:#x} {key[:16]}')
                seen.setdefault(key, blob)
            print(f'{path}: {n} entries, {len(found)} containers', file=sys.stderr, flush=True)
    for key, blob in seen.items():
        vertex = blob[3] & 1
        with open(os.path.join(out, f'{key[:16]}.{"vs" if vertex else "ps"}'), 'wb') as o:
            o.write(blob)
    with open(os.path.join(out, 'manifest.txt'), 'w') as o:
        o.write('\n'.join(sorted(manifest)) + '\n')
    print(f'{len(seen)} distinct containers ({sum(1 for b in seen.values() if b[3] & 1)} vertex)')
    if compare:
        dumps = {hashlib.sha1(open(os.path.join(compare, n), 'rb').read()).hexdigest(): n for n in os.listdir(compare)}
        both = len(set(dumps) & set(seen))
        print(f'runtime dump: {len(dumps)}; found by the extractor: {both}; missing: {len(dumps) - both}; '
              f'extra (not in the dump): {len(seen) - both}')
        with open(os.path.join(out, 'missing.txt'), 'w') as o:
            o.write('\n'.join(sorted(dumps[k] for k in set(dumps) - set(seen))) + '\n')


if __name__ == '__main__':
    main()
