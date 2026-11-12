"""Dump the toy and variant tables a Skylanders game carries on its own disc (docs/runtime/host/collection.md, "The
games' own toy tables"). Reads the player's disc read-only and writes JSON into private/ (game data: never commit it).

usage: python3 -I tools/toy_tables.py <disc.iso> [<out.json> [<image.bin>]]
  default output: private/<game>/toy_tables.json (the game is told by the files on the disc)
  <image.bin> (Swap Force only): the title's loaded image, default private/swapforce/image.bin (tools/xexdump writes it)

Giants [Alchemy]: character/Init_Setup.bld holds the script that defines every toy the game knows ("Portal - Define
Object Level Load": toy type, element, flags), every deco it recognises on a known toy ("Portal - Define Legendary Alt
Level Load": deco id, the subtype's display string), the toy types that are another toy's alternate ("Portal - Define Alt
Character": Legendary Bash, Dark Spyro...) and the toys it shows as LightCore. Strings are external references into
permanent/global.bld's string sets; their English text is in that archive's ENGLISH.pak.
Swap Force [Alchemy]: archives/toydata.pak (IGA v10, big-endian) holds one text "igx" file (XML) per toy family:
CFullCharacterToyData (characters, giants), CShapeshifterPartToyData (a swapper's top, "Body", or bottom, "Legs", with its
movement type), CMagicItemToyData, CBattlePieceToyData, CExpansionLevelToyData, CPetToyData (sidekicks), each with its
CVariantIdentifier list (deco id, year code, Wow Pow, alternate deco and LightCore flags, variant text); English names in
the _en.igx files. Toy types and decos are named by the title's enums (kTfbSpyroTag_ToyType_*, _DecoID_*), whose values
are read from the title's loaded image. The dump gives each variant the id a figure carries (figures.md).
Spyro's Adventure [Lux]: no such table (its accepted toy types are code, docs/runtime/host/figures.md); the dump lists
its toy pictures (uigameimage.str) and character models.

SSA and Giants: "upgrades", per toy type, every upgrade the game shows with its bit in the figure's upgrade field
(block 8 +0x10, docs/runtime/host/figures.md "Upgrades"), path, price and English and French name and description: Giants from each
character's archive (its "Portal - Define Minion" call: the unlock names and descriptions, the two paths) and
global.bld's Minion_UpgradeCosts; SSA from global.str's upgrade records and text/<language>/global.lxb.
"""
import json
import lzma
import os
import re
import struct
import sys
import xml.etree.ElementTree
import zlib

SECTOR = 2048


# --- XDVDFS (the disc) -------------------------------------------------------------------------------------------------
class Disc:
    def __init__(self, path):
        self.f = open(path, "rb")
        for base in (0x0, 0x2080000, 0xFD90000, 0x18300000):
            self.f.seek(base + 32 * SECTOR)
            if self.f.read(20) == b"MICROSOFT*XBOX*MEDIA":
                self.base = base
                break
        else:
            raise SystemExit("not an Xbox 360 disc image (no XDVDFS header)")
        self.f.seek(self.base + 32 * SECTOR + 20)
        sector, size = struct.unpack("<II", self.f.read(8))
        self.files = {}
        self._walk(sector, size, "")

    def _walk(self, sector, size, prefix):
        self.f.seek(self.base + sector * SECTOR)
        data = self.f.read(size)
        stack, seen = [0], set()
        while stack:
            off = stack.pop()
            if off in seen or off + 14 > len(data):
                continue
            seen.add(off)
            left, right, start, length, attr, nlen = struct.unpack_from("<HHIIBB", data, off)
            if left == 0xFFFF and right == 0xFFFF:
                continue
            path = prefix + "/" + data[off + 14:off + 14 + nlen].decode("latin-1")
            stack += [x * 4 for x in (left, right) if x]
            if attr & 0x10:
                if length:
                    self._walk(start, length, path)
            else:
                self.files[path.lower()] = (start, length)

    def read(self, path):
        start, length = self.files[path.lower()]
        self.f.seek(self.base + start * SECTOR)
        return self.f.read(length)


# --- IGA archives: v8 little-endian (Giants), v10 big-endian (Swap Force), v11 little-endian (Trap Team) (game_art.md) -
def iga_entries(blob):
    if blob[:4] == b"IGA\x1a" and struct.unpack_from("<I", blob, 4)[0] == 8:
        count, names_at = struct.unpack_from("<I", blob, 0x0C)[0], struct.unpack_from("<I", blob, 0x1C)[0]
        out = {}
        for i in range(count):
            offset, size, mode = struct.unpack_from("<III", blob, 0x34 + 4 * count + 12 * i)
            at = names_at + struct.unpack_from("<I", blob, names_at + 4 * i)[0]
            out[blob[at:blob.index(b"\0", at)].decode("latin-1")] = (offset, size, mode)
        return out
    if blob[:8] == b"\x1aAGI\x00\x00\x00\x0a":
        # v10 [SwapForce] [V]: everything big-endian. Header 0x38: count at 0x0C, the name table's position at 0x2C and
        # size at 0x30; u32 name hashes[n] (sorted), then n x { u32 ?, u32 offset, u32 size, u32 mode }; chunks start
        # 0x800-aligned (v8: 0x100). Each name is followed by 4 bytes of its own.
        count = struct.unpack_from(">I", blob, 0x0C)[0]
        names_at = struct.unpack_from(">I", blob, 0x2C)[0]
        out = {}
        for i in range(count):
            _, offset, size, mode = struct.unpack_from(">IIII", blob, 0x38 + 4 * count + 16 * i)
            at = names_at + struct.unpack_from(">I", blob, names_at + 4 * i)[0]
            out[blob[at:blob.index(b"\0", at)].decode("latin-1")] = (offset, size, mode, 0x800)
        return out
    if blob[:4] == b"IGA\x1a" and struct.unpack_from("<I", blob, 4)[0] == 11:
        # v11 [TrapTeam] [V]: little-endian, header 0x38, the name table's position (u64) at 0x28 and size at 0x30; u32
        # name hashes[n], then n x { u32 offset, u32 ?, u32 size, u32 mode }; chunks 0x100-aligned as in v8.
        count = struct.unpack_from("<I", blob, 0x0C)[0]
        names_at = struct.unpack_from("<Q", blob, 0x28)[0]
        out = {}
        for i in range(count):
            offset, _, size, mode = struct.unpack_from("<IIII", blob, 0x38 + 4 * count + 16 * i)
            at = names_at + struct.unpack_from("<I", blob, names_at + 4 * i)[0]
            out[blob[at:blob.index(b"\0", at)].decode("latin-1")] = (offset, size, mode)
        return out
    if blob[:8] == b"\x1aAGI\x00\x00\x00\x0b":
        # v11 big-endian [SuperChargers, Imaginators] [V]: v10's entries {?, offset, size, mode}, the name table's position
        # (u64, high word first) at 0x28; chunks 0x800-aligned (docs/runtime/host/game_art.md, "The versions").
        count = struct.unpack_from(">I", blob, 0x0C)[0]
        names_at = struct.unpack_from(">Q", blob, 0x28)[0]
        out = {}
        for i in range(count):
            _, offset, size, mode = struct.unpack_from(">IIII", blob, 0x38 + 4 * count + 16 * i)
            at = names_at + struct.unpack_from(">I", blob, names_at + 4 * i)[0]
            out[blob[at:blob.index(b"\0", at)].decode("latin-1")] = (offset, size, mode, 0x800)
        return out
    raise SystemExit("unexpected archive version (IGA v8, v10 or v11 expected)")


