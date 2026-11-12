"""Find PPC jump tables in a dumped XEX image and emit XenonRecomp's switch-table TOML.

XenonAnalyse matches fixed instruction sequences, which this game's compiler doesn't produce
(reordered slwi/addi, nops interleaved). Instead, for every `bctr` we locate the bounds check
(`cmplwi crN, rI, N` + `bgt crN`) and concretely simulate the instructions between it and the
`bctr` for every index 0..N, reading table data from the image.

It also computes explicit `functions` boundaries for code outside .pdata that XenonRecomp's
analyzer gets wrong: leaf functions containing a jump table (it stops at the bctr), adapters
ending in `b <next function>` (it runs into the next function), and code only reached through
function pointers in data (vtables) that the analyzer merged into a neighbour.

usage: python3 -I tools/find_switch_tables.py private/image.bin private/xexdump.txt private/functions.toml \
           [private/ssa/ppc/ppc_func_mapping.cpp] > games/ssa/config/SSA_switch_tables.toml
The optional mapping from a previous XenonRecomp run excludes pointer targets that already are functions.
"""
import bisect
import re
import struct
import sys

BASE = 0x82000000
BCTR = 0x4E800420
BLR = 0x4E800020
MASK32 = 0xFFFFFFFF


def sign16(v):
    return v - 0x10000 if v & 0x8000 else v


def rotl32(v, n):
    return ((v << n) | (v >> (32 - n))) & MASK32 if n else v


def rlw_mask(mb, me):
    m = 0
    for bit in range(32):
        if (mb <= me and mb <= bit <= me) or (mb > me and (bit >= mb or bit <= me)):
            m |= 1 << (31 - bit)
    return m


def branch_target(addr, ins):
    """Target of a relative b/bl/bc at addr, or None."""
    op = ins >> 26
    if op == 18 and not ins & 2:
        off = ins & 0x03FFFFFC
        return addr + (off - 0x04000000 if off & 0x02000000 else off)
    if op == 16 and not ins & 2:
        off = ins & 0xFFFC
        return addr + (off - 0x10000 if off & 0x8000 else off)
    return None


class Image:
    def __init__(self, path):
        self.data = open(path, "rb").read()

    def u8(self, a):
        return self.data[a - BASE]

    def u16(self, a):
        return struct.unpack_from(">H", self.data, a - BASE)[0]

    def u32(self, a):
        return struct.unpack_from(">I", self.data, a - BASE)[0]


class Unknown(Exception):
    pass


def simulate(img, insns, regs):
    """Executes the handful of integer ops used by jump-table dispatch. Returns CTR."""
    def get(r):
        if r not in regs:
            raise Unknown(f"r{r} unknown")
        return regs[r]

    for addr, ins in insns:
        op = ins >> 26
        rd, ra, rb = (ins >> 21) & 31, (ins >> 16) & 31, (ins >> 11) & 31
        if op in (36, 38, 44, 48, 50, 52, 54):  # stw/stb/sth, lfs/lfd, stfs/stfd: no GPR written
            continue
        if op in (32, 34, 40, 42, 58):  # lwz/lbz/lhz/lha/ld from non-image memory
            regs.pop(rd, None)
            continue
        if op == 14:  # addi / li
            regs[rd] = ((get(ra) if ra else 0) + sign16(ins & 0xFFFF)) & MASK32
        elif op == 15:  # addis / lis
            regs[rd] = ((get(ra) if ra else 0) + (sign16(ins & 0xFFFF) << 16)) & MASK32
        elif op == 24:  # ori (nop is ori r0,r0,0)
            if ins != 0x60000000:
                regs[ra] = get(rd) | (ins & 0xFFFF)
        elif op == 21:  # rlwinm
            sh, mb, me = (ins >> 11) & 31, (ins >> 6) & 31, (ins >> 1) & 31
            regs[ra] = rotl32(get(rd), sh) & rlw_mask(mb, me)
        elif op == 31:
            xo = (ins >> 1) & 0x3FF
            if xo == 23:  # lwzx
                regs[rd] = img.u32(((get(ra) if ra else 0) + get(rb)) & MASK32)
            elif xo == 279:  # lhzx
                regs[rd] = img.u16(((get(ra) if ra else 0) + get(rb)) & MASK32)
            elif xo == 343:  # lhax
                regs[rd] = sign16(img.u16(((get(ra) if ra else 0) + get(rb)) & MASK32)) & MASK32
            elif xo == 87:  # lbzx
                regs[rd] = img.u8(((get(ra) if ra else 0) + get(rb)) & MASK32)
            elif xo == 266:  # add
                regs[rd] = (get(ra) + get(rb)) & MASK32
            elif xo == 444:  # or / mr
                regs[ra] = get(rd) | get(rb)
            elif xo == 467 and (ra | (rb << 5)) == 9:  # mtctr
                return get(rd)
            else:
                raise Unknown(f"op31 xo={xo} at {addr:#x}")
        else:
            raise Unknown(f"op{op} at {addr:#x}")
    raise Unknown("no mtctr")


