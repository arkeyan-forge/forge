"""Reader for the lux engine's shader archives (shaders.lsa, effects.lsa at the root of the disc).

Layout (all big-endian; reverse-engineered from the files and the loader at 0x828FE7B0):

    header (0x40 bytes)
        u32 magic        0x0041534C ("\\0ASL")
        u32 version      2
        u32 shaderCount
        u32 shaderTable  offset of shaderCount x 32-byte shader entries (0x40)
        u32 programCount
        u32 programTable offset of programCount x 32-byte program entries
        (zero padding)
    shader entry (32 bytes)
        u32 keyCount     number of u32 keys stored in front of the container
        u32 keyOffset    the keys, then 0x55 padding up to the container
        u32 type         0x1909BEB3 = vertex shader, 0x88E6E692 = pixel shader (name hashes)
        u32 size         container size = virtualSize + physicalSize
        u32 dataOffset   the D3D shader container (magic 0x102A1101 vertex / 0x102A1100 pixel)
        u32 reserved[3]
    program entry (32 bytes)
        u32 group, name  hashes (group = the effect file, name = the technique/permutation)
        3 x (u32 count, u32 offset): a list of u32, a list of u32 and a list of 16-byte records
        (they reference shaders and constant/sampler bindings; not needed to translate shaders)

The loader hands each container verbatim to D3DDevice_CreateVertexShader (0x82290508) or
D3DDevice_CreatePixelShader (0x82290160), so the bytes extracted here are the bytes the runtime will see.
"""
import struct
import sys

MAGIC = 0x0041534C
TYPE_VERTEX = 0x1909BEB3
TYPE_PIXEL = 0x88E6E692
CONTAINER_MAGIC = 0x102A1100  # low bit set = vertex shader


class Shader:
    def __init__(self, index, stage, keys, data):
        self.index = index
        self.stage = stage  # "vs" or "ps"
        self.keys = keys
        self.data = data


def parse(data):
    """Return (shaders, programs) from the bytes of an .lsa archive."""
    magic, version, shader_count, shader_table, program_count, program_table = struct.unpack_from(">6I", data, 0)
    if magic != MAGIC or version != 2:
        raise ValueError(f"not an LSA v2 archive (magic {magic:#x}, version {version})")

    shaders = []
    for i in range(shader_count):
        key_count, key_offset, kind, size, offset = struct.unpack_from(">5I", data, shader_table + i * 32)
        flags = struct.unpack_from(">I", data, offset)[0]
        if flags & 0xFFFFFF00 != CONTAINER_MAGIC:
            raise ValueError(f"shader {i}: no D3D shader container at {offset:#x}")
        stage = "vs" if flags & 1 else "ps"
        if (kind == TYPE_VERTEX) != (stage == "vs") or kind not in (TYPE_VERTEX, TYPE_PIXEL):
            raise ValueError(f"shader {i}: type {kind:#x} does not match container flags {flags:#x}")
        keys = list(struct.unpack_from(f">{key_count}I", data, key_offset))
        shaders.append(Shader(i, stage, keys, data[offset:offset + size]))

    programs = []
    for i in range(program_count):
        group, name, *lists = struct.unpack_from(">8I", data, program_table + i * 32)
        programs.append((group, name, [(lists[j], lists[j + 1]) for j in range(0, 6, 2)]))
    return shaders, programs


def find_containers(data):
    """Scan any blob (e.g. the loaded default.xex image) for D3D shader containers, like XenosRecomp does."""
    found = []
    offset = 0
    while offset + 0x24 <= len(data):
        flags, virtual_size, physical_size = struct.unpack_from(">3I", data, offset)
        field1c, field20 = struct.unpack_from(">2I", data, offset + 0x1C)
        size = virtual_size + physical_size
        if flags & 0xFFFFFF00 == CONTAINER_MAGIC and field1c == 0 and field20 == 0 and 0x24 < size <= len(data) - offset:
            found.append((offset, "vs" if flags & 1 else "ps", data[offset:offset + size]))
            offset += size
        else:
            offset += 4
    return found


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: python3 -I tools/shaders/lsa.py <archive.lsa>")
    shaders, programs = parse(open(sys.argv[1], "rb").read())
    vs = sum(1 for s in shaders if s.stage == "vs")
    print(f"{len(shaders)} shaders ({vs} vertex, {len(shaders) - vs} pixel), {len(programs)} programs")
    for s in shaders:
        print(f"{s.index:5} {s.stage} {len(s.data):6} keys={' '.join(f'{k:08x}' for k in s.keys)}")


if __name__ == "__main__":
    main()
