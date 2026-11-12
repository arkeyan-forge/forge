"""Feature census of Xenos shader microcode, to find what a translation may silently get wrong.

XenosRecomp ignores some instructions instead of failing (see docs/runtime/gpu/shaders.md), so the build report needs an
independent look at which features each shader uses. Bit layouts follow XenosRecomp's shader_code.h.

usage: python3 -I tools/shaders/ucode.py <container files or folders...>
Folders are walked for .ps/.vs containers; duplicates are removed by content, including across folders.
"""
import collections
from pathlib import Path
import struct
import sys

CF_NAMES = ["Nop", "Exec", "ExecEnd", "CondExec", "CondExecEnd", "CondExecPred", "CondExecPredEnd", "LoopStart",
            "LoopEnd", "CondCall", "Return", "CondJmp", "Alloc", "CondExecPredClean", "CondExecPredCleanEnd",
            "MarkVsFetchDone"]
EXEC_OPS = {1, 2, 3, 4, 5, 6, 13, 14}
BOOL_EXEC_OPS = {3, 4, 13, 14}   # clause runs only if a bool constant matches
PRED_EXEC_OPS = {5, 6}           # clause runs only if p0 matches
FETCH_NAMES = {0: "VertexFetch", 1: "TextureFetch", 16: "GetTextureBorderColorFrac", 17: "GetTextureComputedLod",
               18: "GetTextureGradients", 19: "GetTextureWeights", 24: "SetTextureLod",
               25: "SetTextureGradientsHorz", 26: "SetTextureGradientsVert"}
TEXTURE_DIMENSIONS = ["1D", "2D", "3D", "Cube"]
EXPORT_ADDRESS = 32


def alu_swizzle(value):
    """ALU swizzles encode a component-relative rotation, unlike fetch swizzles."""
    return tuple(((value >> (2 * i)) + i) & 3 for i in range(4))


def microcode(container):
    """(shader stage, microcode dwords) of a D3D shader container."""
    flags, virtual_size, _, _, _, _, shader_offset = struct.unpack_from(">7I", container, 0)
    physical_offset, size = struct.unpack_from(">2I", container, shader_offset)
    start = virtual_size + physical_offset
    words = struct.unpack_from(f">{size // 4}I", container, start)
    return ("vs" if flags & 1 else "ps"), words


def control_flow(words):
    """Yield the 48-bit control flow instructions, stopping where the first clause's instructions begin."""
    end = len(words) // 3
    index = 0
    while index < end:
        w0, w1, w2 = words[index * 3: index * 3 + 3]
        for value in (w0 | (w1 & 0xFFFF) << 32, (w1 >> 16) | (w2 << 16)):
            opcode = (value >> 44) & 0xF
            if opcode in EXEC_OPS and value & 0xFFF:
                end = min(end, value & 0xFFF)
            yield value
        index += 1


def census(container):
    """Counter of the features one shader uses."""
    stage, words = microcode(container)
    features = collections.Counter()
    for cf in control_flow(words):
        opcode = (cf >> 44) & 0xF
        features[f"cf.{CF_NAMES[opcode]}"] += 1
        if opcode == 11 and (cf >> 13) & 1 == 0 and (cf >> 14) & 1 == 0:
            features["cf.CondJmp.bool"] += 1
        if opcode not in EXEC_OPS:
            continue
        address, count, sequence = cf & 0xFFF, (cf >> 12) & 7, (cf >> 16) & 0xFFF
        for i in range(count):
            c0, c1, c2 = words[(address + i) * 3: (address + i) * 3 + 3]
            is_fetch = (sequence >> (2 * i)) & 1
            if opcode in PRED_EXEC_OPS and not (c1 >> (31 if is_fetch else 28)) & 1:
                features["cf.CondExecPred.unpredicatedInstr"] += 1
            if is_fetch:
                fetch = c0 & 0x1F
                features[f"fetch.{FETCH_NAMES.get(fetch, fetch)}"] += 1
                if fetch == 0:
                    features[f"vfetch.format.{(c1 >> 16) & 0x3F}"] += 1
                    if (c1 >> 30) & 1:
                        features["vfetch.mini"] += 1
                elif fetch in (1, 19):
                    features[f"tfetch.{TEXTURE_DIMENSIONS[(c2 >> 14) & 3]}"] += 1
                    if (c1 >> 29) & 1:
                        features["tfetch.useRegLod"] += 1
                    if (c1 >> 28) & 1:
                        features["tfetch.useCompLod"] += 1
                    if fetch == 1 and not (c1 >> 28) & 3:
                        features["tfetch.lodZero"] += 1
                    if c2 & 1:
                        features["tfetch.useRegGradients"] += 1
                    if (c2 >> 2) & 0x7F:
                        features["tfetch.lodBias"] += 1
            else:
                if (c2 >> 24) & 0x1F == 18:
                    features["alu.Cube"] += 1
                    if (alu_swizzle(c1 >> 16) != (2, 2, 0, 1) or
                            alu_swizzle(c1 >> 8) != (1, 0, 2, 2)):
                        features["alu.Cube.noncanonical"] += 1
                if (c1 >> 31) & 1 or (c1 >> 30) & 1:
                    features["alu.relativeConstant"] += 1
                if (c0 >> 6) & 1 or (c0 >> 14) & 1:
                    features["alu.relativeDest"] += 1
                if (c0 >> 15) & 1 and (c0 & 0x3F) == EXPORT_ADDRESS:
                    features["alu.memexport"] += 1
                if (c0 >> 15) & 1 and stage == "vs" and (c0 & 0x3F) == 63:
                    features["alu.pointSizeEdgeKill"] += 1
                if (c0 >> 26) & 0x3F == 50:
                    features["alu.RetainPrev"] += 1
    return features


def main():
    total = collections.Counter()
    shaders_using = collections.Counter()
    seen = set()
    for arg in sys.argv[1:]:
        path = Path(arg)
        paths = sorted(p for p in path.rglob("*") if p.suffix in (".ps", ".vs")) if path.is_dir() else [path]
        for path in paths:
            container = path.read_bytes()
            if container in seen:
                continue
            seen.add(container)
            features = census(container)
            total.update(features)
            shaders_using.update(features.keys())
    print(f"{len(seen)} distinct containers (deduplicated by content)")
    for name in sorted(total):
        print(f"{name:32} {shaders_using[name]:5} shaders {total[name]:7} uses")


if __name__ == "__main__":
    main()
