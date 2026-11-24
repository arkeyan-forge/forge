"""Find the jump tables of a Wii game's ELF (the 750CL's compilers) and emit XenonRecomp's switch-table TOML.

A switch is compiled as a bounds check, the table's address in two halves, the index times four, a load and a jump:

    cmplwi crN, rI, N        bgt crN, default
    lis rT, table@ha         rlwinm rO, rI, 2, 0, 29     addi rT, rT, table@l
    lwzx rX, rT, rO          mtctr rX                    bctr

in that order, with other instructions between them. Each `bctr` after an `lwzx` and an `mtctr` is matched backwards
inside its function (the ELF's function symbols), each register followed on its own: the table's base may be set once
before a loop, the index may be scaled in place. Its N + 1 labels are read from the table. A loop over the cases keeps
the offset in a register of its own (`li rO, 4`, then `addi rO, rO, 4` with the counter) and checks the counter: with
no `rlwinm`, an `lwzx` whose table is a `lis` and `addi` pair, after a bounds check on any register, whose N + 1 entries
are all labels in the function, is that loop's switch, its offset register scaled in place. A `bctr` after a plain
load, or after an `lwzx` with no bounds check (a call through a member pointer), is a call through a pointer, which
XenonRecomp already translates. A switch it cannot match is reported on stderr and the tool fails.

usage: python3 -I tools/elf_switch_tables.py private/<game>.elf > games/<id>/config/<Game>_switch_tables.toml
       python3 -I tools/elf_switch_tables.py --self-test
"""
import bisect
import struct
import sys

BCTR = 0x4E800420


def sign16(value):
    return value - 0x10000 if value & 0x8000 else value