def _inflate_chunk(blob, p, want):
    """[u16 n][n bytes of raw deflate] at p: its bytes if they are exactly `want` long, else None."""
    n = blob[p] | blob[p + 1] << 8 if p + 2 <= len(blob) else 0
    if not n:
        return None
    try:
        out = zlib.decompressobj(-15).decompress(blob[p + 2:p + 2 + n], want)
    except zlib.error:
        return None
    return out if len(out) == want else None


def iga_read(blob, entry):
    offset, size, mode = entry[:3]
    align = entry[3] if len(entry) > 3 else 0x100
    if mode == 0xFFFFFFFF:
        return blob[offset:offset + size]
    out, p = bytearray(), offset
    while len(out) < size:  # chunks of 0x8000 bytes at aligned offsets: [u16 n (LE in both)][LZMA props][n bytes], or stored
        want = min(0x8000, size - len(out))
        n = blob[p] | blob[p + 1] << 8
        if mode >> 28 == 1:
            # Raw deflate [Imaginators]: the next chunk starts at a multiple of 16 after this one, sometimes one to three
            # 16-byte steps later; it is the first whose deflate gives the next chunk's size (game_art.md).
            chunk = _inflate_chunk(blob, p, want)
            if chunk is None:
                raise SystemExit("IGA: a deflate chunk did not unpack")
            out += chunk
            nxt = (p + 2 + n + 15) & ~15
            p = nxt
            left = min(0x8000, size - len(out))
            for step in range(4):
                if left and _inflate_chunk(blob, nxt + 16 * step, left) is not None:
                    p = nxt + 16 * step
                    break
        elif blob[p + 2:p + 7] == b"\x5d\x00\x80\x00\x00":
            raw = lzma.LZMADecompressor(lzma.FORMAT_RAW, filters=[{"id": lzma.FILTER_LZMA1, "dict_size": 0x8000, "lc": 3,
                                                                    "lp": 0, "pb": 2}])
            out += raw.decompress(blob[p + 7:p + 7 + n] + b"\0" * 8, max_length=want)[:want]
            p = (p + 7 + n + align - 1) & ~(align - 1)
        else:
            out += blob[p:p + want]
            p = (p + want + align - 1) & ~(align - 1)
    return bytes(out)


# --- IGZ v6 objects (games/giants/docs/art.md) -------------------------------------------------------------------
class Igz:
    def __init__(self, b):
        self.b, self.pools, o = b, [], 0x10
        if b[:4] != b"IGZ\x01" or self.u32(4) != 6:
            raise SystemExit("unexpected IGZ version (6 expected)")
        sections = []
        while True:
            offset, size = self.u32(o), self.u32(o + 4)
            if offset == 0 or size == 0:
                break
            sections.append((offset, size))
            o += 16
        self.pools = [s[0] for s in sections[1:]]
        base, end = sections[0][0], sections[0][0] + sections[0][1]
        p, self.fixups = base + self.u32(base + 0x0C), {}
        while p + 0x18 <= end:
            kind, count, length, start = self.u32(p), self.u32(p + 12), self.u32(p + 16), self.u32(p + 20)
            if length == 0:
                break
            self.fixups[kind] = (count, b[p + start:p + length])
            p += length
        count, data = self.fixups[0]
        self.types = data.split(b"\0")[:count]
        self.types = [t.decode() for t in self.types]
        count, data = self.fixups[5]
        self.objects, value, shift, at, high = [], 0, 0, 0, []
        for byte in data:
            high += [byte & 15, byte >> 4]
        for nibble in high:
            value |= (nibble & 7) << shift
            shift += 3
            if not nibble & 8:
                at += value * 4 + 4
                self.objects.append(at)
                value = shift = 0
                if len(self.objects) == count:
                    break
        self.known = set(self.objects)
        count, data = self.fixups.get(2, (0, b""))
        self.external = [struct.unpack_from(">II", data, 8 * i) for i in range(count)]

    def u32(self, at):
        return struct.unpack_from(">I", self.b, at)[0]

    def at(self, pointer):
        return self.pools[pointer >> 24] + (pointer & 0xFFFFFF)

    def word(self, obj, index):
        return self.u32(self.at(obj) + 4 * index)

    def kind(self, obj):
        t = self.word(obj, 0)
        return self.types[t] if obj in self.known and t < len(self.types) else None

    def text(self, pointer):
        if pointer == 0 or (pointer >> 24) >= len(self.pools):
            return None
        a = self.at(pointer)
        return self.b[a:self.b.index(b"\0", a)].decode("latin-1")

    def items(self, obj):  # a list object: { type, refs, count, capacity, size, data pointer, ... }
        n, data = self.word(obj, 2), self.word(obj, 5)
        return [self.u32(self.at(data) + 4 * i) for i in range(n)]


