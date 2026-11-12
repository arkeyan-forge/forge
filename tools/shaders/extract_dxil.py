#!/usr/bin/env python3
# Writes every DXIL container of private/ssa/ppc/shader_cache.cpp (each specialization variant of each shader) to one file, for
# tools/shaders/dxil_check.cpp: per container, the shader hash (u64), the variant index (u32), the size (u32), then the
# bytes. Prints a summary of the DXIL entries (variants, shaders whose vertex shader outputs a point size).
#
#   python3 -I tools/shaders/extract_dxil.py private/ssa/ppc/shader_cache.cpp <output file>
#
# Needs the zstd command line tool.
import re
import struct
import subprocess
import sys

source, output = sys.argv[1], sys.argv[2]
text = open(source, encoding='latin-1').read()

entries = [(int(h, 16), int(offset), int(size)) for h, offset, size in
           re.findall(r'\{ 0x([0-9A-F]+), (\d+), (\d+), \d+, \d+, \d+, \d+, \d+, "', text)]
match = re.search(r'g_compressedDxilCache\[\] = \{([0-9,]*)\}', text)
if match is None:
    sys.exit('no DXIL in ' + source)
compressed = bytes(int(value) for value in match.group(1).rstrip(',').split(','))
dxil = subprocess.run(['zstd', '-d', '-c'], input=compressed, stdout=subprocess.PIPE, check=True).stdout

containers = 0
variants = {}
point_size = 0
with open(output, 'wb') as out:
    for shader, offset, size in entries:
        if size == 0:
            continue
        flags, count = struct.unpack_from('<II', dxil, offset)
        sizes = struct.unpack_from('<%dI' % count, dxil, offset + 8)
        at = offset + 8 + 4 * count
        for index, blob_size in enumerate(sizes):
            blob = dxil[at:at + blob_size]
            assert blob[:4] == b'DXBC', hex(shader)
            out.write(struct.pack('<QII', shader, index, blob_size) + blob)
            at += blob_size
            containers += 1
        assert at == offset + size, hex(shader)
        variants[count] = variants.get(count, 0) + 1
        point_size += flags & 1

print('%d shaders with DXIL (%s), %d containers, %d vertex shaders output a point size' % (
    sum(variants.values()), ', '.join('%d with %d variant(s)' % (n, c) for c, n in sorted(variants.items())),
    containers, point_size))