def find_bounds_check(img, bctr_addr, text_start):
    """Walks back from the bctr to `cmplwi crN, rI, N` guarding it with `bgt crN` (or `ble crN, dispatch; b default`).
    Returns (index register, N, first instruction of the dispatch block, default label, aliases)."""
    a = bctr_addr - 4
    branch = None
    while a >= max(text_start, bctr_addr - 4 * 128):
        ins = img.u32(a)
        op = ins >> 26
        if op == 18 or ins == BLR:
            previous = img.u32(a - 4)
            bo, bi = (previous >> 21) & 31, (previous >> 16) & 31
            if branch is None and op == 18 and not ins & 3 and previous >> 26 == 16 and bo == 4 and bi % 4 == 1 \
                    and branch_target(a - 4, previous) == a + 4:
                branch = (a + 4, bi // 4, branch_target(a, ins))  # ble crN, dispatch; b default
                a -= 8
                continue
            return None  # unconditional branch / blr: left the dispatch block
        if branch is None and (op == 16 or (op == 19 and (ins >> 1) & 0x3FF == 16)):
            bo, bi = (ins >> 21) & 31, (ins >> 16) & 31
            if bo == 12 and bi % 4 == 1:  # bgt crN, or bgtlr crN (default case returns)
                branch = (a + 4, bi // 4, branch_target(a, ins) or 0)
        elif branch is not None and op == 10 and (ins >> 23) & 7 == branch[1]:  # cmplwi crN
            reg = (ins >> 16) & 31
            return reg, ins & 0xFFFF, branch[0], branch[2], find_aliases(img, a, reg)
        a -= 4
    return None


def written_register(ins):
    """GPR an integer instruction writes, if any (enough for the instruction mix around dispatch code)."""
    op = ins >> 26
    if op in (14, 15, 32, 33, 34, 35, 40, 41, 42, 43, 58):
        return (ins >> 21) & 31
    if op in (20, 21, 23, 24, 25, 26, 27, 28, 29, 30):
        return (ins >> 16) & 31
    if op == 31:
        xo = (ins >> 1) & 0x3FF
        if xo in (23, 87, 279, 343, 266, 40, 8, 10, 136, 138, 202, 234, 104, 200, 235, 459, 491, 75, 11, 21, 55, 119, 311, 375, 53):
            return (ins >> 21) & 31
        if xo in (444, 28, 60, 124, 284, 316, 412, 476, 24, 536, 792, 824, 922, 954, 986, 26, 27, 539, 794):
            return (ins >> 16) & 31
    return None


def find_distant_bounds_check(img, bctr_addr, text_start):
    """Dispatch blocks reached from several paths keep their bounds check far upstream: take the block after the
    last branch, find the one register it reads without defining (the index), and walk back to its `cmplwi`."""
    start = bctr_addr - 4
    while start > text_start and img.u32(start - 4) >> 26 not in (16, 18, 19):
        start -= 4

    defined, inputs = set(), set()
    for a in range(start, bctr_addr, 4):
        ins = img.u32(a)
        op = ins >> 26
        reads = []
        if op in (14, 15) and (ins >> 16) & 31:
            reads = [(ins >> 16) & 31]
        elif op in (21, 24):
            reads = [(ins >> 21) & 31]
        elif op == 31:
            reads = [(ins >> 16) & 31, (ins >> 11) & 31] if (ins >> 1) & 0x3FF != 467 else [(ins >> 21) & 31]
        inputs |= {r for r in reads if r and r not in defined}
        if (w := written_register(ins)) is not None:
            defined.add(w)
    if len(inputs) != 1:
        return None

    reg = inputs.pop()
    for a in range(start - 4, max(text_start, start - 4 * 400), -4):
        ins = img.u32(a)
        if ins >> 26 == 10 and (ins >> 16) & 31 == reg:
            return reg, ins & 0xFFFF, start, 0, set()
        if written_register(ins) == reg:
            return None
    return None


def find_aliases(img, cmp_addr, reg):
    """Registers copied from/to the index right before the bounds check (`mr r31, r4; cmplwi r4`)."""
    aliases = set()
    for a in range(cmp_addr - 4, cmp_addr - 4 * 8, -4):
        ins = img.u32(a)
        rs, ra, rb = (ins >> 21) & 31, (ins >> 16) & 31, (ins >> 11) & 31
        if ins >> 26 == 31 and (ins >> 1) & 0x3FF == 444 and rs == rb and reg in (rs, ra):  # mr ra, rs
            other = ra if rs == reg else rs
            between = [img.u32(b) for b in range(a + 4, cmp_addr, 4)]
            if not any((i >> 21) & 31 == other or (i >> 16) & 31 == other for i in between):
                aliases.add(other)
    return aliases


def find_switch_tables(img, text):
    found, failures = [], []
    for start, size in text:
        for bctr in range(start, start + size, 4):
            if img.u32(bctr) != BCTR:
                continue
            check = find_bounds_check(img, bctr, start) or find_distant_bounds_check(img, bctr, start)
            if check is None:
                continue  # indirect call / vtable dispatch, not a jump table
            reg, count, window_start, default, aliases = check
            window = [(a, img.u32(a)) for a in range(window_start, bctr, 4)]
            indexed_load = any(ins >> 26 == 31 and (ins >> 1) & 0x3FF in (23, 87, 279, 343) for _, ins in window)
            if not indexed_load:
                continue  # e.g. a vtable call after an unrelated compare
            written = {(ins >> 21) & 31 for _, ins in window if ins >> 26 in (14, 15, 32, 34, 40, 42, 58)} | \
                      {(ins >> 16) & 31 for _, ins in window if ins >> 26 in (21, 24) and ins != 0x60000000}
            try:
                if reg in written:
                    raise Unknown(f"index register r{reg} is overwritten before bctr")
                labels = [simulate(img, window, {r: i for r in {reg} | aliases}) for i in range(count + 1)]
                bad = [l for l in labels if l & 3 or not any(s <= l < s + n for s, n in text)]
                if bad:
                    raise Unknown(f"labels outside code: {[hex(b) for b in bad[:4]]}")
                found.append((bctr, reg, default, labels))
            except (Unknown, struct.error, IndexError) as e:
                failures.append((bctr, str(e)))
    return found, failures


def find_inline_tables(img, text, known):
    """Tables right after their bctr (`lis/addi rT, bctr+4; slwi r0, rI, 2; lwzx r0, rT, r0; mtctr r0; bctr`) whose
    bounds check is far before or absent (masked index): Giants' Bink 2 decoder and Alchemy render code. The table ends
    at the first word that is not a nearby code address after it."""
    found = []
    for start, n in text:
        for bctr in range(start + 32, start + n, 4):
            if img.u32(bctr) != BCTR or bctr in known:
                continue
            hi = lo = index = target_reg = None
            for k in range(1, 9):
                ins = img.u32(bctr - 4 * k)
                op = ins >> 26
                if op == 14 and lo is None:
                    lo, target_reg = sign16(ins & 0xFFFF), (ins >> 16) & 31
                elif op == 15 and lo is not None and hi is None and (ins >> 21) & 31 == target_reg and (ins >> 16) & 31 == 0:
                    hi = (ins & 0xFFFF) << 16
                elif op == 21 and index is None and (ins >> 11) & 31 == 2 and (ins >> 6) & 31 == 0 and (ins >> 1) & 31 == 29:
                    index = (ins >> 21) & 31
            if hi is None or index is None or (hi + lo) & MASK32 != bctr + 4:
                continue
            labels, entry = [], bctr + 4
            while True:
                label = img.u32(entry)
                if label & 3 or not bctr - 0x3000 < label < bctr + 0x3000 or bctr < label <= entry:
                    break
                labels.append(label)
                entry += 4
            if labels:
                found.append((bctr, index, labels[0], labels))
    return found


def find_function_boundaries(img, text, pdata_section, switches, known_starts, data_sections):
    """Explicit boundaries for non-.pdata code XenonRecomp's analyzer can't size."""
    base, size = pdata_section
    pdata = []
    for a in range(base, base + size, 8):
        begin, packed = img.u32(a), img.u32(a + 4)
        if begin:
            pdata.append((begin, ((packed >> 8) & 0x3FFFFF) * 4, packed >> 31))
    pdata.sort()
    starts = [p[0] for p in pdata]

    bl_targets = set()
    for start, n in text:
        for a in range(start, start + n, 4):
            ins = img.u32(a)
            if ins >> 26 == 18 and ins & 1:
                bl_targets.add(branch_target(a, ins))

    def gap_around(addr):
        """[start, end) of the non-.pdata region containing addr, without padding/EH data."""
        i = bisect.bisect_right(starts, addr) - 1
        if i >= 0 and addr < pdata[i][0] + pdata[i][1]:
            return None
        if i + 1 >= len(pdata):  # past the last .pdata function: no closing neighbour to size it against
            return None
        start = pdata[i][0] + pdata[i][1] if i >= 0 else addr
        end = pdata[i + 1][0] - (8 if pdata[i + 1][2] else 0)
        while img.u32(start) == 0:
            start += 4
        while img.u32(end - 4) == 0:
            end -= 4
        return start, end, pdata[i + 1][0]

    # Leaf functions without .pdata packed into one gap (vtable stubs, accessors): the analyzer makes one function of
    # them, so the ones only reached through pointers built at run time are missing. A gap splits after every
    # unconditional terminator (code after one is only part of the same function if a branch reaches it), then pieces
    # a branch or jump table crosses are joined again.
    switch_edges = [(bctr, label) for bctr, _, _, labels in switches for label in labels]
    packed_leaves = []
    for (b0, s0, _), (b1, _, eh1) in zip(pdata, pdata[1:]):
        gap_start, gap_end = b0 + s0, b1 - (8 if eh1 else 0)
        if gap_end <= gap_start or not any(s <= gap_start < s + n for s, n in text):
            continue
        pieces, a = [], gap_start
        while a < gap_end:
            while a < gap_end and img.u32(a) == 0:
                a += 4
            if a >= gap_end:
                break
            piece_start = a
            while a < gap_end:
                ins = img.u32(a)
                a += 4
                if ins == BLR or ins == BCTR or (ins >> 26 == 18 and not ins & 1):
                    break
            pieces.append([piece_start, a])
        if len(pieces) < 2:
            continue
        edges = [(source, target) for source, target in switch_edges if gap_start <= source < gap_end]
        for b in range(gap_start, gap_end, 4):
            ins = img.u32(b)
            target = branch_target(b, ins)
            if target is not None and not ins & 1:
                edges.append((b, target))
        edges = [(source, target) for source, target in edges if gap_start <= target < gap_end]
        merged = True
        while merged:
            merged = False
            for i in range(len(pieces) - 1):
                split = pieces[i + 1][0]
                if any((source < split) != (target < split) for source, target in edges):
                    pieces[i:i + 2] = [[pieces[i][0], pieces[i + 1][1]]]
                    merged = True
                    break
        last = img.u32(pieces[-1][1] - 4)
        if not (last == BLR or last == BCTR or (last >> 26 == 18 and not last & 1)):
            pieces.pop()  # runs into data or the next function: leave it to the analyzer
        # With a previous run's mapping, only gaps where the analyzer missed a function change.
        if len(pieces) >= 2 and (not known_starts or any(start not in known_starts for start, _ in pieces)):
            packed_leaves += pieces
    leaf_starts = {start for start, _ in packed_leaves}

    # Seeds: dispatch instructions whose forward targets the analyzer doesn't follow.
    seeds = [(bctr, labels) for bctr, _, _, labels in switches]
    switch_labels = {label for _, _, _, labels in switches for label in labels}
    for start, n in text:
        for a in range(start, start + n, 4):
            ins = img.u32(a)
            target = branch_target(a, ins)
            # bdz/bdnz chains (`mtctr; bdz case0; bdz case1; ...`) are small switches on CTR.
            if ins >> 26 == 16 and not ins & 1 and not (ins >> 21) & 0x4 and target is not None and target > a:
                seeds.append((a, [target]))

    functions = {}
    for seed, labels in seeds:
        gap = gap_around(seed)
        if gap is None:
            continue
        gap_start, gap_end, _ = gap
        entries = sorted({gap_start} | {t for t in bl_targets | leaf_starts if gap_start <= t < gap_end})
        fn_start = entries[bisect.bisect_right(entries, seed) - 1]
        limit = next((e for e in entries if e > max(labels)), gap_end)
        # Furthest-reachable scan: stop at the first unconditional terminator past the seed and every known target.
        reach, fn_end = max(max(labels), seed), limit
        for a in range(fn_start, limit, 4):
            ins = img.u32(a)
            target = branch_target(a, ins)
            if target is not None and not ins & 1 and a < target < limit:
                reach = max(reach, target)
            if a >= reach and (ins == BLR or ins == BCTR or (ins >> 26 == 18 and not ins & 1)):
                fn_end = a + 4
                break
        functions[fn_start] = max(functions.get(fn_start, 0), fn_end - fn_start)

    # Adapters: non-.pdata code whose last instruction is `b` to the following .pdata function.
    for (b0, s0, _), (b1, _, eh1) in zip(pdata, pdata[1:]):
        end = b1 - (8 if eh1 else 0)
        if end <= b0 + s0 or branch_target(end - 4, img.u32(end - 4)) != b1 or img.u32(end - 4) & 1:
            continue
        gap_start, gap_end, _ = gap_around(end - 4)
        entries = sorted({gap_start} | {t for t in bl_targets | leaf_starts if gap_start <= t < gap_end})
        fn_start = entries[bisect.bisect_right(entries, end - 4) - 1]
        functions.setdefault(fn_start, end - fn_start)

    # Code reached only through function pointers stored in data (vtables), outside .pdata.
    for start, size in data_sections:
        for a in range(start, start + size - 3, 4):
            target = img.u32(a)
            if target & 3 or not any(s <= target < s + n for s, n in text):
                continue
            if target in known_starts or target in functions or target in switch_labels or gap_around(target) is None:
                continue
            gap_start, gap_end, _ = gap_around(target)
            limit = min([e for e in starts if e > target] + [gap_end])
            reach, fn_end = target, limit
            for b in range(target, limit, 4):
                ins = img.u32(b)
                branch = branch_target(b, ins)
                if branch is not None and not ins & 1 and b < branch < limit:
                    reach = max(reach, branch)
                if b >= reach and (ins == BLR or ins == BCTR or (ins >> 26 == 18 and not ins & 1)):
                    fn_end = b + 4
                    break
            functions[target] = fn_end - target

    for start, end in packed_leaves:
        if not any(f <= start < f + n for f, n in functions.items()):
            functions[start] = end - start

    return functions


def main():
    img = Image(sys.argv[1])
    text, data, pdata_section, thunks = [], [], None, []
    for line in open(sys.argv[2], encoding="latin-1"):
        parts = line.split()
        if parts and parts[0] == "import":
            thunks.append(int(parts[1], 16))
        if parts and parts[0] == "section":
            section = (int(parts[-4], 16), int(parts[-3], 16))
            if parts[-1] == "2":
                text.append(section)
            elif parts[1] == ".pdata":
                pdata_section = section
            elif parts[1] in (".rdata", ".data"):
                data.append(section)

    known_starts = set()
    if len(sys.argv) > 4:
        known_starts = {int(a, 16) for a in re.findall(r"\{ 0x([0-9A-F]+), ", open(sys.argv[4]).read())}

    found, failures = find_switch_tables(img, text)
    found += find_inline_tables(img, text, {bctr for bctr, _, _, _ in found})
    failures = [(bctr, why) for bctr, why in failures if bctr not in {f[0] for f in found}]

    print("# Generated by tools/find_switch_tables.py")
    for bctr, reg, default, labels in found:
        print("[[switch]]")
        print(f"base = 0x{bctr:X}")
        print(f"r = {reg}")
        print(f"default = 0x{default:X}")
        print("labels = [")
        for label in labels:
            print(f"    0x{label:X},")
        print("]\n")

    functions = find_function_boundaries(img, text, pdata_section, found, known_starts, data)
    # Import thunks (16 bytes each, filled by the loader) are XenonRecomp's imports, never functions of their own: a
    # function entry over one makes the recompiled code call the empty thunk instead of the runtime's import (Swap
    # Force: its thunks are reached through data, so they looked like vtable targets).
    functions = {a: n for a, n in functions.items() if not any(a < t + 16 and t < a + n for t in thunks)}
    # A pointer into the EH data words before a .pdata function sizes to nothing or less (Imaginators: a data word equal
    # to 0x83240984, the FuncInfo word of the handler pair before 0x83240988): that is no code.
    functions = {a: n for a, n in functions.items() if n > 0}
    with open(sys.argv[3], "w") as out:
        out.write("# Generated by tools/find_switch_tables.py: paste into SSA.toml [main]\n")
        out.write("functions = [\n")
        for address, size in sorted(functions.items()):
            out.write(f"    {{ address = 0x{address:X}, size = 0x{size:X} }},\n")
        out.write("]\n")

    print(f"{len(found)} jump tables, {len(failures)} unresolved, {len(functions)} explicit functions",
          file=sys.stderr)
    for bctr, why in failures:
        print(f"  unresolved bctr at 0x{bctr:X}: {why}", file=sys.stderr)


if __name__ == "__main__":
    main()