class IgzTagged:
    """IGZ v7-v9 [SwapForce] [TrapTeam] [SuperChargers] [V] (docs/runtime/host/game_art.md, "The versions"): sections at
    0x14 as { ?, offset, size, alignment }; section 0 holds records { tag (byte-swapped), count, length, data start }:
    TMET the class names, TSTR the strings objects name by index, RVTB the objects (packed nibbles, the distance in
    4-byte units, minus one before v9), EXID the external ids. A pointer: pool in its top 5 bits, offset below."""

    def __init__(self, b):
        self.b = b
        if b[:4] != b"IGZ\x01" or self.u32(4) not in (7, 8, 9):
            raise SystemExit("unexpected IGZ version (7 to 9 expected)")
        version = self.u32(4)
        sections, o = [], 0x14
        while len(sections) < 16:
            offset, size = self.u32(o + 4), self.u32(o + 8)
            if offset == 0 or size == 0:
                break
            sections.append((offset, size))
            o += 16
        self.pools = [s[0] for s in sections[1:]]
        p, end, self.records = sections[0][0], sum(sections[0]), {}
        while p + 16 <= end:
            tag, count, length, start = struct.unpack_from(">4sIII", b, p)
            if length < 16 or p + length > end or start > length:
                break
            self.records[tag[::-1].decode("latin-1")] = (count, b[p + start:p + length])
            p += length
        align = 1 if version == 7 else 2
        self.types = self._names("TMET", align)
        self.strings = self._names("TSTR", align)
        count, data = self.records["RVTB"]
        self.objects, value, shift, at = [], 0, 0, 0
        for nibble in [n for byte in data for n in (byte & 15, byte >> 4)]:
            value |= (nibble & 7) << shift
            shift += 3
            if not nibble & 8:
                at += value * 4 + (0 if version == 9 else 4)
                self.objects.append(at)
                value = shift = 0
                if len(self.objects) == count:
                    break
        self.known = set(self.objects)

    def _names(self, tag, align):
        count, data = self.records.get(tag, (0, b""))
        out, p = [], 0
        while len(out) < count and p < len(data):
            text = data[p:data.index(b"\0", p)]
            out.append(text.decode("latin-1"))
            p = (p + len(text) + 1 + align - 1) // align * align
        return out

    def u32(self, at):
        return struct.unpack_from(">I", self.b, at)[0]

    def at(self, pointer):
        return self.pools[pointer >> 27] + (pointer & 0x7FFFFFF)

    def word(self, obj, index):
        return self.u32(self.at(obj) + 4 * index)

    def kind(self, obj):
        t = self.word(obj, 0)
        return self.types[t] if obj in self.known and t < len(self.types) else None

    def of_kind(self, name):
        return [o for o in self.objects if self.kind(o) == name]


# --- Giants -----------------------------------------------------------------------------------------------------------
ELEMENTS = ["magic", "water", "air", "undead", "tech", "fire", "earth", "life"]  # the order of UI_ElementName_(n)
# How Init_Setup's external string references are numbered: the build's string groups, in this order, each taking one
# number and then one per string. global.bld keeps them as string sets, some merged (its Item_Loads also lists the
# legendary and location loads, its Minion_Loads the legendary ones): a group is a set minus the sets named after it.
# Checked below against the toy types (a toy's level load starts with its own number).
STRING_GROUPS = [("Item_Loads", ["ItemLoads_Legendary", "Location_Loads"]), ("ItemLoads_Legendary", []), ("Location_Loads", []),
                 ("Item_Names", ["ItemName_Locations"]), ("ItemName_Locations", []), ("Minion_Loads", ["Minion_Loads_Legendary"]),
                 ("Minion_Loads_Legendary", []), ("Minion_Names", []), ("Toy Type Strings", [])]
STRING_NAMESPACE = 0x2694BE8E


def giants_strings(disc):
    """global.bld: its string sets (in STRING_SETS order) and the English text of every string key."""
    blob = disc.read("/permanent/global.bld")
    archive = iga_entries(blob)
    level = Igz(iga_read(blob, archive["level.bld"]))
    keys = {}
    infos = sorted(o for o in level.objects if level.kind(o) == "StringInfo")
    for o in infos:
        keys[o] = level.text(level.word(o, 2))
    # ENGLISH.pak: one LanguagePackInfo whose text block lists every string (UTF-16, NUL-terminated), in the order of
    # the StringInfo objects.
    pak = Igz(iga_read(blob, archive["ENGLISH.pak"]))
    info = next(o for o in pak.objects if pak.kind(o) == "LanguagePackInfo")
    block = pak.at(pak.word(info, 7))
    size = pak.word(info, 6) & 0xFFFFFFF
    texts = pak.b[block:block + size].decode("utf-16-be", errors="replace").split("\0")
    english = {keys[o]: texts[i] for i, o in enumerate(infos) if i < len(texts)}
    sets = {}
    for o in level.objects:
        if level.kind(o) == "ScriptSet":
            name, lst = level.text(level.word(o, 2)), level.word(o, 6)
            if lst in level.known and level.word(lst, 6) == 0x8000000C:
                sets[name] = [keys.get(x) for x in level.items(lst)]
    numbered = []
    for name, minus in STRING_GROUPS:
        if name not in sets or any(m not in sets for m in minus):
            raise SystemExit(f"global.bld: string set {name} not found")
        others = {k for m in minus for k in sets[m]}
        numbered += ["<group " + name + ">"] + [k for k in sets[name] if k not in others]
    return numbered, english


