"""Lists the guest registers a translated function reads before it writes them, once XenonRecomp keeps them as locals.

    python3 -I tools/xenonrecomp/local_reads.py <ppc dir> [--all | --toml]

With the *_as_local options (docs/tools/toolchain/xenonrecomp.md) a register kept as a local starts at zero in every
function: a function that reads one before writing it on some path expected its caller's value, which locals no longer
carry (a function the analyser split in two, a helper with its own convention). A forward "assigned on every path" pass
over each function's blocks (labels, gotos, returns) finds those reads, per register and per CR / XER field. Left out
unless --all, because they are what every function does: the prologue's saves of the non-volatile registers to its
stack frame (std, stfd, stvx through r1), mfcr into r12 for the non-volatile CR fields, XER.SO copied by a compare, and
a call that falls into a label (below). A function XenonRecomp keeps in the context (an exception funclet, which reads
its parent's r12 and registers) has no locals, so nothing to report.
Prints one line per function and register, then the totals. Exit code 0 whatever it finds: the lines are for review.
--toml prints the config's `context_functions` instead: every function with such a read, also following the edge
after a call that falls into a label (a cold function kept in the context costs nothing, a missed one breaks the game);
XenonRecomp translates those with every register in the context and its callers store and reload their locals around
the call (docs/tools/toolchain/xenonrecomp.md, "Registers as locals").
"""
import collections
import os
import re
import sys

GPR = ["r0", "r2", "r11", "r12"] + ["r%d" % i for i in range(14, 32)]
FPR = ["f0"] + ["f%d" % i for i in range(14, 32)]
VR = ["v%d" % i for i in list(range(14, 64)) + list(range(64, 128))]
CR = ["cr%d" % i for i in range(8)]
LOCALS = set(GPR + FPR + VR + CR + ["ctr", "xer", "reserved"])
TOKEN = re.compile(r"(?<![\w.>])(r\d+|f\d+|v\d+|cr\d|ctr|xer|reserved)\b(?:\.(\w+))?")
FUNC = re.compile(r"^PPC_FUNC_IMPL\(__imp__(\w+)\) \{")
LABEL = re.compile(r"^(loc_[0-9A-F]+):")
GOTO = re.compile(r"goto (loc_[0-9A-F]+);")
CALL = re.compile(r"^(sub_[0-9A-F]+|__imp__\w+)\(ctx, base\);$|^PPC_CALL_INDIRECT_FUNC\(")
CR_FIELDS = ("lt", "gt", "eq", "so")
XER_FIELDS = ("ca", "ov", "so")
STACK_SAVE = re.compile(r"^PPC_STORE_U(32|64)\(ctx\.r1\.u32 \+ -?\d+,|^simde_mm_store_si128\(\(simde__m128i\*\)\(base \+ \(\(ctx\.r1\.u32")


def keys(name, field):
    """The analysis' units: a whole register, or one field of a CR field / XER."""
    if name.startswith("cr"):
        return [name + "." + f for f in (CR_FIELDS if field not in CR_FIELDS else (field,))]
    if name == "xer":
        return ["xer." + f for f in (XER_FIELDS if field not in XER_FIELDS else (field,))]
    return [name]


def split(stmt):
    """(written keys, read keys) of one statement."""
    if re.match(r"^((\w+) = ctx\.\2; ctx\.\2 = saved_\2; ?)+\}$", stmt):  # the locals read back after a context call
        return [k for name in re.findall(r"(\w+) = ctx\.", stmt) for k in keys(name, None)], []
    tokens = [(m.group(1), m.group(2), m.start(), m.end()) for m in TOKEN.finditer(stmt) if m.group(1) in LOCALS]
    writes = []
    lhs_end = 0
    m = re.match(r"^(\w+)(?:\.(\w+))?(?:\[\d+\])?\s*(\|=|&=|\^=|\+=|-=|=)(?!=)", stmt)
    if m and m.group(1) in LOCALS:
        if m.group(3) == "=":
            writes = keys(m.group(1), m.group(2))
            lhs_end = m.end()
    m = re.match(r"^(cr\d)\.(?:compare|setFromMask)", stmt)
    if m and m.group(1) in LOCALS:
        writes = keys(m.group(1), None)
        lhs_end = len(m.group(0))
    m = re.match(r"^simde_mm_store_(?:si128|ps)\((?:\((?:simde__m128i|float)\*\))?(v\d+)\.\w+,", stmt)
    if m and m.group(1) in LOCALS:
        writes = [m.group(1)]
        lhs_end = m.end()
    reads = []
    for name, field, start, _ in tokens:
        if start < lhs_end:
            continue
        # A compare reads only XER.SO (into the CR field's so).
        reads += ["xer.so"] if name == "xer" and field is None and ".compare" in stmt else keys(name, field)
    return writes, reads


