"""Quick static queries over the dumped game image, for identifying functions without symbols.

Function starts come from XenonRecomp's ppc/ppc_func_mapping.cpp; a function spans up to the next start.

usage: python3 -I tools/ppcinfo.py [--game giants] <command> [args]
  --game <g>                another game's files: private/<g>/{image.bin,xexdump.txt,ppc/}, games/<g>/config/*_symbols.txt
  callers <addr|import>     functions that `bl` to an address or import thunk (e.g. VdSwap)
  callees <addr>            calls made by the function containing addr
  strings <addr>            C strings referenced (lis/addi pairs) by the function containing addr
  dis <addr> [count]        disassemble the function containing addr (needs capstone, no VMX128)
  asm <addr>                the function's instructions as XenonRecomp disassembled them (handles VMX128)
  find-string <text>        functions referencing strings that contain text
"""
import bisect
import glob
import os
import re
import site
import struct
import sys

BASE = 0x82000000
ROOT = __file__.rsplit("/tools/", 1)[0]


class Program:
    def __init__(self, game=None):
        data = f"{ROOT}/private/{game}" if game else f"{ROOT}/private"
        ppc = f"{ROOT}/private/{game or 'ssa'}/ppc"
        symbols = next(iter(glob.glob(f"{ROOT}/games/{game or 'ssa'}/config/*_symbols.txt")), "")
        self.image = open(f"{data}/image.bin", "rb").read()
        self.imports, self.sections = {}, []
        for line in open(f"{data}/xexdump.txt", encoding="latin-1"):
            parts = line.split()
            if parts and parts[0] == "import":
                self.imports[int(parts[1], 16)] = parts[2].replace("__imp__", "")
            elif parts and parts[0] == "section":
                self.sections.append((parts[1], int(parts[-4], 16), int(parts[-3], 16), parts[-1] == "2"))
        self.ppc = ppc
        mapping = open(f"{ppc}/ppc_func_mapping.cpp").read()
        self.starts = sorted(int(a, 16) for a in re.findall(r"\{ 0x([0-9A-F]+), ", mapping))
        self.names = {address: name for address, name in self.imports.items()}
        for line in open(symbols) if os.path.exists(symbols) else []:
            parts = line.split("#", 1)[0].split()
            if len(parts) == 2:
                self.names[int(parts[0], 16)] = parts[1]

    def u32(self, address):
        return struct.unpack_from(">I", self.image, address - BASE)[0]

    def function_of(self, address):
        i = bisect.bisect_right(self.starts, address) - 1
        start = self.starts[i]
        end = self.starts[i + 1] if i + 1 < len(self.starts) else start + 4
        return start, end

    def code(self):
        for name, start, size, is_code in self.sections:
            if is_code:
                yield from range(start, start + size, 4)

    def calls(self):
        """(call site, target) for every `bl`."""
        for address in self.code():
            ins = self.u32(address)
            if ins >> 26 == 18 and ins & 3 == 1:
                offset = ins & 0x03FFFFFC
                yield address, address + (offset - 0x04000000 if offset & 0x02000000 else offset)

    def c_string(self, address):
        if not BASE <= address < BASE + len(self.image):
            return None
        raw = self.image[address - BASE: address - BASE + 120].split(b"\0", 1)[0]
        if len(raw) >= 4 and all(32 <= b < 127 or b in (9, 10) for b in raw):
            return raw.decode()
        return None

    def strings_in(self, start, end):
        """Strings built with `lis rX, hi` followed by `addi rY, rX, lo` in [start, end)."""
        his = {}
        for address in range(start, end, 4):
            ins = self.u32(address)
            op, rd, ra, imm = ins >> 26, (ins >> 21) & 31, (ins >> 16) & 31, ins & 0xFFFF
            if op == 15 and ra == 0:
                his[rd] = (imm << 16) & 0xFFFFFFFF
            elif op == 14 and ra in his:
                text = self.c_string((his[ra] + (imm - 0x10000 if imm & 0x8000 else imm)) & 0xFFFFFFFF)
                if text is not None:
                    yield address, text

    def name(self, address):
        return self.names.get(address, f"sub_{address:08X}")


def parse_address(program, text):
    for address, name in program.imports.items():
        if name == text:
            return address
    return int(text, 16)


def main():
    game = None
    if sys.argv[1] == "--game":
        game = sys.argv[2]
        del sys.argv[1:3]
    program = Program(game)
    command, args = sys.argv[1], sys.argv[2:]

    if command == "callers":
        target = parse_address(program, args[0])
        sites = [call_site for call_site, called in program.calls() if called == target]
        for function in sorted({program.function_of(s)[0] for s in sites}):
            print(f"{program.name(function)}  ({sum(program.function_of(s)[0] == function for s in sites)} calls)")
    elif command == "callees":
        start, end = program.function_of(int(args[0], 16))
        for call_site, called in program.calls():
            if start <= call_site < end:
                print(f"  {call_site:08X}: bl {program.name(called)}")
    elif command == "strings":
        start, end = program.function_of(int(args[0], 16))
        for address, text in program.strings_in(start, end):
            print(f"  {address:08X}: {text!r}")
    elif command == "find-string":
        needle = args[0].lower()
        for name, start, size, is_code in program.sections:
            if not is_code:
                continue
            for i, function in enumerate(program.starts):
                if not start <= function < start + size:
                    continue
                end = program.starts[i + 1] if i + 1 < len(program.starts) else function + 4
                for address, text in program.strings_in(function, end):
                    if needle in text.lower():
                        print(f"{program.name(function)}  {address:08X}: {text!r}")
    elif command == "dis":
        sys.path.append(site.getusersitepackages())
        import capstone
        md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
        md.skipdata = True
        start, end = program.function_of(int(args[0], 16))
        if len(args) > 1:
            end = min(end, start + 4 * int(args[1]))
        print(f"{program.name(start)}:")
        for ins in md.disasm(program.image[start - BASE: end - BASE], start):
            comment = ""
            if ins.mnemonic in ("bl", "b") and ins.op_str.startswith("0x"):
                comment = f"  ; {program.name(int(ins.op_str, 16))}"
            print(f"  {ins.address:08X}: {ins.mnemonic:8s} {ins.op_str}{comment}")
    elif command == "asm":
        start, _ = program.function_of(int(args[0], 16))
        marker = f"PPC_FUNC_IMPL(__imp__sub_{start:08X}) {{"
        for path in glob.glob(f"{program.ppc}/ppc_recomp.*.cpp"):
            text = open(path).read()
            begin = text.find(marker)
            if begin < 0:
                continue
            body = text[begin: text.index("\n}\n", begin)]
            print(f"{program.name(start)}:")
            for line in body.splitlines():
                line = line.strip()
                if line.startswith("// "):
                    print(f"    {line[3:]}")
                elif line.startswith("loc_"):
                    print(line)
            break
    else:
        print(__doc__)


if __name__ == "__main__":
    main()