def giants_tables(disc):
    numbered, english = giants_strings(disc)
    blob = disc.read("/character/Init_Setup.bld")
    z = Igz(iga_read(blob, iga_entries(blob)["level.bld"]))

    def constant(obj):  # a literal: ValueRHSVariant -> RHSValueStack with no entries, the value at word 8
        if z.kind(obj) != "ValueRHSVariant":
            return None
        stack = z.word(obj, 2)
        return z.word(stack, 8) if z.kind(stack) == "RHSValueStack" and z.word(stack, 2) == 0 else None

    def external(obj):  # a string reference: RHSReferenceStack whose value is an external id
        if z.kind(obj) != "RHSReferenceStack":
            return None
        handle = z.word(obj, 12)
        if handle >> 24 != 0x80 or (handle & 0xFFFFFF) >= len(z.external):
            return None
        number, namespace = z.external[handle & 0xFFFFFF]
        return number if namespace == STRING_NAMESPACE else None

    # Walk the scripts' op lists: a "use macro" op starts a call, its parameters follow.
    calls = []
    lightcore_refs, loads_temp = [], []
    for obj in z.objects:
        if z.kind(obj) != "OpCodeList":
            continue
        call = None
        for op in z.items(obj):
            kind = z.kind(op)
            if kind == "OpUseMacro":
                call = {}
                calls.append(call)
            elif kind == "OpMacroParameter" and call is not None and not z.word(op, 6) & 0x80000000:
                macro = z.word(op, 14)
                call["macro"] = z.text(z.word(macro, 2)) if macro in z.known else None
                value = constant(z.word(op, 12))
                if value is None:
                    value = external(z.word(op, 12))
                    index = constant(z.word(op, 13)) if z.word(op, 13) in z.known else None
                    if value is not None and index is not None:
                        value = ("indexed", value, index)
                call[z.text(z.word(op, 7))] = value
            elif kind == "OpChangeMembership":
                # "set [loads temp] to ...": the level loads of the LightCore toys are added to a scratch list (mode 1
                # with the list itself empties it), which a loop then turns into the set "display as lightcore".
                target, group = z.word(op, 7), z.word(op, 9)
                variable = z.word(target, 5 + z.word(target, 2)) if z.kind(target) == "SetStack" else 0  # the path's last name
                name = z.text(z.word(variable, 7)) if variable in z.known else None
                if name == "loads temp" and z.kind(group) == "ScriptGroupStack" and z.word(group, 6) == 0x8000000D:
                    handle = z.word(group, 7)
                    if z.word(op, 8) == 1:
                        loads_temp = []
                    elif handle >> 24 == 0x80:
                        loads_temp.append(z.external[handle & 0xFFFFFF][0])
                elif name == "display as lightcore" and not lightcore_refs:
                    lightcore_refs = list(loads_temp)

    defines = [c for c in calls if c.get("macro") == "Portal - Define Object Level Load"]
    first = next(c for c in defines if c.get("item code") == 0)
    offset = first["level load"] - numbered.index("MinionLoad_000_AirDragon")

    def key(value):
        if isinstance(value, tuple):  # an entry of a set: the set's number, then the index in it
            value = value[1] + 1 + value[2]
        i = value - offset if isinstance(value, int) else -1
        return numbered[i] if 0 <= i < len(numbered) else None

    toys, variants, alternates, by_load = [], [], [], {}
    last = None
    for c in calls:
        macro = c.get("macro")
        if macro == "Portal - Define Object Level Load":
            code, load = c.get("item code", 999), key(c.get("level load"))
            name = key(c.get("name")) if "name" in c else None
            element = c.get("element")
            toy = {"type": code, "load": load, "model": english.get(load), "name_key": name,
                   "name": english.get(name) if name else None,
                   "element": ELEMENTS[element] if isinstance(element, int) and element < 8 else None,
                   "magic_item_or_pack": bool(c.get("is magic item", 0)), "sky1": bool(c.get("is sky1 character", 1)),
                   "giant": bool(c.get("is giant", 0)), "series2_repose": bool(c.get("has known repose", 0)),
                   "lightcore": False}
            plain = numbered.index("<group Minion_Loads>") < numbered.index(load) < numbered.index("<group Minion_Loads_Legendary>")
            if plain and load[11:14] != f"{code:03d}":  # MinionLoad_<type>_<model>
                raise SystemExit(f"string numbering check failed: toy {code} loads {load}")
            toys.append(toy)
            by_load[c.get("level load")] = toy
            last = toy
        elif macro == "Portal - Define Legendary Alt Level Load" and last is not None:
            load, shown = key(c.get("level load")), key(c.get("subtype display string")) or "ToyType_Legendary"
            variants.append({"type": last["type"], "deco": c.get("check against deco id"), "load": load,
                             "model": english.get(load), "display_key": shown, "display": english.get(shown)})
        elif macro == "Portal - Define Alt Character":
            normal, alt = by_load.get(c.get("normal toy load")), by_load.get(c.get("alt toy load"))
            prefix = key(c.get("alt prefix")) if "alt prefix" in c else "ToyType_Legendary"
            alternates.append({"type": normal and normal["type"], "alt_type": alt and alt["type"],
                               "prefix_key": prefix, "prefix": english.get(prefix)})
    for ref in lightcore_refs:
        if ref in by_load:
            by_load[ref]["lightcore"] = True
    if not toys or "Polar" not in [v["display"] for v in variants]:
        raise SystemExit("Init_Setup.bld: the toy definitions were not found where expected")

    blob = disc.read("/misc/UI_Collection_Champions.bld")
    portraits = sorted(set(m.decode() for m in re.findall(rb"collections/sky2/portraits/([a-z0-9_]+)\.png",
                                                            iga_read(blob, iga_entries(blob)["level.bld"]))))
    return {"game": "giants", "source": "character/Init_Setup.bld (definitions), permanent/global.bld (strings, ENGLISH.pak), "
            "misc/UI_Collection_Champions.bld (portraits)",
            "variant_bits": "bit 11 repose (Series 2, only for series2_repose toys), bit 10 alternate deco (matched by deco "
            "id against variants), bit 9 LightCore (only for lightcore toys), low byte deco id",
            "toys": toys, "variants": variants, "alternate_types": alternates, "portraits": portraits,
            "upgrades": giants_upgrades(disc, toys)}


# --- Upgrades ---------------------------------------------------------------------------------------------------------
# Where each entry of a toy's upgrade list goes in the figure's upgrade field [V] (both games' menus, figures.md): the
# list is 4 basic upgrades, 3 for each of the two paths, the Soul Gem's, and (Giants, Series 2) the Wow Pow. Bits 0 and 1
# are "a path is chosen" and "it is the second one"; the two paths share bits 6-8.
UPGRADE_SLOTS = [(2, 0), (3, 0), (4, 0), (5, 0), (6, 1), (7, 1), (8, 1), (6, 2), (7, 2), (8, 2), (9, 3), (10, 4)]
# (bit, path): path 0 basic, 1 and 2 the paths, 3 the Soul Gem, 4 the Wow Pow (bit 10 of the editors' working bits: the
# figure keeps it in the extra area, +0x06 bit 0, collection::EditorBits). Price index of each entry in the games'
# price list (Giants' Minion_UpgradeCosts, the same values as SSA's records).
UPGRADE_PRICE_INDEX = [0, 1, 2, 3, 4, 5, 6, 4, 5, 6, 7, 8]
LANGUAGES = {"en": ("ENGLISH", "eng"), "fr": ("FRENCH", "fre")}


def pak_texts(blob, archive, pak):
    z = Igz(iga_read(blob, archive[pak]))
    info = next(o for o in z.objects if z.kind(o) == "LanguagePackInfo")
    block, size = z.at(z.word(info, 7)), z.word(info, 6) & 0xFFFFFFF
    return z.b[block:block + size].decode("utf-16-be", errors="replace").split("\0")


