"""Finds SSA's hooked functions and data in another build of the Lux engine (Skylanders: Giants), by byte signature.

A signature is a function's first instructions with the bits that change from one link to the next masked:
  strict  branch targets leaving the function (b, bl) and absolute addresses (lis + the D-form that completes it)
  loose   also every 16-bit immediate and displacement, and every branch target (struct offsets, constants and code
          inside the function may differ between games)
A data address (a global the runtime reads) is found through a function that references it: once the function is
found, its lis/addi pair at the recorded offset gives the new address.

usage:
  python3 -I tools/sigmatch/sigmatch.py make <hooks.json> [--image private/image.bin] [--funcs private/ssa/ppc/ppc_func_mapping.cpp]
                                              [--sigs private/giants/ssa_sigs.json]
      reads SSA's image and writes the signatures to --sigs (game bytes: private, never committed); the committed
      docs/giants/ssa_hooks.json only gets metadata (length, candidates, occurrence, data reference), no bytes
  python3 -I tools/sigmatch/sigmatch.py match <hooks.json> <image.bin | default.xex> [--sigs ...] [--out map.json] [--toml]
      prints one line per entry: name, SSA address, new address, confidence, method; --toml also prints the
      XenonRecomp [main] addresses found. A .xex is first dumped with xexdump (private/build-host-tools).

Confidence: 1.0 strict match, unique; 0.9 loose match, unique; 1/n with n candidates (the SSA occurrence index picks
one); below that, the similarity of the best fuzzy alignment (difflib over loose-masked words) for code that moved or
changed. Treat anything under 0.9 as a lead to check by hand (tools/ppcinfo.py asm <addr>).
"""
import difflib
import json
import os
import re
import struct
import subprocess
import sys
import tempfile

BASE = 0x82000000
ROOT = __file__.rsplit("/tools/", 1)[0]
MAX_WORDS = 64        # signature length for hooks; config entries use fewer (see make)
MIN_WORDS = 6         # a shorter function is extended past its end until unique
CHUNK = 4             # fuzzy search: words per anchor chunk

DFORM = {14, 24, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 58, 62}
IMM16 = DFORM | {7, 8, 10, 11, 12, 13, 15, 25, 26, 27, 28, 29}


def load_image(path):
    """(bytes, base, [(start, end) code ranges]) from image.bin (+ xexdump.txt beside it) or a .xex."""
    with open(path, "rb") as f:
        head = f.read(4)
    if head == b"XEX2":
        tool = os.path.join(ROOT, "private/build-host-tools/tools/xexdump/xexdump")
        out = tempfile.mkdtemp(prefix="sigmatch-")
        with open(os.path.join(out, "xexdump.txt"), "w") as log:
            subprocess.run([tool, path, out], check=True, stdout=log)
        path = os.path.join(out, "image.bin")
    data = open(path, "rb").read()
    base, code = BASE, []
    layout = os.path.join(os.path.dirname(path), "xexdump.txt")
    if os.path.exists(layout):
        for line in open(layout, encoding="latin-1"):
            m = re.match(r"base 0x([0-9A-F]+)", line)
            if m:
                base = int(m.group(1), 16)
            m = re.match(r"section .*? 0x([0-9A-F]{8}) 0x([0-9A-F]{8}) flags (\d+)", line)
            if m and int(m.group(3)) & 2:  # executable
                code.append((int(m.group(1), 16), int(m.group(1), 16) + int(m.group(2), 16)))
    if not code:
        code = [(base, base + len(data))]
    return data, base, code


