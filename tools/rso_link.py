"""Link a Wii game's RSO module at a fixed address, as the SDK's own linker would, and add it to the game's ELF.

A Wii game may keep part of its code in an RSO, a relocatable module it loads at run time (SuperChargers Racing:
`gamelogic.rso`). The host loads it at a fixed address (games/<id>/README.md says where), so it can be translated with
the rest: this tool links it there and writes one ELF holding the game's and the module's code, for XenonRecomp.

The link is the SDK's (RSOLinkList in the game, read from its code): each section's address is the module's plus its
offset (the section with a size and no offset is the BSS, at its own address); an internal relocation's offset is
from the module's start, its symbol the section it points into; an import's relocations point at the module's
`unresolved` function, then at the symbol of that name the game exports: with `--exports`, the game's own export table
(entries of 12 bytes: a section, the name's address, the symbol's address; SuperChargers Racing's engine builds its
exports from one, `ExposeEngineRSOSymbols`, and some names there are not the ELF's symbols: `sqrt` is a single-precision
function), else the ELF's symbols. A name not exported stays at `unresolved`. Relocation types:
1 ADDR32, 2 ADDR24, 3 ADDR16, 4 ADDR16_LO, 5 ADDR16_HI, 6 ADDR16_HA, 7 ADDR14, 10 REL24, 11 REL14; a REL24 out of
reach is an error (the module must sit within 32 MB of what it calls).

The module has no symbols for its own functions: they are found from the linked code. A function starts at the module's
prolog, epilog and unresolved functions, its exports, every `bl` target, and every code address its data or its
`lis`/`addi` pairs hold (vtables, function pointers), except the labels of a jump table (a table whose address an
`addi` loads for an `lwzx`, `mtctr`, `bctr`); and where a branch from another function goes (a tail call), when an
unconditional end comes before it (padding, `blr`, `b`, `bctr`). Each ends where the next one starts.

usage: python3 -I tools/rso_link.py <game.elf> <module.rso> <module address> <bss address> <out.elf>
           [--exports <table address> <count>] [--hash <hash.h>]
       python3 -I tools/rso_link.py --self-test
The header gets the linked sections' FNV-1a hash (`GAME_MODULE_LINK_HASH`), for the host to check the game's own link
against.
"""
import bisect
import struct
import sys

SECTION_HEADER = ">10I"


def u32(data, offset):
    return struct.unpack_from(">I", data, offset)[0]