def giants_upgrades(disc, toys):
    blob = disc.read("/permanent/global.bld")
    level = Igz(iga_read(blob, iga_entries(blob)["level.bld"]))
    prices = None
    for o in level.objects:
        if level.kind(o) == "ScriptSet" and level.text(level.word(o, 2)) == "Minion_UpgradeCosts":
            prices = [level.word(v, 8) for v in level.items(level.word(o, 6))]
    if not prices or len(prices) != 9:
        raise SystemExit("global.bld: Minion_UpgradeCosts not found")
    out = {}
    for toy in toys:
        path = "/character/%s.bld" % (toy["model"] or "")
        if toy["magic_item_or_pack"] or toy["type"] == 999 or path.lower() not in disc.files:
            continue
        blob = disc.read(path)
        archive = iga_entries(blob)
        z = Igz(iga_read(blob, archive["level.bld"]))
        infos = sorted(o for o in z.objects if z.kind(o) == "StringInfo")
        texts = {lang: pak_texts(blob, archive, pak + ".pak") for lang, (pak, _) in LANGUAGES.items()}

        def text(o):  # a StringInfo's text in each language: the pak lists them in the order of the StringInfo objects
            i = infos.index(o)
            return {lang: t[i] if i < len(t) else None for lang, t in texts.items()}

        params = {}
        for L in z.objects:  # the toy's "Portal - Define Minion" call
            if z.kind(L) != "OpCodeList":
                continue
            for op in z.items(L):
                if z.kind(op) == "OpMacroParameter" and not z.word(op, 6) & 0x80000000:
                    macro = z.word(op, 14)
                    if macro in z.known and z.text(z.word(macro, 2)) == "Portal - Define Minion":
                        value = z.word(op, 12)
                        if z.kind(value) == "RHSReferenceStack":
                            params[z.text(z.word(op, 7))] = z.word(value, 12)
        names, descriptions = (z.items(z.word(params[k], 6)) for k in ("unlock names", "unlock descriptions"))
        entries = []
        for i, (n, d) in enumerate(zip(names, descriptions)):
            bit, path_ = UPGRADE_SLOTS[i]
            entries.append({"bit": bit, "path": path_, "price": prices[UPGRADE_PRICE_INDEX[i]], "name": text(n),
                            "description": text(d)})
        paths = [{"name": text(params["upgrade path %d name" % k]), "description": text(params["upgrade path %d desc" % k])}
                 for k in (1, 2) if "upgrade path %d name" % k in params]
        out[str(toy["type"])] = {"upgrades": entries, "paths": paths}
    return out


def lxb_texts(data):
    """A Lux text file (text/<language>/*.lxb): CRC-32 of the key -> UTF-8 text."""
    count, out = struct.unpack_from(">I", data, 0x7C)[0], {}
    for i in range(count):
        key, offset = struct.unpack_from(">Ii", data, 0x80 + 8 * i)
        at = 0x84 + 8 * i + offset
        out[key] = data[at:data.index(b"\0", at)].decode("utf-8", errors="replace")
    return out


def ssa_upgrades(disc, names_by_type):
    texts = {lang: lxb_texts(disc.read("/text/%s/global.lxb" % code)) for lang, (_, code) in LANGUAGES.items()}
    english = texts["en"]
    data = disc.read("/global.str")
    stream = zlib.decompress(data[20:])
    # An upgrade record (0x54 bytes, not 4-aligned in the stream): index, name key, description key, price; a toy's 12
    # records follow its header (name key, path 1 and 2 name keys) 0x60 bytes before the first.
    def u32(at):
        return struct.unpack_from(">I", stream, at)[0]
    types_by_name = {name: t for t, name in names_by_type.items()}
    out = {}
    at = 0
    first = struct.Struct(">IIII")
    while True:
        at = stream.find(b"\0\0\0\0", at)
        if at < 0 or at + 0x54 * 12 > len(stream):
            break
        index, name, description, price = first.unpack_from(stream, at)
        if not (index == 0 and name in english and description in english and price == 500 and u32(at - 0x60) in english):
            at += 1
            continue
        toy = english[u32(at - 0x60)]
        records = [first.unpack_from(stream, at + 0x54 * k) for k in range(12)]
        basic = [r for r in records if r[0] < 4]
        path_records = [r for r in records if 4 <= r[0] <= 6]
        soul = [r for r in records if r[0] == 7]
        order = basic + path_records + soul
        if toy in types_by_name and len(order) == 11:
            def text(key):
                return {lang: t.get(key) for lang, t in texts.items()}
            entries = [{"bit": UPGRADE_SLOTS[i][0], "path": UPGRADE_SLOTS[i][1], "price": r[3], "name": text(r[1]),
                        "description": text(r[2])} for i, r in enumerate(order)]
            out.setdefault(str(types_by_name[toy]), {"upgrades": entries, "paths": [
                {"name": text(u32(at - 0x5C)), "description": None}, {"name": text(u32(at - 0x58)), "description": None}]})
        at += 0x54 * 12
    return out


# --- Swap Force -------------------------------------------------------------------------------------------------------
# The toy data: archives/toydata.pak (IGA v10), one text "igx" file (XML) per toy family, <name>_toydata.igx, with its
# English and French strings in <name>_toydata_en.igx / _fr.igx (igLocalizedString: the object and field they replace).
# The igx files name toy types and decos by the title's enums (kTfbSpyroTag_ToyType_Character_Zook); their values are
# the title's own (igMetaEnum tables in the executable: the enum's name, u32 pointers to the member names, then, 8-aligned,
# their s32 values), read from the loaded image tools/xexdump writes (private/<game>/image.bin; base 0x82000000).
IMAGE_BASE = 0x82000000
SF_ELEMENTS = {"eET_Earth": "earth", "eET_Water": "water", "eET_Air": "air", "eET_Fire": "fire", "eET_Life": "life",
               "eET_Death": "undead", "eET_Magic": "magic", "eET_Tech": "tech", "eET_None": "none"}
# eSG_* (no value table: Invalid, Spyro2011, Giants2012, Shapeshifters2013): the variant id's high nibble [V] (real figures)
SF_YEARS = {"eSG_Spyro2011": 0, "eSG_Giants2012": 1, "eSG_Shapeshifters2013": 2}
SF_KINDS = {"CFullCharacterToyData": "character", "CShapeshifterPartToyData": "swapper", "CMagicItemToyData": "magic_item",
            "CBattlePieceToyData": "battle_piece", "CExpansionLevelToyData": "adventure", "CPetToyData": "sidekick"}


def image_enum(image, name):
    """The members and values of one of the title's enums (igMetaEnum: name, member name pointers, 8-aligned values)."""
    def u32(at):
        return struct.unpack_from(">I", image, at)[0]

    def text(pointer):
        at = pointer - IMAGE_BASE
        return image[at:image.index(b"\0", at)].decode("latin-1") if 0 <= at < len(image) else ""
    prefix, names_at = name + "_", None
    for match in re.finditer(re.escape(b"\0" + name.encode() + b"\0"), image):  # the enum's name, then its member table
        end = (match.end() + 3) & ~3
        names_at = next((p for p in range(end, end + 0x10, 4) if text(u32(p)).startswith(prefix)), None)
        if names_at is not None:
            break
    if names_at is None:
        raise SystemExit(f"image: enum {name} not found")
    members = []
    while text(u32(names_at + 4 * len(members))).startswith(prefix):
        members.append(text(u32(names_at + 4 * len(members))))
    values_at = (names_at + 4 * len(members) + 7) & ~7
    return {m: struct.unpack_from(">i", image, values_at + 4 * i)[0] for i, m in enumerate(members)}