def words_at(data, base, address, count):
    off = address - base
    count = max(0, min(count, (len(data) - off) // 4))
    return list(struct.unpack_from(f">{count}I", data, off))


def masks(words, start, end, base, size):
    """(strict, loose) mask per word. start/end: the function's bounds (branches inside it are kept by strict)."""
    strict, loose = [], []
    high = {}  # register -> index of the lis that loaded an image address into it
    for i, w in enumerate(words):
        op, rd, ra = w >> 26, (w >> 21) & 31, (w >> 16) & 31
        low16 = 0xFFFF0003 if op in (58, 62) else 0xFFFF0000
        s = 0xFFFFFFFF
        if op == 18:
            li = w & 0x03FFFFFC
            target = (li - (1 << 26) if li & 0x02000000 else li) + (0 if w & 2 else start + i * 4)
            if not (start <= target < end) or (w & 1):
                s = 0xFC000003
        elif op == 15 and ra == 0 and base <= (w & 0xFFFF) << 16 <= base + size + 0x10000:
            s = low16
        elif op in DFORM and (rd if 24 <= op <= 29 else ra) in high and i - high[rd if 24 <= op <= 29 else ra] <= 8:
            s = low16
        # GPR writes end the tracking (addi/ori complete the address; later displacements are struct offsets)
        written = ra if 24 <= op <= 29 else rd
        if op in (14, 15, 24, 25, 26, 27, 28, 29, 32, 33, 34, 35, 40, 41, 42, 43, 46, 58):
            high.pop(written, None)
        if op == 15 and s != 0xFFFFFFFF:
            high[rd] = i
        l = s
        if op in IMM16:
            l &= low16
        elif op == 16:
            l &= 0xFFFF0003
        elif op == 18:
            l &= 0xFC000003
        strict.append(s)
        loose.append(l)
    return strict, loose


def regex(words, mask):
    parts = []
    for w, m in zip(words, mask):
        for shift in (24, 16, 8, 0):
            b, mb = (w >> shift) & 0xFF, (m >> shift) & 0xFF
            if mb == 0xFF:
                parts.append(re.escape(bytes([b])))
            elif mb == 0:
                parts.append(b".")
            else:
                parts.append(b"[" + b"".join(re.escape(bytes([v])) for v in range(256) if v & mb == b & mb) + b"]")
    return re.compile(b"".join(parts), re.DOTALL)


def find_all(pattern, data, base, code):
    hits = []
    for lo, hi in code:
        pos = lo - base
        while True:
            m = pattern.search(data, pos, hi - base)
            if not m:
                break
            if m.start() % 4 == 0:
                hits.append(base + m.start())
            pos = m.start() + 1
    return hits


def hexs(words):
    return "".join(f"{w:08x}" for w in words)


def unhex(text):
    return [int(text[i:i + 8], 16) for i in range(0, len(text), 8)]


def function_starts(path):
    starts = sorted({int(a, 16) for a in re.findall(r"\{\s*0x([0-9A-Fa-f]{8})", open(path).read())})
    return starts


def sig_key(entry):
    return f"{entry['name']}@{entry['ssa']}"


def make(hooks_path, image_path, funcs_path, sigs_path):
    import bisect
    data, base, code = load_image(image_path)
    starts = function_starts(funcs_path)
    hooks = json.load(open(hooks_path))

    def bounds(address):
        i = bisect.bisect_right(starts, address) - 1
        start = starts[i] if i >= 0 else address
        end = starts[i + 1] if i + 1 < len(starts) else start + 4 * MAX_WORDS
        return start, end

    def signature(address, end, limit):
        count = max(MIN_WORDS, min(limit, (end - address) // 4))
        while True:
            words = words_at(data, base, address, count)
            strict, loose = masks(words, address, end, base, len(data))
            hits = find_all(regex(words, strict), data, base, code)
            if len(hits) == 1 or count >= MAX_WORDS:
                break
            count = min(MAX_WORDS, count * 2)
        return {"words": hexs([w & m for w, m in zip(words, strict)]), "strict": hexs(strict), "loose": hexs(loose),
                "occurrence": hits.index(address) if address in hits else 0, "candidates": len(hits),
                "past_end": address + 4 * count > end}

    def data_ref(target):
        """A lis/D-form pair in code that builds `target`, or failing that the base of the struct holding it (within
        0x100 bytes before): (function, offset of the lis, pair distance, target - the pair's value)."""
        near = None
        for hi in sorted({target >> 16, (target + 0x8000) >> 16 & 0xFFFF, (target - 0x100) >> 16, (target - 0x100 + 0x8000) >> 16 & 0xFFFF}):
            pattern = re.compile(b"[\x3c-\x3f][\x00\x20\x40\x60\x80\xa0\xc0\xe0]" + re.escape(struct.pack(">H", hi)), re.DOTALL)
            for lo_c, hi_c in code:
                for m in pattern.finditer(data, lo_c - base, hi_c - base):
                    if m.start() % 4:
                        continue
                    rd = (data[m.start()] & 3) << 3 | data[m.start() + 1] >> 5
                    for k in range(1, 17):
                        w = struct.unpack_from(">I", data, m.start() + 4 * k)[0]
                        source = (w >> 21) & 31 if 24 <= w >> 26 <= 29 else (w >> 16) & 31
                        if w >> 26 not in DFORM or source != rd:
                            continue
                        delta = target - pair_value(m.start(), k)
                        if 0 <= delta < 0x100 and (near is None or delta < near[3]):
                            start, _ = bounds(base + m.start())
                            near = (start, base + m.start() - start, k, delta)
                        if near and near[3] == 0:
                            return near
        return near

    def pair_value(offset, k):
        return resolve_pair(struct.unpack_from(">I", data, offset)[0], struct.unpack_from(">I", data, offset + 4 * k)[0])

    for entry in hooks["entries"]:
        address = int(entry["ssa"], 16)
        if entry["kind"] == "data":
            ref = data_ref(address)
            if ref is None:
                entry["sig"] = None
                continue
            start, offset, pair, delta = ref
            _, end = bounds(start)
            entry["ref"] = {"function": f"0x{start:08X}", "lis_offset": offset, "pair_distance": pair, "delta": delta}
            entry["sig"] = signature(start, end, MAX_WORDS)
        else:
            start, end = bounds(address)
            if start != address:  # not a function start: sign from the address itself (e.g. a .pdata-less helper)
                end = address + int(entry.get("size", "0"), 16) if entry.get("size") else max(end, address + 16)
            limit = MAX_WORDS if entry["kind"] in ("hook", "replaced") else MAX_WORDS // 2
            entry["sig"] = signature(address, end, limit)
    private = {}
    for entry in hooks["entries"]:
        sig = entry.get("sig")
        if sig:
            private[sig_key(entry)] = {k: sig.pop(k) for k in ("words", "strict", "loose")}
            sig["length"] = len(private[sig_key(entry)]["words"]) // 8
    os.makedirs(os.path.dirname(sigs_path), exist_ok=True)
    with open(sigs_path, "w") as f:
        json.dump(private, f)
    with open(hooks_path, "w") as f:
        json.dump(hooks, f, indent=1)
        f.write("\n")
    unique = sum(1 for e in hooks["entries"] if e.get("sig") and e["sig"]["candidates"] == 1)
    print(f"{len(hooks['entries'])} entries, {unique} unique strict signatures", file=sys.stderr)


def fuzzy(sig_words, loose, data, base, code, index):
    """Best start for a signature whose code moved or changed: vote with CHUNK-word anchors, then align."""
    want = [w & m for w, m in zip(sig_words, loose)]
    votes = {}
    for c in range(0, len(want) - CHUNK + 1, CHUNK):
        key = tuple(want[c:c + CHUNK])
        if all(k == 0 for k in key):
            continue
        for position in index.get(key, ())[:64]:
            start = position - 4 * c
            bucket = start & ~0x3F
            votes[bucket] = votes.get(bucket, 0) + 1
    best = (0.0, None)
    for bucket, _ in sorted(votes.items(), key=lambda v: -v[1])[:8]:
        for start in range(bucket - 0x40, bucket + 0x80, 4):
            got = words_at(data, base, start, len(want))
            if len(got) < len(want):
                continue
            _, got_loose = masks(got, start, start + 4 * len(got), base, len(data))
            ratio = difflib.SequenceMatcher(None, want, [w & m for w, m in zip(got, got_loose)], autojunk=False).ratio()
            if ratio > best[0]:
                best = (ratio, start)
    return best


def chunk_index(data, base, code, sigs):
    """Positions of every loose-masked CHUNK-word key the signatures use, in one pass over the code."""
    keys = set()
    for words, loose in sigs:
        want = [w & m for w, m in zip(words, loose)]
        for c in range(0, len(want) - CHUNK + 1, CHUNK):
            keys.add(tuple(want[c:c + CHUNK]))
    index = {}
    for lo, hi in code:
        all_words = words_at(data, base, lo, (hi - lo) // 4)
        _, loose = masks(all_words, lo, hi, base, len(data))
        masked = [w & m for w, m in zip(all_words, loose)]
        for i in range(len(masked) - CHUNK + 1):
            key = tuple(masked[i:i + CHUNK])
            if key in keys:
                index.setdefault(key, []).append(lo + 4 * i)
    return index


def resolve_pair(lis, low):
    lo = low & 0xFFFF
    if low >> 26 in (24, 25):  # ori, oris: unsigned
        return ((lis & 0xFFFF) << 16 | lo) & 0xFFFFFFFF
    return ((lis & 0xFFFF) << 16) + (lo - 0x10000 if lo & 0x8000 else lo) & 0xFFFFFFFF


def resolve_data(entry, function, data, base):
    ref = entry["ref"]
    at = function + ref["lis_offset"] - base
    lis, low = struct.unpack_from(">I", data, at)[0], struct.unpack_from(">I", data, at + 4 * ref["pair_distance"])[0]
    if lis >> 26 != 15 or low >> 26 not in DFORM:
        return None
    return resolve_pair(lis, low) + ref.get("delta", 0)


def match(hooks_path, image_path, out_path, toml, sigs_path):
    data, base, code = load_image(image_path)
    hooks = json.load(open(hooks_path))
    if not os.path.exists(sigs_path):
        sys.exit(f"no signatures at {sigs_path}: run `sigmatch.py make` on SSA's image first")
    private = json.load(open(sigs_path))
    entries = [e for e in hooks["entries"] if e.get("sig")]
    for e in entries:  # metadata (committed) + bytes (private)
        e["sig"] = {**e["sig"], **private[sig_key(e)]}
    results, pending = [], []
    for entry in entries:
        sig = entry["sig"]
        words, strict, loose = unhex(sig["words"]), unhex(sig["strict"]), unhex(sig["loose"])
        found, confidence, method = None, 0.0, "none"
        for mask, score, name in ((strict, 1.0, "strict"), (loose, 0.9, "loose")):
            hits = find_all(regex(words, mask), data, base, code)
            if hits:
                pick = sig["occurrence"] if len(hits) == sig["candidates"] and sig["occurrence"] < len(hits) else 0
                found, method = hits[pick], name if len(hits) == 1 else f"{name}, {len(hits)} candidates"
                confidence = score / len(hits) if len(hits) > 1 else score
                if len(hits) == sig["candidates"] and len(hits) > 1:
                    confidence = score * 0.8  # same ambiguity as in SSA: the occurrence index is a fair guess
                elif sig["candidates"] > 1:
                    confidence, method = 0.4, f"{method}, {sig['candidates']} copies in SSA: check by hand"
                break
        results.append([entry, found, confidence, method])
        if found is None:
            pending.append(results[-1])
    if pending:
        index = chunk_index(data, base, code, [(unhex(r[0]["sig"]["words"]), unhex(r[0]["sig"]["loose"])) for r in pending])
        for r in pending:
            sig = r[0]["sig"]
            ratio, start = fuzzy(unhex(sig["words"]), unhex(sig["loose"]), data, base, code, index)
            if start is not None and ratio >= 0.5:
                r[1], r[2], r[3] = start, round(ratio * 0.85, 3), "fuzzy"
    out = []
    for entry, found, confidence, method in results:
        address = found
        if found is not None and entry["kind"] == "data":
            address = resolve_data(entry, found, data, base)
            method += f", via 0x{found:08X}"
        out.append({"name": entry["name"], "kind": entry["kind"], "ssa": entry["ssa"],
                    "address": f"0x{address:08X}" if address is not None else None,
                    "confidence": round(confidence, 3) if address is not None else 0.0, "method": method})
        print(f"{entry['name']:<40} {entry['ssa']}  {out[-1]['address'] or '-':<10}  {out[-1]['confidence']:<5}  {method}")
    found = sum(1 for o in out if o["address"])
    exact = sum(1 for o in out if o["confidence"] == 1.0)
    print(f"{found}/{len(out)} found, {exact} strict unique", file=sys.stderr)
    if toml:
        for o in out:
            if o["kind"] == "config" and o["address"] and o["name"].endswith("_address"):
                print(f"{o['name']} = {o['address']}  # confidence {o['confidence']}")
    if out_path:
        with open(out_path, "w") as f:
            json.dump(out, f, indent=1)
            f.write("\n")
    return out


def main(argv):
    if len(argv) < 3 or argv[1] not in ("make", "match"):
        sys.exit(__doc__)
    opts = {argv[i]: argv[i + 1] for i in range(2, len(argv) - 1) if argv[i].startswith("--")}
    sigs = opts.get("--sigs", f"{ROOT}/private/giants/ssa_sigs.json")
    if argv[1] == "make":
        make(argv[2], opts.get("--image", f"{ROOT}/private/image.bin"), opts.get("--funcs", f"{ROOT}/private/ssa/ppc/ppc_func_mapping.cpp"), sigs)
    else:
        if len(argv) < 4:
            sys.exit(__doc__)
        match(argv[2], argv[3], opts.get("--out"), "--toml" in argv, sigs)


if __name__ == "__main__":
    main(sys.argv)