class Rso:
    def __init__(self, data):
        self.data = data
        self.sections = [(u32(data, u32(data, 0x0C) + 8 * i), u32(data, u32(data, 0x0C) + 8 * i + 4))
                         for i in range(u32(data, 0x08))]
        self.bss_size = u32(data, 0x1C)
        self.prolog_section, self.epilog_section, self.unresolved_section = data[0x20], data[0x21], data[0x22]
        self.prolog, self.epilog, self.unresolved = u32(data, 0x24), u32(data, 0x28), u32(data, 0x2C)
        self.internal = self.relocations(u32(data, 0x30), u32(data, 0x34))
        self.external = self.relocations(u32(data, 0x38), u32(data, 0x3C))
        names = u32(data, 0x48)
        self.exports = [(self.string(names + u32(data, u32(data, 0x40) + 16 * i)), u32(data, u32(data, 0x40) + 16 * i + 4),
                         u32(data, u32(data, 0x40) + 16 * i + 8)) for i in range(u32(data, 0x44) // 16)]
        names = u32(data, 0x54)
        self.imports = [self.string(names + u32(data, u32(data, 0x4C) + 12 * i)) for i in range(u32(data, 0x50) // 12)]

    def relocations(self, at, size):
        return [struct.unpack_from(">III", self.data, at + 12 * i) for i in range(size // 12)]

    def string(self, at):
        return self.data[at:self.data.index(b"\0", at)].decode("latin-1")


class Elf:
    def __init__(self, data):
        if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 2:
            raise SystemExit("not a 32-bit big-endian ELF")
        self.data = data
        shoff, = struct.unpack_from(">I", data, 0x20)
        count, strings = struct.unpack_from(">HH", data, 0x30)
        self.headers = [list(struct.unpack_from(SECTION_HEADER, data, shoff + 40 * i)) for i in range(count)]
        self.symbols = {}
        for header in self.headers:
            if header[1] != 2:  # SHT_SYMTAB
                continue
            names = self.headers[header[6]][4]
            for i in range(header[5] // 16):
                name, value, size, info, _, section = struct.unpack_from(">IIIBBH", data, header[4] + 16 * i)
                if name and section and info & 0xF in (0, 1, 2):  # NOTYPE, OBJECT, FUNC, defined
                    label = data[names + name:data.index(b"\0", names + name)].decode("latin-1")
                    self.symbols.setdefault(label, value)
        self.loaded = [(h[3], h[3] + h[5], h[4]) for h in self.headers if h[1] == 1 and h[3]]

    def u32(self, address):
        for start, end, offset in self.loaded:
            if start <= address and address + 4 <= end:
                return u32(self.data, offset + address - start)
        raise ValueError(f"{address:#x} is in no section")

    def string(self, address):
        for start, end, offset in self.loaded:
            if start <= address < end:
                at = offset + address - start
                return self.data[at:self.data.index(b"\0", at)].decode("latin-1")
        raise ValueError(f"{address:#x} is in no section")

    def export_table(self, address, count):
        """The game's export table: name -> address."""
        return {self.string(self.u32(address + 12 * i + 4)): self.u32(address + 12 * i + 8) for i in range(count)}


def apply(image, kind, place, at, value):
    """One relocation into `image` (the module's bytes, `place` its offset there, `at` its address)."""
    word = u32(image, place) if place + 4 <= len(image) else 0
    if kind == 1:
        struct.pack_into(">I", image, place, value & 0xFFFFFFFF)
    elif kind == 2:
        struct.pack_into(">I", image, place, word & 0xFC000003 | value & 0x03FFFFFC)
    elif kind in (3, 4):
        struct.pack_into(">H", image, place, value & 0xFFFF)
    elif kind == 5:
        struct.pack_into(">H", image, place, value >> 16 & 0xFFFF)
    elif kind == 6:
        struct.pack_into(">H", image, place, (value >> 16) + (value >> 15 & 1) & 0xFFFF)
    elif kind == 7:
        struct.pack_into(">I", image, place, word & 0xFFFF0003 | value & 0xFFFC)
    elif kind in (10, 11):
        distance = value - at
        limit = 1 << (25 if kind == 10 else 15)
        if not -limit <= distance < limit:
            raise ValueError(f"a branch at {at:#x} to {value:#x} is out of reach")
        mask = 0x03FFFFFC if kind == 10 else 0xFFFC
        struct.pack_into(">I", image, place, word & ~mask & 0xFFFFFFFF | distance & mask)
    elif kind not in (0, 109):
        raise ValueError(f"relocation type {kind} at {at:#x} not known")


def link(rso, symbols, module, bss):
    """The module's bytes as linked at `module` (its BSS at `bss`), and each section's address."""
    image = bytearray(rso.data)
    bases = [0] * len(rso.sections)
    for i, (offset, size) in enumerate(rso.sections):
        if offset:
            bases[i] = module + (offset & ~1)
        elif size:
            bases[i] = bss
    for offset, info, addend in rso.internal:
        apply(image, info & 0xFF, offset, module + offset, bases[info >> 8] + addend)
    unresolved = bases[rso.unresolved_section] + rso.unresolved
    for offset, info, addend in rso.external:
        name = rso.imports[info >> 8]
        apply(image, info & 0xFF, offset, module + offset, symbols.get(name, unresolved) + addend)
    return image, bases


def functions(rso, image, bases, module):
    """The module's function starts, sorted (addresses), and the code's range."""
    code = max((i for i, (o, s) in enumerate(rso.sections) if o and s), key=lambda i: rso.sections[i][1])
    start, end = bases[code], bases[code] + rso.sections[code][1]
    word = lambda address: u32(image, address - module)
    in_code = lambda address: start <= address < end and address % 4 == 0
    starts = {bases[rso.prolog_section] + rso.prolog, bases[rso.epilog_section] + rso.epilog,
              bases[rso.unresolved_section] + rso.unresolved}
    starts |= {bases[section] + value for _, value, section in rso.exports}
    for address in range(start, end, 4):
        w = word(address)
        if w >> 26 == 18 and w & 3 == 1:  # bl
            target = address + ((w & 0x03FFFFFC) ^ 0x02000000) - 0x02000000
            if in_code(target):
                starts.add(target)
    # Code addresses the module holds: in its data (ADDR32), or built by an lis/addi pair (the LO half).
    held, tables = {}, set()
    for offset, info, addend in rso.internal:
        kind, value = info & 0xFF, bases[info >> 8] + addend
        if kind in (1, 4) and in_code(value):
            held[module + offset] = (kind, value)
        if kind == 4 and not in_code(value):
            loaded = module + offset - 2  # the instruction the LO half is in
            count = jump_table_load(word, loaded, end) if in_code(loaded) else None
            if count:
                tables.add((value, count))
    labels = set()
    for table, count in tables:
        for at in range(table, table + 4 * count, 4):
            if at in held and held[at][0] == 1:
                labels.add(held[at][1])
    starts |= {value for kind, value in held.values() if value not in labels}
    # A branch to another function (a tail call): its target starts one when an unconditional end comes before it
    # (padding, blr, b, bctr). Each new start splits a function, so until no branch finds another.
    ends = lambda w: w == 0 or w in (0x4E800020, 0x4E800420) or (w >> 26 == 18 and w & 3 == 0)
    branches = []
    for address in range(start, end, 4):
        w = word(address)
        if w >> 26 == 18 and w & 3 == 0:  # b
            branches.append((address, address + ((w & 0x03FFFFFC) ^ 0x02000000) - 0x02000000))
        elif w >> 26 == 16 and w & 3 == 0:  # bc
            branches.append((address, address + ((w & 0xFFFC) ^ 0x8000) - 0x8000))
    while True:
        ordered = sorted(starts)
        region = lambda address: bisect.bisect_right(ordered, address)
        found = {target for source, target in branches if in_code(target) and target not in starts and
                 region(source) != region(target) and ends(word(target - 4))}
        if not found:
            return ordered, start, end
        starts |= found


def jump_table_load(word, address, end):
    """The labels of the jump table the `addi` at `address` loads (an `lwzx` from it, then `mtctr` and `bctr`, soon
    after): its bounds check's N + 1 (the nearest `cmplwi` around the load); None for no jump table."""
    w = word(address)
    if w >> 26 != 14:
        return None
    table = w >> 21 & 31
    for at in range(address + 4, min(address + 4 * 24, end), 4):
        w = word(at)
        if w >> 26 == 31 and w >> 1 & 0x3FF == 23 and w >> 16 & 31 == table:  # lwzx rX, rT, rO
            loaded = w >> 21 & 31
            m, b = word(at + 4), word(at + 8)
            if m != (31 << 26 | loaded << 21 | 9 << 16 | 467 << 1) or b != 0x4E800420:
                return None
            for back in range(at - 4, max(address - 4 * 24, 0) - 4, -4):
                c = word(back)
                if c >> 26 == 10 and not c >> 21 & 1:  # cmplwi crN, rI, N
                    return (c & 0xFFFF) + 1
            return None
        if w == 0x4E800020:  # blr: the function's end
            return None
    return None


def combine(elf, rso, image, bases, starts, code_end):
    """The game's ELF with the module's sections and a symbol table of its functions appended."""
    out = bytearray(elf.data)
    headers = [h[:] for h in elf.headers]
    code = max((i for i, (o, s) in enumerate(rso.sections) if o and s), key=lambda i: rso.sections[i][1])
    for i, (offset, size) in enumerate(rso.sections):
        if not offset or not size:
            continue
        while len(out) % 32:
            out.append(0)
        place = len(out)
        out += image[offset & ~1:(offset & ~1) + size]
        flags = 6 if i == code else 3  # ALLOC | EXECINSTR, or ALLOC | WRITE
        headers.append([0, 1, flags, bases[i], place, size, 0, 0, 32, 0])
    while len(out) % 4:
        out.append(0)
    place = len(out)
    out += b"\0" * 16
    ends = starts[1:] + [code_end]
    for address, end in zip(starts, ends):
        out += struct.pack(">IIIBBH", 0, address, end - address, 0x12, 0, 1)  # GLOBAL FUNC
    headers.append([0, 2, 0, 0, place, 16 * (len(starts) + 1), 0, 1, 4, 16])
    while len(out) % 4:
        out.append(0)
    shoff = len(out)
    for header in headers:
        out += struct.pack(SECTION_HEADER, *header)
    struct.pack_into(">I", out, 0x20, shoff)
    struct.pack_into(">H", out, 0x30, len(headers))
    return out


def fnv1a(data):
    value = 0x811C9DC5
    for byte in data:
        value = (value ^ byte) * 0x01000193 & 0xFFFFFFFF
    return value


def linked_sections(rso, image):
    """The linked bytes the host checks: every loaded section's, in order."""
    return b"".join(bytes(image[o & ~1:(o & ~1) + s]) for o, s in rso.sections if o and s)


def run(elf_data, rso_data, module, bss, exports=None):
    elf, rso = Elf(elf_data), Rso(rso_data)
    symbols = elf.export_table(*exports) if exports else elf.symbols
    image, bases = link(rso, symbols, module, bss)
    starts, _, code_end = functions(rso, image, bases, module)
    missing = sorted({rso.imports[info >> 8] for _, info, _ in rso.external} - set(symbols))
    return elf, rso, image, bases, starts, code_end, missing


def self_test():
    """A module of two functions (one calls the other and the game; its data holds the first's address and a jump
    table), linked against a game of one symbol."""
    blr, bctr = 0x4E800020, 0x4E800420
    game = 0x80010000
    module, bss = 0x81800000, 0x81810000
    code = [0x48000001, blr,  # 0x60: bl (to 0x68: an internal REL24), blr
            0x48000001, 0x48000028,  # 0x68: bl (to the game: an import), b 0x94 (a tail call)
            0x28040000, 0x3C600000, 0x38630000,  # 0x70: cmplwi r4, 0 (one label), lis, addi (the table)
            0x7C63202E, 0x7C6903A6, bctr, blr,  # lwzx, mtctr, bctr; 0x88: the label
            0, 0, blr]  # padding, then 0x94: a function only a tail call reaches
    data = [0, 0, 0]  # 0x98: the jump table's label, then a function pointer (0x70)
    body = b"".join(struct.pack(">I", w) for w in code + data)
    header_size = 0x60  # past the header's fields
    info = header_size + len(body)
    sections = [(0, 0), (header_size, 4 * len(code)), (header_size + 4 * len(code), 12), (0, 0x20)]
    internal = [(0x60, 1 << 8 | 10, 8), (0x74 + 2, 2 << 8 | 6, 0), (0x78 + 2, 2 << 8 | 4, 0),
                (0x98, 1 << 8 | 1, 0x28), (0x9C, 1 << 8 | 1, 0x10)]
    external = [(0x68, 0 << 8 | 10, 0)]
    imports_names = b"game_function\0"
    rel_internal = info + 8 * len(sections)
    rel_external = rel_internal + 12 * len(internal)
    imports = rel_external + 12 * len(external)
    names = imports + 12
    blob = bytearray(names + len(imports_names))
    fields = [0, 0, len(sections), info, 0, 0, 1, 0x20]
    struct.pack_into(">8I", blob, 0, *fields)
    blob[0x20:0x24] = bytes([1, 1, 1, 3])
    struct.pack_into(">13I", blob, 0x24, 0, 0, 0, rel_internal, 12 * len(internal), rel_external,
                     12 * len(external), 0, 0, 0, imports, 12, names)
    blob[header_size:header_size + len(body)] = body
    for i, (o, s) in enumerate(sections):
        struct.pack_into(">II", blob, info + 8 * i, o, s)
    for i, r in enumerate(internal):
        struct.pack_into(">III", blob, rel_internal + 12 * i, *r)
    struct.pack_into(">III", blob, rel_external, *external[0])
    struct.pack_into(">III", blob, imports, 0, 0, 0)
    blob[names:names + len(imports_names)] = imports_names
    # The game: one section and a symbol table naming game_function.
    strtab = b"\0game_function\0"
    text = struct.pack(">I", blr)
    # The game's export table (one entry: game_function, at another address than its symbol's), then the name.
    table = game + 0x100
    exported = struct.pack(">III", 1, table + 12, game + 0x40) + b"game_function\0\0\0"
    elf = bytearray(52)
    elf[0:7] = b"\x7fELF\x01\x02\x01"
    data_off = 52 + len(text)
    sym_off = data_off + len(exported)
    symtab = b"\0" * 16 + struct.pack(">IIIBBH", 1, game, 4, 0x12, 0, 1)
    str_off = sym_off + len(symtab)
    shoff = str_off + len(strtab)
    elf += text + exported + symtab + strtab
    headers = [[0] * 10, [0, 1, 6, game, 52, 4, 0, 0, 4, 0], [0, 2, 0, 0, sym_off, len(symtab), 4, 1, 4, 16],
               [0, 1, 2, table, data_off, len(exported), 0, 0, 4, 0], [0, 3, 0, 0, str_off, len(strtab), 0, 0, 1, 0]]
    for h in headers:
        elf += struct.pack(SECTION_HEADER, *h)
    struct.pack_into(">HHIIIIIHHHHHH", elf, 16, 2, 20, 1, game, 0, shoff, 0, 52, 0, 0, 40, len(headers), 4)
    parsed, rso, image, bases, starts, code_end, missing = run(bytes(elf), bytes(blob), module, bss)
    word = lambda a: u32(image, a - module)
    checks = [
        bases[1] == module + 0x60 and bases[3] == bss,
        word(module + 0x60) == 0x48000009,  # bl +8
        word(module + 0x68) == (0x48000001 | (game - (module + 0x68)) & 0x03FFFFFC),  # bl the game
        word(module + 0x74) & 0xFFFF == 0x8180 and word(module + 0x78) & 0xFFFF == 0x0098,  # the table's address
        word(module + 0x98) == module + 0x88 and word(module + 0x9C) == module + 0x70,
        starts == [module + 0x60, module + 0x68, module + 0x70, module + 0x94],  # the label 0x88 is not one
        code_end == module + 0x98 and missing == [],
    ]
    combined = Elf(bytes(combine(parsed, rso, image, bases, starts, code_end)))
    added = [h for h in combined.headers[len(headers):]]
    checks.append(len(added) == 3 and added[0][3] == module + 0x60 and added[0][2] == 6 and added[2][1] == 2)
    checks.append(fnv1a(b"a") == 0xE40C292C)
    # With the export table, the import is its address, not the symbol's.
    image = run(bytes(elf), bytes(blob), module, bss, (table, 1))[2]
    checks.append(u32(image, 0x68) == (0x48000001 | (game + 0x40 - (module + 0x68)) & 0x03FFFFFC))
    try:
        apply(bytearray(8), 10, 0, 0x80000000, 0x84000000)
        checks.append(False)
    except ValueError:
        checks.append(True)
    print(f"rso_link self-test: {'PASS' if all(checks) else 'FAIL'} ({sum(checks)} of {len(checks)})")
    if not all(checks):
        print([i for i, c in enumerate(checks) if not c], [hex(s) for s in starts])
    return 0 if all(checks) else 1


def main():
    if sys.argv[1:] == ["--self-test"]:
        return self_test()
    args = sys.argv[1:]
    exports = hash_path = None
    if "--exports" in args:
        at = args.index("--exports")
        exports = (int(args[at + 1], 0), int(args[at + 2], 0))
        del args[at:at + 3]
    if "--hash" in args:
        at = args.index("--hash")
        hash_path = args[at + 1]
        del args[at:at + 2]
    if len(args) != 5:
        raise SystemExit(__doc__)
    module, bss = int(args[2], 0), int(args[3], 0)
    elf_data = open(args[0], "rb").read()
    elf, rso, image, bases, starts, code_end, missing = run(elf_data, open(args[1], "rb").read(), module, bss, exports)
    out = combine(elf, rso, image, bases, starts, code_end)
    open(args[4], "wb").write(out)
    linked = linked_sections(rso, image)
    if hash_path:
        with open(hash_path, "w") as header:
            header.write("// Generated by tools/rso_link.py: the module's linked sections, as the host checks them.\n")
            header.write("#pragma once\n")
            header.write(f"#define GAME_MODULE_LINK_HASH 0x{fnv1a(linked):08X}u\n")
            header.write(f"#define GAME_MODULE_LINK_SIZE 0x{len(linked):X}u\n")
    print(f"{len(starts)} functions in 0x{bases[1]:08X}-0x{code_end:08X}, {len(rso.internal)} internal and "
          f"{len(rso.external)} external relocations, {len(rso.imports)} imports"
          + (f" ({len(missing)} not exported, left unresolved: {', '.join(missing)})" if missing else ""), file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