def igx_objects(data):
    """An igx file: {refname: (type, {var: value or ref}, [list item refs])}."""
    out = {}
    for o in xml.etree.ElementTree.fromstring(data).iter("object"):
        fields = {v.get("name"): v.get("value", v.get("ref")) for v in o.findall("var")}
        out[o.get("refname")] = (o.get("type"), fields, [i.get("ref") for i in o.findall("listitem")])
    return out


def igx_strings(data):
    """A localisation igx: {(object refname, field): text}."""
    out = {}
    for kind, fields, _ in igx_objects(data).values():
        if kind == "igLocalizedString" and fields.get("_object"):
            out[(fields["_object"].split(".", 1)[-1], fields["_field"].split("::")[-1])] = fields.get("_string")
    return out


def swapforce_tables(disc, image_path):
    if not os.path.exists(image_path):
        raise SystemExit(f"{image_path}: the title's loaded image is needed for its enum values (tools/xexdump "
                         "default.xex private/swapforce writes it; tools/prepare_game.sh does)")
    image = open(image_path, "rb").read()
    toy_types = image_enum(image, "kTfbSpyroTag_ToyType")
    decos = image_enum(image, "kTfbSpyroTag_DecoID")
    if toy_types.get("kTfbSpyroTag_ToyType_Character_Zook") != 25 or decos.get("kTfbSpyroTag_DecoID_LightDirect") != 6:
        raise SystemExit("image: the enum tables are not where expected (is it Swap Force's image?)")

    blob = disc.read("/archives/toydata.pak")
    archive = iga_entries(blob)
    files = {k.split("\\")[-1][:-4]: v for k, v in archive.items() if k.endswith(".igx")}
    toys = []
    for base in sorted(f for f in files if not f.endswith(("_en", "_fr"))):
        objects = igx_objects(iga_read(blob, files[base]))
        english = igx_strings(iga_read(blob, files[base + "_en"])) if base + "_en" in files else {}
        for ref, (kind, f, _) in objects.items():
            if kind not in SF_KINDS:
                continue
            enum = f["_toyId"]
            code = toy_types[enum]
            toy = {"type": code, "enum": enum[len("kTfbSpyroTag_ToyType_"):], "file": base, "object": ref,
                   "class": kind, "kind": SF_KINDS[kind], "name": english.get((ref, "_toyName"), f.get("_toyName")),
                   "element": SF_ELEMENTS.get(f.get("_elementType")), "series": f.get("_seriesName")}
            if kind == "CFullCharacterToyData":
                toy["giant"] = f.get("_isGiant") == "true"
                toy["character"] = f.get("_characterName")
            if kind == "CShapeshifterPartToyData":
                toy["kind"] = "swap_top" if f.get("_partType") == "eSPT_Body" else "swap_bottom"
                toy["part"] = f.get("_partName")
                if f.get("_turboType"):
                    toy["movement"] = f["_turboType"][len("eSTT_"):]
            if f.get("_itemName"):
                toy["item"] = f["_itemName"]
            if 1999 <= code <= 3999 and code % 1000 == 999:
                toy["kind"] = "template"  # Shapeshifter_Leg_Template, _Body_Template, Character_Template: debug toys
            toy["picture"] = f.get("_toyCollectionMaterial")
            toy["portrait"] = f.get("_faceOffPortraitMaterial")
            variants = []
            if f.get("_variants") and f["_variants"].split(".", 1)[-1] in objects:
                for item in objects[f["_variants"].split(".", 1)[-1]][2]:
                    vref = item.split(".", 1)[-1]
                    vf = objects[vref][1]
                    deco = decos[vf["_decoId"]]
                    year = SF_YEARS[vf["_yearCode"]]
                    flags = {k: vf.get(v) == "true" for k, v in (("wow_pow", "_wowPowFlag"), ("alt_deco", "_fullAltDecoFlag"),
                                                                  ("lightcore", "_lightCoreFlag"))}
                    vid = year << 12 | flags["wow_pow"] << 11 | flags["alt_deco"] << 10 | flags["lightcore"] << 9 | deco
                    variants.append({"id": "0x%04X" % vid, "deco": deco, "deco_enum": vf["_decoId"][len("kTfbSpyroTag_DecoID_"):],
                                     "year": vf["_yearCode"][len("eSG_"):], **flags, "object": vref,
                                     "text": vf.get("_variantText"), "name": english.get((vref, "_toyName")),
                                     "picture": vf.get("_toyCollectionMaterial"), "portrait": vf.get("_faceOffPortraitMaterial")})
            toy["variants"] = variants
            toys.append(toy)
    toys.sort(key=lambda t: (t["type"], t["object"]))
    # A swapper's name is its two halves' (Boom + Jet), the halves paired by their part name (Body_Aviator, Legs_Aviator).
    halves = {(t["kind"], t["part"].split("_", 1)[1]): t["name"] for t in toys if t["kind"] in ("swap_top", "swap_bottom")}
    for t in toys:
        if t["kind"] in ("swap_top", "swap_bottom"):
            pair = t["part"].split("_", 1)[1]
            t["swapper"] = "%s %s" % (halves.get(("swap_top", pair)), halves.get(("swap_bottom", pair)))
    defined = {t["type"] for t in toys}
    return {"game": "swapforce", "source": "archives/toydata.pak (toy data, igx), the title's enums (image.bin)",
            "variant_bits": "bits 12-15 year code (eSG: 0 Spyro2011, 1 Giants2012, 2 Shapeshifters2013), bit 11 Wow Pow "
            "(_wowPowFlag: the reposes), bit 10 alternate deco (_fullAltDecoFlag), bit 9 LightCore (_lightCoreFlag), low "
            "byte deco id (kTfbSpyroTag_DecoID)",
            "toys": toys,
            "toy_type_enum": [{"name": k[len("kTfbSpyroTag_ToyType_"):], "value": v, "defined": v in defined}
                              for k, v in toy_types.items()],
            "deco_enum": [{"name": k[len("kTfbSpyroTag_DecoID_"):], "value": v} for k, v in decos.items()]}


# --- SuperChargers and Imaginators ----------------------------------------------------------------------------------
# The element field's values [V] (toys of known element: Shark Tank 1, Dive-Clops 2, Sky Slicer 3, Eruptor 4, Barkley 5,
# Eye-Brawl 6, Spyro 7, Trigger Happy 8, Knight Light 9, Knight Mare 10, Kaos 11).
ALCHEMY9_ELEMENTS = {0: "none", 1: "earth", 2: "water", 3: "air", 4: "fire", 5: "life", 6: "undead", 7: "magic", 8: "tech",
                     9: "light", 10: "dark", 11: "kaos"}