def benign(stmt, key):
    if STACK_SAVE.search(stmt) or re.match(r"^\{ (auto saved_(\w+) = ctx\.\2; ctx\.\2 = \2; ?)+$", stmt):
        return True  # a save to the stack frame; the locals stored around a call to a context function
    if key == "xer.so" and (".compare" in stmt or re.match(r"^cr\d\.so = xer\.so;", stmt)):
        return True
    return key.startswith("cr") and stmt.startswith("r12.u64 =")


def analyse(name, body, show_all, every_edge, out):
    blocks = [[None, []]]
    for line in body:
        s = line.strip()
        if not s or s.startswith(("//", "PPCRegister ", "PPCVRegister ", "PPCCRRegister ", "PPCXERRegister ")):
            continue  # the locals' declarations: zero, not the caller's value
        if s == "}" and blocks[-1][1][-1:] == ["__builtin_unreachable();"]:
            blocks.append([None, []])  # after a switch table's "default: unreachable": no fall-through
            continue
        m = LABEL.match(s)
        if m:
            blocks.append([m.group(1), []])
            s = s[m.end():].strip()
            if not s:
                continue
        blocks[-1][1].append(s)
    index = {b[0]: i for i, b in enumerate(blocks) if b[0]}
    succ = []
    for i, (_, stmts) in enumerate(blocks):
        targets, falls = set(), True
        for s in stmts:
            targets.update(index[t] for t in GOTO.findall(s) if t in index)
            if re.match(r"^(goto loc_[0-9A-F]+;|return;|__builtin_trap\(\);|__builtin_unreachable\(\);)$", s):
                falls = False
        # A call that falls into a label: if the register is unset only on that edge, the callee does not return
        # (an assert's fatal error; compiled code never reads a register it did not set). Not followed unless --all.
        after_call = bool(stmts) and CALL.match(stmts[-1]) is not None and blocks[i + 1][0] is not None if i + 1 < len(blocks) else False
        if falls and i + 1 < len(blocks) and (show_all or every_edge or not after_call):
            targets.add(i + 1)
        succ.append(targets)
    assigned = [None] * len(blocks)
    assigned[0] = frozenset()
    work, reported = [0], set()
    while work:
        i = work.pop()
        cur = set(assigned[i])
        for s in blocks[i][1]:
            writes, reads = split(s)
            for k in reads:
                if k in cur or k in reported:
                    continue
                if benign(s, k) and not show_all:
                    continue
                reported.add(k)
                out.append((name, k, s[:150]))
            cur.update(writes)
        for j in succ[i]:
            new = frozenset(cur) if assigned[j] is None else assigned[j] & frozenset(cur)
            if new != assigned[j]:
                assigned[j] = new
                work.append(j)


def main():
    ppc = sys.argv[1]
    show_all = "--all" in sys.argv
    toml = "--toml" in sys.argv
    out = []
    for f in sorted(os.listdir(ppc)):
        if not re.match(r"ppc_recomp\.\d+\.cpp$", f):
            continue
        name, body = None, []
        with open(os.path.join(ppc, f)) as src:
            for line in src:
                m = FUNC.match(line)
                if m:
                    name, body = m.group(1), []
                elif name and line.startswith("}"):
                    if not name.startswith(("__save", "__rest")):  # never called: their calls are left out
                        analyse(name, body, show_all, toml, out)
                    name = None
                elif name:
                    body.append(line)
    if toml:
        names = sorted({n for n, _, _ in out})
        print("context_functions = [")
        for i in range(0, len(names), 8):
            print("    " + ", ".join("0x" + n[4:] for n in names[i:i + 8]) + ",")
        print("]")
        return
    for name, k, s in out:
        print("%s %s: %s" % (name, k, s))
    counts = collections.Counter(k.split(".")[0] for _, k, _ in out)
    print("functions: %d; per register: %s" % (len({n for n, _, _ in out}), dict(counts.most_common())))


main()
