#!/usr/bin/env python3
"""Extracts every Xenos shader container of Skylanders: Swap Force (Alchemy, IGA v10 / IGZ v7) from its disc, statically.

    python3 -I tools/shaders/swapforce_extract.py <game disc .iso> <output dir> [<runtime dump dir to compare with>]
    ARCHIVES=all ...   scan every .pak of the disc (default: /archives/shaders_xenon.pak only, where they all are)

Writes one file per distinct shader, <sha1 prefix>.vs / .ps, holding the container exactly as the game hands it to
D3DVertexShader_Init / D3DPixelShader_Init: the virtual part followed by the physical part (the microcode), as
giants_extract.py does for Giants. Also writes manifest.txt (archive, entry, offset, hash) and, with a dump dir,
missing.txt (the dumped containers the disc does not have). Python standard library only. See
games/swapforce/docs/shaders.md.

Formats (reverse-engineered from the disc; see the doc):
  IGA v10, big-endian: header 0x38, u32 hashes[n], then n x {?, offset, size, mode}; names at 0x2C; chunked LZMA as v8
  but 0x800-aligned.
  IGZ v7, big-endian: sections at 0x14 {?, offset, size, alignment}; section 0 holds tagged records (tags stored
  byte-swapped); TMHN lists the memory handles {kind<<24 | size, pool<<27 | offset}, kind 0x18 = physical memory. The
  shaders' virtual parts sit in an object pool and their physical parts in the 0x20-aligned pool, in the same order.
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
CHUNK, ALIGN = 0x8000, 0x800
MAGIC = re.compile(rb'\x10\*\x11[\x00\x01]')


def iga_entries(head):
    """(offset, size, mode) of every file of an IGA v10 (big-endian) archive, given the first bytes of the file."""
    if head[:8] != b'\x1aAGI\x00\x00\x00\x0a':
        return None
    n = struct.unpack_from('>I', head, 0x0C)[0]
    return [struct.unpack_from('>3I', head, 0x38 + 4 * n + 16 * i + 4) for i in range(n)]


def unpack(read, offset, size):
    """Decodes a chunked entry: 0x8000-byte chunks, each [u16 n (LE)][LZMA props 5][n bytes], or stored; 0x800-aligned."""
    out = bytearray()
    p = offset
    while len(out) < size:
        want = min(CHUNK, size - len(out))
        head = read(p, 7)
        n = struct.unpack_from('<H', head, 0)[0]
        if head[2:7] == LZMA_PROPS:
            body = read(p + 7, n + 8)
            out += lzma.LZMADecompressor(lzma.FORMAT_RAW, filters=LZMA).decompress(body, want)
            p = (p + 7 + n + ALIGN - 1) & ~(ALIGN - 1)
        else:
            out += read(p, want)
            p = (p + want + ALIGN - 1) & ~(ALIGN - 1)
    return bytes(out)


def containers(igz):
    """Yields (offset, bytes) of the shader containers in an IGZ v7 (big-endian) file."""
    sections = []
    for i in range(16):
        _, offset, size, _ = struct.unpack_from('>4I', igz, 0x14 + 16 * i)
        if not size:
            break
        sections.append((offset, size))
    if not sections:
        return
    # The physical memory blocks, in order, from the TMHN record (tag stored byte-swapped: "NHMT").
    physical = []
    o, end = sections[0][0], min(sections[0][0] + sections[0][1], len(igz))
    while o + 16 <= end:
        count, length, start = struct.unpack_from('>3I', igz, o + 4)
        if length < 16:
            break
        if igz[o:o + 4] == b'NHMT':
            for i in range(count):
                word, rel = struct.unpack_from('>II', igz, o + start + 8 * i)
                index = (rel >> 27) + 1
                if word >> 24 == 0x18 and index < len(sections):
                    physical.append((word & 0xFFFFFF, sections[index][0] + (rel & 0x7FFFFFF)))
        o += length
    # Each container takes the next physical block of its physical size, in file order.
    k = 0
    for m in MAGIC.finditer(igz):
        p = m.start()
        virtual, size = struct.unpack_from('>II', igz, p + 4)
        if not 0x40 <= virtual <= 0x4000 or virtual % 4:
            continue
        j = k
        while j < len(physical) and physical[j][0] != size:
            j += 1
        if j == len(physical):
            continue
        k = j + 1
        at = physical[j][1]
        if at + size <= len(igz):
            yield p, igz[p:p + virtual] + igz[at:at + size]


def scan_archive(args):
    iso, base, path, start, length = args
    found = []
    with open(iso, 'rb') as f:
        def read(off, n):
            f.seek(base + start * SECTOR + off)
            return f.read(n)

        count = struct.unpack_from('>I', read(0, 0x10), 0x0C)[0]
        entries = iga_entries(read(0, 0x38 + 20 * count))
        if entries is None:
            return path, 0, []
        for offset, size, mode in entries:
            if size < 0x40:
                continue
            if mode == 0xFFFFFFFF:
                if read(offset, 4) != b'IGZ\x01':
                    continue
                data = read(offset, size)
            else:
                if unpack(read, offset, 4) != b'IGZ\x01':
                    continue
                data = unpack(read, offset, size)
            if struct.unpack_from('>I', data, 4)[0] != 7:
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
    every = os.environ.get('ARCHIVES') == 'all'
    jobs = [(iso, base, path, start, length) for path, length, start in sorted(files)
            if length and (path.lower().endswith('.pak') if every else path.lower() == '/archives/shaders_xenon.pak')]
    seen = {}
    manifest = []
    with Pool(int(os.environ.get('JOBS', '4'))) as pool:
        for path, n, found in pool.imap_unordered(scan_archive, jobs):
            for offset, p, blob in found:
                key = hashlib.sha1(blob).hexdigest()
                manifest.append(f'{path} {offset:#x} {p:#x} {key[:16]}')
                seen.setdefault(key, blob)
            if found or not every:
                print(f'{path}: {n} entries, {len(found)} containers', file=sys.stderr, flush=True)
    for key, blob in seen.items():
        with open(os.path.join(out, f'{key[:16]}.{"vs" if blob[3] & 1 else "ps"}'), 'wb') as o:
            o.write(blob)
    with open(os.path.join(out, 'manifest.txt'), 'w') as o:
        o.write('\n'.join(sorted(manifest)) + '\n')
    print(f'{len(jobs)} archives, {len(manifest)} containers, {len(seen)} distinct '
          f'({sum(1 for b in seen.values() if b[3] & 1)} vertex)')
    if compare:
        dumps = {hashlib.sha1(open(os.path.join(compare, n), 'rb').read()).hexdigest(): n for n in os.listdir(compare)}
        both = len(set(dumps) & set(seen))
        print(f'runtime dump: {len(dumps)}; found by the extractor: {both}; missing: {len(dumps) - both}; '
              f'extra (not in the dump): {len(seen) - both}')
        with open(os.path.join(out, 'missing.txt'), 'w') as o:
            o.write('\n'.join(sorted(dumps[k] for k in set(dumps) - set(seen))) + '\n')


if __name__ == '__main__':
    main()
