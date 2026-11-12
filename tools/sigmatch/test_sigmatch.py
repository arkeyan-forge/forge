"""Checks tools/sigmatch on SSA's own image (private/image.bin, from tools/prepare_game.sh):
  make      the signatures are regenerated from the image (private/giants/ssa_sigs.json) and the committed
            docs/giants/ssa_hooks.json must not change (it holds no game bytes: a hex signature in it fails the test)
  self      every entry of docs/giants/ssa_hooks.json is found at its SSA address
  operands  every bl target and lis-built address changed (another link), plus struct offsets in the hooked functions:
            each code entry is still found at its address
  shifted   blocks of nops inserted through .text, and one instruction inserted inside every other hooked function:
            each hook is found at its moved address

usage: python3 -I tools/sigmatch/test_sigmatch.py
"""
import json
import os
import re
import random
import struct
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sigmatch  # noqa: E402

ROOT = sigmatch.ROOT
HOOKS = f"{ROOT}/docs/giants/ssa_hooks.json"
SIGS = f"{ROOT}/private/giants/ssa_sigs.json"
CODE_KINDS = ("hook", "replaced", "patch", "config", "config_function", "symbol")
HOOK_KINDS = ("hook", "replaced", "patch")


def run(image, entries, expected, name, kinds):
    path = os.path.join(tempfile.mkdtemp(prefix="sigmatch-test-"), "image.bin")
    with open(path, "wb") as f:
        f.write(image)
    devnull = open(os.devnull, "w")
    stdout, sys.stdout = sys.stdout, devnull
    try:
        out = sigmatch.match(HOOKS, path, None, False, SIGS)
    finally:
        sys.stdout = stdout
    checked = [o for o in out if o["kind"] in kinds]
    # a wrong address is acceptable only when flagged (an entry with several copies in SSA, one of them changed)
    wrong = [o for o in checked if o["address"] != f"0x{expected(int(o['ssa'], 16)):08X}" and o["confidence"] >= 0.5]
    confidences = sorted(o["confidence"] for o in checked)
    low = sum(1 for c in confidences if c < 0.9)
    print(f"{name}: {len(checked) - len(wrong)}/{len(checked)} right, confidence min {confidences[0]} "
          f"median {confidences[len(confidences) // 2]}, {low} under 0.9")
    for o in wrong[:10]:
        print(f"  wrong: {o['name']} {o['ssa']} -> {o['address']} ({o['confidence']}, {o['method']})")
    return not wrong


def main():
    data, base, code = sigmatch.load_image(f"{ROOT}/private/image.bin")
    before = open(HOOKS).read()
    sigmatch.make(HOOKS, f"{ROOT}/private/image.bin", f"{ROOT}/private/ssa/ppc/ppc_func_mapping.cpp", SIGS)
    clean = open(HOOKS).read() == before and not re.search(r"[0-9a-f]{32}", before)
    print(f"make: committed metadata unchanged and without bytes: {clean}")
    entries = json.load(open(HOOKS))["entries"]
    random.seed(1)
    ok = clean
    ok &= run(data, entries, lambda a: a, "self", CODE_KINDS + ("data", "config_data"))

    # operands: another link moves every callee and global
    image = bytearray(data)
    for lo, hi in code:
        for off in range(lo - base, hi - base, 4):
            w = struct.unpack_from(">I", image, off)[0]
            op = w >> 26
            if op == 18 and w & 1:
                w = (w & 0xFC000003) | ((w + random.randrange(1, 4096) * 4) & 0x03FFFFFC)
            elif op == 15 and (w >> 16) & 31 == 0 and 0x8200 <= w & 0xFFFF < 0x8300:
                w = (w & 0xFFFF0000) | ((w + random.randrange(1, 8)) & 0xFFFF)
            struct.pack_into(">I", image, off, w)
    for e in entries:  # struct offsets of the hooked functions change too (half of them)
        if e["kind"] in HOOK_KINDS and random.random() < 0.5:
            a = int(e["ssa"], 16) - base
            for k in range(4, 24):
                w = struct.unpack_from(">I", image, a + 4 * k)[0]
                if w >> 26 in (32, 36) and (w >> 16) & 31 == 3:  # lwz/stw off(r3): a field of the first argument
                    struct.pack_into(">I", image, a + 4 * k, (w & 0xFFFF0000) | ((w + 8) & 0xFFFF))
    ok &= run(bytes(image), entries, lambda a: a, "operands", CODE_KINDS)

    # shifted: nop blocks through .text, plus one instruction inside every other hooked function
    nop = struct.pack(">I", 0x60000000)
    lo, hi = code[0]
    inserts = {lo + (hi - lo) * i // 5 & ~3: 0x400 * i for i in range(1, 5)}
    hooks = sorted(int(e["ssa"], 16) for e in entries if e["kind"] in HOOK_KINDS)
    for i, a in enumerate(hooks):
        if i % 2 == 0:
            inserts[a + 4 * 10] = inserts.get(a + 4 * 10, 0) + 4
    image, last = bytearray(), base
    for at in sorted(inserts):
        image += data[last - base:at - base] + nop * (inserts[at] // 4)
        last = at
    image += data[last - base:]

    def moved(a):
        return a + sum(n for at, n in inserts.items() if at <= a)
    ok &= run(bytes(image), entries, moved, "shifted", HOOK_KINDS)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