class Elf:
    def __init__(self, path=None, data=None):
        self.data = open(path, "rb").read() if data is None else data
        if self.data[:4] != b"\x7fELF" or self.data[5] != 2:
            raise SystemExit(f"{path}: not a big-endian ELF")
        shoff = self.u32_at(0x20)
        entry_size, count = struct.unpack_from(">HH", self.data, 0x2E)
        self.sections = [struct.unpack_from(">10I", self.data, shoff + i * entry_size) for i in range(count)]
        # (address, end, file offset) of the sections with file contents (SHT_PROGBITS).
        self.loaded = sorted((s[3], s[3] + s[5], s[4]) for s in self.sections if s[1] == 1 and s[3] != 0)
        self.code = [(s[3], s[3] + s[5], s[4]) for s in self.sections if s[1] == 1 and s[2] & 4]
        self.functions = []
        for s in self.sections:
            if s[1] != 2:  # SHT_SYMTAB
                continue
            for i in range(s[5] // 16):
                _, value, size, info, _, _ = struct.unpack_from(">IIIBBH", self.data, s[4] + i * 16)
                if info & 0xF == 2 and size:  # STT_FUNC
                    self.functions.append((value, value + size))
        self.functions.sort()
        self.starts = [f[0] for f in self.functions]

    def u32_at(self, offset):
        return struct.unpack_from(">I", self.data, offset)[0]

    def u32(self, address):
        for start, end, offset in self.loaded:
            if start <= address and address + 4 <= end:
                return self.u32_at(offset + address - start)
        raise KeyError(address)

    def function(self, address):
        i = bisect.bisect_right(self.starts, address) - 1
        if i >= 0 and address < self.functions[i][1]:
            return self.functions[i]
        return None


def fields(word):
    return word >> 26, (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 1) & 0x3FF


def find_back(words, start, test):
    """The first (index, word) at or after `start` in `words` (newest first) for which test(word) holds."""
    for i in range(start, len(words)):
        if test(words[i][1]):
            return i
    return None


def match(elf, bctr):
    """The switch ending at `bctr`: (index register, default, labels); None for a call through a pointer."""
    fn = elf.function(bctr)
    if fn is None:
        raise ValueError("outside every function")
    words = [(a, elf.u32(a)) for a in range(bctr - 4, fn[0] - 4, -4)]  # newest first
    if len(words) < 2:
        return None
    op, rs, ra, rb, xo = fields(words[0][1])
    if not (op == 31 and xo == 467 and (ra | rb << 5) == 9):  # mtctr rX
        return None
    op, rt, ra, rb, xo = fields(words[1][1])
    if not (op == 31 and xo == 23 and rt == rs):  # lwzx rX, rT, rO
        return None
    table_register, offset_register = ra, rb
    # The index: rlwinm rO, rI, 2, 0, 29, then the bounds check cmplwi crN, rI, N before it. None: a call.
    shift = find_back(words, 2, lambda w: w >> 26 == 21 and (w >> 16) & 31 == offset_register)
    if shift is None:
        return loop_switch(elf, fn, words, table_register, offset_register)
    w = words[shift][1]
    if (w >> 11) & 31 != 2 or (w >> 6) & 31 != 0 or (w >> 1) & 31 != 29:
        raise ValueError(f"index scaled otherwise at {words[shift][0]:#x}")
    index = (w >> 21) & 31
    check = find_back(words, shift + 1, lambda w: w >> 26 == 10 and (w >> 16) & 31 == index and not (w >> 21) & 1)
    if check is None:
        return None
    bound, default = bounds(words, check)
    table = table_address(words, table_register)
    if table is None:
        raise ValueError("table address not found")
    labels = [elf.u32(table + 4 * i) for i in range(bound + 1)]
    for label in labels:
        if not fn[0] <= label < fn[1]:
            raise ValueError(f"label {label:#x} outside the function")
    if index == offset_register:
        return offset_register, default, scaled(labels, bound, default)
    return index, default, labels


def bounds(words, check):
    """The bounds check at words[check]: its N, and the default the branch right after it goes to (or None)."""
    bound = words[check][1] & 0xFFFF
    default = None
    if check >= 1 and words[check - 1][1] >> 26 == 16:
        target = words[check - 1][1] & 0xFFFC
        default = words[check - 1][0] + (target - 0x10000 if target & 0x8000 else target)
    return bound, default


def table_address(words, table_register):
    """addi rT, rB, lo, and lis rB, hi before it (rB may be rT, or a register set once for a loop); None without."""
    add = find_back(words, 2, lambda w: w >> 26 == 14 and (w >> 21) & 31 == table_register)
    if add is None:
        return None
    base_register = (words[add][1] >> 16) & 31
    high = find_back(words, add + 1, lambda w: w >> 26 == 15 and (w >> 21) & 31 == base_register and not (w >> 16) & 31)
    if high is None or base_register == 0:
        return None
    return ((words[high][1] & 0xFFFF) << 16) + sign16(words[add][1] & 0xFFFF) & 0xFFFFFFFF


def scaled(labels, bound, default):
    """Scaled in place: at the bctr the register holds the index times four, so every fourth case is a label and the
    others (never reached) the default."""
    filler = default if default is not None else labels[0]
    return [labels[i // 4] if i % 4 == 0 else filler for i in range(4 * bound + 1)]


def loop_switch(elf, fn, words, table_register, offset_register):
    """A loop's switch (the offset in a register of its own), or None for a call through a pointer."""
    table = table_address(words, table_register)
    check = find_back(words, 2, lambda w: w >> 26 == 10 and not (w >> 21) & 1)
    if table is None or check is None:
        return None
    bound, default = bounds(words, check)
    try:
        labels = [elf.u32(table + 4 * i) for i in range(bound + 1)]
    except KeyError:
        return None
    if not all(fn[0] <= label < fn[1] for label in labels):
        return None
    return offset_register, default, scaled(labels, bound, default)


def scan(elf):
    """(switches, calls through a pointer, problems) of every bctr in the ELF's code."""
    switches, calls, problems = [], 0, []
    for start, end, offset in elf.code:
        for address in range(start, end, 4):
            if elf.u32_at(offset + address - start) != BCTR:
                continue
            try:
                found = match(elf, address)
            except (ValueError, KeyError) as error:
                problems.append(f"bctr at {address:#x}: {error}")
                continue
            if found is None:
                calls += 1
            else:
                switches.append((address,) + found)
    return switches, calls, problems


def synthetic_elf(text_base, words, data_base, data_words, functions):
    """A big-endian ELF: .text, .data and STT_FUNC symbols (address, size)."""
    text = b"".join(struct.pack(">I", w) for w in words)
    data = b"".join(struct.pack(">I", w) for w in data_words)
    symbols = b"\0" * 16 + b"".join(struct.pack(">IIIBBH", 0, a, n, 0x12, 0, 1) for a, n in functions)
    body = text + data + symbols
    text_off, data_off, sym_off = 52, 52 + len(text), 52 + len(text) + len(data)
    sections_off = 52 + len(body)

    def section(kind, flags, address, offset, size, entry=0):
        return struct.pack(">10I", 0, kind, flags, address, offset, size, 0, 0, 4, entry)

    sections = (section(0, 0, 0, 0, 0) + section(1, 6, text_base, text_off, len(text)) +
                section(1, 3, data_base, data_off, len(data)) + section(2, 0, 0, sym_off, len(symbols), 16))
    header = b"\x7fELF\x01\x02\x01" + b"\0" * 9 + struct.pack(">HHIIIIIHHHHHH", 2, 20, 1, text_base, 0,
                                                                 sections_off, 0, 52, 0, 0, 40, 4, 0)
    return header + body + sections


def self_test():
    def ins(op, a, b, c, rest):
        return op << 26 | a << 21 | b << 16 | rest | c << 11

    lis = lambda rt, hi: 15 << 26 | rt << 21 | hi
    addi = lambda rt, ra, lo: 14 << 26 | rt << 21 | ra << 16 | (lo & 0xFFFF)
    cmplwi = lambda ra, n: 10 << 26 | ra << 16 | n
    bgt = lambda offset: 16 << 26 | 12 << 21 | 1 << 16 | (offset & 0xFFFC)
    rlwinm = lambda ra, rs: 21 << 26 | rs << 21 | ra << 16 | 2 << 11 | 0 << 6 | 29 << 1
    lwzx = lambda rt, ra, rb: ins(31, rt, ra, rb, 23 << 1)
    mtctr = lambda rs: 31 << 26 | rs << 21 | 9 << 16 | 467 << 1
    blr = 0x4E800020
    text, data = 0x80001000, 0x80002000
    # 1: the usual order, the index in r4. 2: the table's base set before a loop, the index scaled in place in r0.
    # 3: a call through a member pointer: an lwzx, no bounds check. 4: a loop's switch: the offset in r15, the counter
    # in r17 checked.
    one = [cmplwi(4, 2), bgt(0x20), lis(5, 0x8000), rlwinm(6, 4), addi(5, 5, 0x2000), lwzx(7, 5, 6), mtctr(7), BCTR,
           blr, blr, blr]
    two_base = text + 4 * len(one)
    two = [lis(29, 0x8000), 0x60000000, 0x60000000, cmplwi(0, 1), bgt(0x18), addi(3, 29, 0x200C), rlwinm(0, 0),
           lwzx(3, 3, 0), mtctr(3), BCTR, blr, blr]
    three_base = two_base + 4 * len(two)
    three = [lwzx(12, 3, 12), mtctr(12), BCTR]
    four_base = three_base + 4 * len(three)
    li = lambda rt, n: addi(rt, 0, n)
    four = [li(15, 4), lis(21, 0x8000), cmplwi(17, 1), bgt(0x14), addi(3, 21, 0x2014), lwzx(3, 3, 15), mtctr(3), BCTR,
            blr, blr]
    labels_one = [text + 0x20, text + 0x24, text + 0x28]
    labels_two = [two_base + 0x28, two_base + 0x2C]
    labels_four = [four_base + 0x20, four_base + 0x24]
    elf = Elf(data=synthetic_elf(text, one + two + three + four, data, labels_one + labels_two + labels_four,
                                 [(text, 4 * len(one)), (two_base, 4 * len(two)), (three_base, 4 * len(three)),
                                  (four_base, 4 * len(four))]))
    switches, calls, problems = scan(elf)
    expected = [(text + 0x1C, 4, text + 0x24, labels_one),
                (two_base + 0x24, 0, two_base + 0x28, [labels_two[0], two_base + 0x28, two_base + 0x28,
                                                       two_base + 0x28, labels_two[1]]),
                (four_base + 0x1C, 15, four_base + 0x20, [labels_four[0], four_base + 0x20, four_base + 0x20,
                                                          four_base + 0x20, labels_four[1]])]
    checks = [switches == expected, calls == 1, problems == []]
    print(f"elf_switch_tables self-test: {'PASS' if all(checks) else 'FAIL'} ({sum(checks)} of {len(checks)})")
    if not all(checks):
        print(switches, calls, problems)
    return 0 if all(checks) else 1


def main():
    if sys.argv[1:] == ["--self-test"]:
        return self_test()
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    elf = Elf(sys.argv[1])
    switches, calls, problems = scan(elf)
    for problem in problems:
        print(problem, file=sys.stderr)
    print(f"{len(switches)} jump tables, {calls} calls through a pointer, {len(problems)} not matched", file=sys.stderr)
    print("# Generated by tools/elf_switch_tables.py")
    for base, index, default, labels in switches:
        print("[[switch]]")
        print(f"base = 0x{base:08X}")
        print(f"r = {index}")
        if default is not None:
            print(f"default = 0x{default:08X}")
        print("labels = [")
        for label in labels:
            print(f"    0x{label:08X},")
        print("]")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