def toydata_tables(disc, game, archives):
    """SuperChargers [Alchemy] [SuperChargers] [V], Imaginators [Imaginators] [V]: archives/permanent.pak holds one binary
    IGZ v9 file per toy family (`ToyData/<model>_ToyData.igz`), the objects of Swap Force's igx files: CFullCharacterToyData,
    CVehicleToyData, CShapeshifterPartToyData, CTrapToyData, CMagicItemToyData, CExpansionLevelToyData, CBattlePieceToyData,
    CLootChestToyData. A toy object: word 2 the toy type, word 3 the element (ALCHEMY9_ELEMENTS), word 4 the name, word 6
    the series text, word 8 the collection picture (an index in TSTR); one of its words points to its CVariantIdentifierList (word 2 the count, word 5 the item array).
    A CVariantIdentifier: word 2 the deco id, word 3 the year code (0 = 2011 ... 5 = 2016), word 4 four flag bytes
    (LightCore, alternate deco, Wow Pow / repose, and a last one [?]: SuperChargers never sets it, Imaginators nearly
    always), word 5 the variant text, word 7 the collection picture (an index in TSTR, top bit set). The variant id a figure
    carries is Swap Force's: year << 12 | Wow Pow << 11 | alternate deco << 10 | LightCore << 9 | deco [V: Spyro 0x0402
    Dark, 0x0403 Legendary, 0x1801 Series 2, 0x3810 Eon's Elite; Eye-Brawl 0x1214 LightCore]."""
    def text(z, pointer):
        if not pointer or pointer >> 27 >= len(z.pools):
            return None
        a = z.at(pointer)
        end = z.b.find(b"\0", a)
        raw = z.b[a:end] if 0 <= a < end else b""
        return raw.decode("utf-8", errors="replace") if 0 < len(raw) < 120 and all(c >= 32 for c in raw) else None

    def tstr(z, value):
        return z.strings[value & 0xFFFFFF] if value >> 31 and (value & 0xFFFFFF) < len(z.strings) else None

    toys = []
    for archive in archives:
        blob = disc.read(archive)
        for path, entry in sorted(iga_entries(blob).items()):
            name = path.replace("\\", "/").rsplit("/", 1)[-1]
            if "/ToyData/" not in path.replace("\\", "/") or not name.endswith("_ToyData.igz"):
                continue
            z = IgzTagged(iga_read(blob, entry))
            objects = sorted(z.objects)
            for n, o in enumerate(objects):
                kind = z.kind(o)
                if not kind or not kind.endswith("ToyData"):
                    continue
                end = objects[n + 1] if n + 1 < len(objects) and objects[n + 1] >> 27 == o >> 27 else o + 0x40
                words = [z.word(o, k) for k in range(min((end - o) // 4, 24))]
                toy = {"type": words[2], "class": kind, "file": name[:-len("_ToyData.igz")],
                       "element": ALCHEMY9_ELEMENTS.get(words[3]), "name": text(z, words[4]),
                       "series": text(z, words[6]) if len(words) > 6 else None,
                       "picture": tstr(z, words[8]) if len(words) > 8 else None, "variants": []}
                for w in words[4:]:
                    if w in z.known and z.kind(w) == "CVariantIdentifierList":
                        count, data = z.word(w, 2), z.word(w, 5)
                        for v in [z.u32(z.at(data) + 4 * k) for k in range(count)]:
                            if z.kind(v) != "CVariantIdentifier":
                                continue
                            deco, year, flags = z.word(v, 2), z.word(v, 3), z.word(v, 4)
                            light, alt, wow, last = flags >> 24 & 1, flags >> 16 & 1, flags >> 8 & 1, flags & 1
                            vid = year << 12 | wow << 11 | alt << 10 | light << 9 | deco
                            toy["variants"].append({"id": "0x%04X" % vid, "deco": deco, "year": year, "lightcore": bool(light),
                                                    "alt_deco": bool(alt), "wow_pow": bool(wow), "last_flag": bool(last),
                                                    "text": text(z, z.word(v, 5)), "picture": tstr(z, z.word(v, 7))})
                        break
                toys.append(toy)
    toys.sort(key=lambda t: (t["type"], t["file"]))
    if not toys or not any(t["type"] == 16 and t["name"] == "Spyro" for t in toys):
        raise SystemExit("permanent.pak: the toy data were not found where expected")
    return {"game": game, "source": "archives/permanent.pak (ToyData/*.igz, IGZ v9)",
            "variant_bits": "bits 12-15 year code, bit 11 Wow Pow (repose), bit 10 alternate deco, bit 9 LightCore, low "
            "byte deco id (Swap Force's)", "toys": toys}


# --- Spyro's Adventure ------------------------------------------------------------------------------------------------
def ssa_tables(disc):
    data = disc.read("/uigameimage.str")
    if data[:4] != b"strm":
        raise SystemExit("uigameimage.str: not a strm file")
    unpacked = zlib.decompress(data[20:])
    names = sorted(set(m.decode() for m in re.findall(rb"\0XET.{44}([a-z0-9_]+)\0", unpacked, re.S)))
    toys = [n for n in names if (n.startswith("toy_") and n.endswith("_hud")) or n.endswith("_vs")]
    models = sorted(p.rsplit("/", 1)[1][:-4] for p in disc.files if p.startswith("/models/characters/minionsmonsters/"))
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    with open(os.path.join(root, "resources", "collection", "catalogue.json")) as f:
        names = {t["type"]: t["name"] for t in json.load(f)["toys"] if t["game"] == "ssa"}
    return {"game": "ssa", "source": "uigameimage.str (toy pictures), models/characters/minionsmonsters (models), "
            "global.str and text/<language>/global.lxb (upgrades)",
            "note": "no toy or variant table in data: the accepted toy types are the title's code (figures.md)",
            "toy_pictures": toys, "character_models": models, "upgrades": ssa_upgrades(disc, names)}


def trapteam_tables(disc):
    """Trap Team [Alchemy] [TrapTeam] [V]: character/Init_Setup.bld's ENGLISH.pak (an IGZ whose text is UTF-16 big-endian)
    holds the game's own tables as comma-separated rows, in this order: the 46 villains (`1001,life,ChompyMage,Chompy
    Mage,5,quest=0`: id, element, model, name, vault index, quest), every toy (`450,air,Typhoon,Gusto,ranger`: type,
    element, model, name, flags), the debug minions and old test toys, the items and adventure packs (`230,Item_HelpingHand,
    Hand of Fate`), the traps (`210,magic,Magic Trap,2,8,11,14,18,21`: type, element, name, the shapes: a trap's variant id
    is 0x3000 + a shape), the figure kinds by year and the variants each toy has (`474,Legendary,Legendary`), the Swap
    Force bots. The villains' and the toys' pictures are the file names of misc/UI_Collection_Champions.arc and the
    villain vault's (`<type>_<variant>_<name>.png`, the variant id a figure carries)."""
    blob = disc.read("/character/Init_Setup.bld")
    entries = iga_entries(blob)
    # An edition without English (the French and Dutch one) carries the same tables in its own languages: the ids and models
    # are the same, the names are that language's.
    language = "ENGLISH.pak" if "ENGLISH.pak" in entries else sorted(n for n in entries if n.endswith(".pak") and n[:-4].isupper())[0]
    pak = iga_read(blob, entries[language])
    rows = [m.group().decode("utf-16-be") for m in re.finditer(rb"(?:\x00[\x20-\x7e]){3,}", pak)]
    elements = {"death": "undead", "kaos": "kaos"}
    tables = {"game": "trapteam", "language": language[:-4].lower(), "source": f"character/Init_Setup.bld ({language}), misc/UI_Collection_Champions.arc, "
              "misc/ui_collection_traps.arc, misc/ui_villainvault_stream.arc", "villains": [], "toys": [], "debug": [],
              "items": [], "traps": [], "variants": [], "bots": []}
    year, debug = None, False
    for row in rows:
        f = row.split(",")
        if re.fullmatch(r"10\d\d", f[0]) and len(f) == 6 and f[5].startswith("quest="):
            tables["villains"].append({"id": int(f[0]), "element": elements.get(f[1], f[1]), "model": f[2], "name": f[3],
                                       "vault": int(f[4]), "quest": int(f[5][6:])})
        elif re.fullmatch(r"21\d|220", f[0]) and len(f) >= 4 and f[1] in ("magic", "water", "air", "death", "tech", "fire", "earth", "life", "dark", "light", "kaos"):
            tables["traps"].append({"type": int(f[0]), "element": elements.get(f[1], f[1]), "name": f[2],
                                    "shapes": [int(x) for x in f[3:]]})
        elif row.startswith("year="):
            year = int(row[5:])
        elif re.fullmatch(r"\d+", f[0]) and len(f) >= 3 and f[1].startswith("Item_"):
            debug = False  # the debug minions and test toys come right before the items
            tables["items"].append({"type": int(f[0]), "model": f[1], "name": f[2], "level": next((x for x in f[3:] if x.startswith("level=")), None)})
        elif re.fullmatch(r"\d+", f[0]) and len(f) >= 4 and f[1] in ("magic", "water", "air", "death", "tech", "fire", "earth", "life", "dark", "light"):
            if f[0] == "9990":
                debug = True
            toy = {"type": int(f[0]), "element": elements.get(f[1], f[1]), "model": f[2], "name": f[3], "flags": f[4:]}
            (tables["debug"] if debug else tables["bots"] if re.fullmatch(r"1\d\d\d|20\d\d", f[0]) and int(f[0]) >= 1000 else tables["toys"]).append(toy)
        elif year is not None and re.fullmatch(r"\d+", f[0]) and len(f) >= 3 and not f[1].startswith("Item_"):
            deco = next((int(x[5:]) for x in f[3:] if x.startswith("deco=")), None)
            tables["variants"].append({"year": year, "type": int(f[0]), "kind": f[1].strip(), "name": f[2], "deco": deco})
    # The pictures: the game's own file names.
    names = []
    for archive in ("/misc/UI_Collection_Champions.arc", "/misc/ui_collection_traps.arc", "/misc/ui_villainvault_stream.arc"):
        blob = disc.read(archive)
        for name in iga_entries(blob):
            m = re.search(r"collection_images/(skylander_toys/\d{4}/\w+|traps/square|villains/(?:captured|escaped))/([\w-]+)\.png/", name, re.I)
            if m:
                names.append(m[1].split("/")[0] + "/" + m[2])
    tables["pictures"] = sorted(set(names))
    if len(tables["villains"]) != 46 or not tables["traps"] or not tables["toys"]:
        raise SystemExit("Init_Setup.bld: Trap Team's tables were not found where expected")
    return tables


def check_accepts(tables, root):
    """Giants' own accepted toy types (its "Portal - Define Object Level Load" calls; 999 is a debug minion) against what the
    catalogue says Giants accepts (the toys of SSA and Giants). The game has no toy-type check in its code (the PPC):
    the table is the check. Returns the differences."""
    with open(os.path.join(root, "resources", "collection", "catalogue.json")) as f:
        catalogue = json.load(f)
    own = {t["type"] for t in tables["toys"]} - {999}
    accepted = {t["type"] for t in catalogue["toys"] if t["game"] in ("ssa", "giants")}
    return sorted(own - accepted), sorted(accepted - own)


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    disc = Disc(sys.argv[1])
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if "/character/init_setup.bld" in disc.files and struct.unpack_from("<I", disc.read("/character/Init_Setup.bld")[:8], 4)[0] == 11:
        tables = trapteam_tables(disc)
    elif "/character/init_setup.bld" in disc.files:
        tables = giants_tables(disc)
    elif "/archives/toydata.pak" in disc.files:
        image = sys.argv[3] if len(sys.argv) > 3 else os.path.join(root, "private", "swapforce", "image.bin")
        tables = swapforce_tables(disc, image)
    elif "/uigameimage.str" in disc.files:
        tables = ssa_tables(disc)
    elif "/archives/permanent_2016.pak" in disc.files:
        tables = toydata_tables(disc, "imaginators", ["/archives/permanent.pak"])
    elif "/archives/maincollection_2015.pak" in disc.files:
        tables = toydata_tables(disc, "superchargers", ["/archives/permanent.pak"])
    else:
        raise SystemExit("not one of the six games' discs")
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(root, "private", tables["game"], "toy_tables.json")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w") as f:
        json.dump(tables, f, indent=1)
    counts = {k: len(v) for k, v in tables.items() if isinstance(v, list)}
    print(f"{tables['game']}: {counts} -> {out}")
    if tables["game"] == "giants":
        missing, extra = check_accepts(tables, root)
        print("accepts check: " + ("the catalogue's Giants toys are exactly the disc's" if not missing and not extra else
                                   f"DIFFERENT: on the disc only {missing}, in the catalogue only {extra}"))
        if missing or extra:
            raise SystemExit(1)


if __name__ == "__main__":
    main()
