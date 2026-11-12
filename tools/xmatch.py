"""Find a reference game's functions in another game's executable (same XDK libraries, other build).

The reference is a checkout of this repo with SSA's files (private/image.bin, private/xexdump.txt, private/ssa/ppc/, games/ssa/config/SSA_symbols.txt);
the target is this tree's private/<game>/ (tools/ppcinfo.py --game). `<tree>:giants` takes another game of that tree as the
reference (its private/giants/ files and games/giants/config symbols, if any), e.g. `.:giants`. Three methods, from strongest to weakest:

usage: python3 -I tools/xmatch.py <reference tree>[:<ref game>] <game> callgraph [<ref addr>=<addr> ...] [--find <ref addr> ...]
           seeds = the imports (matched by name) plus the pairs given; matched functions whose callee lists have the
           same length and the same imports at the same places match position by position, unique callers too.
           Prints every named reference function matched; --find ranks candidates for the given reference functions
           by how many of their (matched) callees they call, then by size.
       python3 -I tools/xmatch.py <reference tree> <game> shape <ref addr> ...
           instruction-shape similarity (opcode + extended opcode, no registers or immediates), best 4 candidates.
       python3 -I tools/xmatch.py <reference tree> <game> offsets <ref addr>=<addr> ...
           aligns matched functions instruction by instruction and lists the D-form displacements that differ
           (structure offsets that moved between the two builds, e.g. the D3D device's).
"""
import collections
import difflib
import importlib.util
import struct
import sys

BASE = 0x82000000
HERE = __file__.rsplit("/tools/", 1)[0]


def load(root, game):
    spec = importlib.util.spec_from_file_location("ppcinfo", f"{HERE}/tools/ppcinfo.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.ROOT = root
    return module.Program(game)


def graph(program):
    callees, callers = collections.defaultdict(list), collections.defaultdict(list)
    for site, target in program.calls():
        function = program.function_of(site)[0]
        callees[function].append((site, target))
        callers[target].append(function)
    for function in callees:
        callees[function].sort()
    return callees, callers


def size(program, function):
    start, end = program.function_of(function)
    return end - start


def words(program, function):
    start, end = program.function_of(function)
    return [program.u32(a) for a in range(start, end, 4)]


def shape(word):
    op = word >> 26
    if op in (4, 5, 6, 19, 31, 59, 63):
        return (op << 11) | (word & 0x7FF)
    if op in (16, 18):
        return (op << 11) | (word & 3)
    return op << 11


def callgraph(ref, new, args):
    pairs = [a for a in args if "=" in a]
    find = [int(a, 16) for a in args[args.index("--find") + 1:]] if "--find" in args else []
    rc, rr = graph(ref)
    nc, nr = graph(new)
    match = {int(a, 16): int(b, 16) for a, b in (p.split("=") for p in pairs)}
    by_name = {name: address for address, name in new.imports.items()}
    match.update({a: by_name[n] for a, n in ref.imports.items() if n in by_name})
    changed = True
    while changed:
        changed = False
        for rf, nf in list(match.items()):
            rl = [t for _, t in rc.get(rf, [])]
            nl = [t for _, t in nc.get(nf, [])]
            if rl and len(rl) == len(nl) and all(ref.imports.get(a, "") == new.imports.get(b, "") for a, b in zip(rl, nl)):
                for a, b in zip(rl, nl):
                    if a not in match and b not in match.values():
                        match[a] = b
                        changed = True
            r_callers, n_callers = sorted(set(rr.get(rf, []))), sorted(set(nr.get(nf, [])))
            if len(r_callers) == 1 and len(n_callers) == 1 and r_callers[0] not in match and n_callers[0] not in match.values():
                match[r_callers[0]] = n_callers[0]
                changed = True
    for a, b in sorted(match.items()):
        if a in ref.names and a not in ref.imports:
            print(f"{ref.names[a]:40s} ref {a:08X}  new {b:08X}")
    calls = collections.defaultdict(set)
    for function, sites in nc.items():
        calls[function].update(t for _, t in sites)
    for rf in find:
        mapped = {match[t] for _, t in rc.get(rf, []) if t in match}
        ranked = sorted((-len(mapped & cs) + abs(size(new, f) - size(ref, rf)) / 64, f, len(mapped & cs))
                        for f, cs in calls.items() if len(mapped & cs) >= max(2, len(mapped) - 2))
        print(f"{ref.name(rf)} {rf:08X} size {size(ref, rf):X}, {len(mapped)} callees matched: " +
              ", ".join(f"{f:08X} (size {size(new, f):X}, {n} common)" for _, f, n in ranked[:4]))


def shapes(ref, new, args):
    def grams(body):
        return collections.Counter(tuple(body[i:i + 4]) for i in range(len(body) - 3))
    text = [(s, n) for _, s, n, code in new.sections if code]
    bodies = {f: [shape(w) for w in words(new, f)] for f in new.starts if any(s <= f < s + n for s, n in text)}
    counted = {f: grams(b) for f, b in bodies.items()}
    for arg in args:
        rf = int(arg, 16)
        body = [shape(w) for w in words(ref, rf)]
        g = grams(body)
        total = sum(g.values()) or 1
        ranked = sorted(((sum((g & c).values()) / max(total, sum(c.values())), f) for f, c in counted.items()
                         if abs(len(bodies[f]) - len(body)) <= len(body) // 2 + 4), reverse=True)
        print(f"{ref.name(rf)} {rf:08X} ({len(body)} instructions): " +
              ", ".join(f"{f:08X} ({len(bodies[f])}, {s:.2f})" for s, f in ranked[:4]))


def offsets(ref, new, args):
    moved = collections.defaultdict(collections.Counter)
    for pair in args:
        a, b = (int(x, 16) for x in pair.split("="))
        rw, nw = words(ref, a), words(new, b)
        key = lambda w: (w >> 26, (w & 0x7FF) if (w >> 26) in (4, 19, 31, 59, 63) else 0)
        matcher = difflib.SequenceMatcher(None, [key(w) for w in rw], [key(w) for w in nw], autojunk=False)
        for tag, i1, i2, j1, _ in matcher.get_opcodes():
            if tag != "equal":
                continue
            for k in range(i2 - i1):
                x, y = rw[i1 + k], nw[j1 + k]
                if (x >> 26 in range(32, 56) or x >> 26 == 14) and (x & 0xFFFF) != (y & 0xFFFF):
                    moved[x & 0xFFFF][y & 0xFFFF] += 1
    for offset in sorted(moved):
        print(f"0x{offset:04X} -> " + ", ".join(f"0x{o:04X} x{n}" for o, n in moved[offset].most_common(3)))


def main():
    root, _, ref_game = sys.argv[1].partition(":")
    ref, new = load(root, ref_game or None), load(HERE, sys.argv[2])
    {"callgraph": callgraph, "shape": shapes, "offsets": offsets}[sys.argv[3]](ref, new, sys.argv[4:])


if __name__ == "__main__":
    main()
