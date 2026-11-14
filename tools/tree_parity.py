#!/usr/bin/env python3
"""Compares two folders (two kits, two installs of one game) against a rules file: the parity gate of
docs/release/launcher/kits_without_disc.md. Prints file names and counts only, never contents or hashes.

    python3 -I tools/tree_parity.py <old> <new> --rules <rules file>
    python3 -I tools/tree_parity.py --self-test

A folder or a .zip of one. Rules, one per line, the first matching pattern wins (fnmatch, on the path relative to the
folder; `#` starts a comment):

    <pattern>  same                    present in both, identical bytes (the default for a path no rule matches)
    <pattern>  differs  <reason>       may differ, appear or disappear; listed with its reason
    <pattern>  gone     <reason>       must be in the old folder only
    <pattern>  added    <reason>       must be in the new folder only
    <pattern>  ignore                  not compared (work files, timestamps)
    <pattern>  keys <key patterns>     key=value text: the keys matching the comma-separated patterns may differ,
                                       appear or disappear; every other key must be equal

Exit status 0 when every path follows its rule, 1 otherwise, 2 on a usage error.
"""
import fnmatch
import hashlib
import io
import os
import sys
import tempfile
import zipfile


def read_tree(root):
    """{relative path: bytes reader} of a folder or a zip (its single top folder removed)."""
    files = {}
    if os.path.isfile(root) and zipfile.is_zipfile(root):
        archive = zipfile.ZipFile(root)
        names = [n for n in archive.namelist() if not n.endswith("/")]
        tops = {n.split("/", 1)[0] for n in names}
        strip = len(tops) == 1 and all("/" in n for n in names)
        for name in names:
            files[name.split("/", 1)[1] if strip else name] = (lambda n=name: archive.read(n))
        return files
    for folder, _, entries in os.walk(root):
        for entry in entries:
            path = os.path.join(folder, entry)
            if os.path.islink(path):
                files[os.path.relpath(path, root)] = (lambda p=path: b"link:" + os.readlink(p).encode())
            else:
                files[os.path.relpath(path, root)] = (lambda p=path: open(p, "rb").read())
    return files


def read_rules(path):
    rules = []
    for number, line in enumerate(open(path, encoding="utf-8"), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        parts = line.split(None, 2)
        if len(parts) < 2 or parts[1] not in ("same", "differs", "gone", "added", "ignore", "keys"):
            raise ValueError(f"{path}:{number}: '<pattern> same|differs|gone|added|ignore|keys ...'")
        rules.append((parts[0], parts[1], parts[2] if len(parts) > 2 else ""))
    return rules


def rule_for(rules, path):
    for pattern, kind, detail in rules:
        if fnmatch.fnmatchcase(path, pattern):
            return kind, detail
    return "same", ""


def key_values(data):
    values = {}
    for line in data.decode("utf-8", "replace").splitlines():
        if "=" in line and not line.lstrip().startswith("#"):
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()
    return values


def digest(read):
    return hashlib.sha256(read()).digest()


def compare(old_root, new_root, rules):
    """(problems, notes): one line each, names only."""
    old, new = read_tree(old_root), read_tree(new_root)
    problems, notes, counts = [], [], {"same": 0, "differs": 0, "ignored": 0}
    for path in sorted(set(old) | set(new)):
        kind, detail = rule_for(rules, path)
        here_old, here_new = path in old, path in new
        if kind == "ignore":
            counts["ignored"] += 1
        elif kind == "gone":
            if here_new:
                problems.append(f"{path}: should be gone, still in the new folder ({detail})")
            else:
                notes.append(f"{path}: gone ({detail})")
        elif kind == "added":
            if here_old:
                problems.append(f"{path}: should be new, already in the old folder ({detail})")
            else:
                notes.append(f"{path}: added ({detail})")
        elif kind == "differs":
            counts["differs"] += 1
            if not (here_old and here_new and digest(old[path]) == digest(new[path])):
                notes.append(f"{path}: {'differs' if here_old and here_new else 'only in one'} ({detail})")
        elif not (here_old and here_new):
            problems.append(f"{path}: only in the {'old' if here_old else 'new'} folder")
        elif kind == "keys":
            allowed = [p.strip() for p in detail.split(",") if p.strip()]
            a, b = key_values(old[path]()), key_values(new[path]())
            for key in sorted(set(a) | set(b)):
                if a.get(key) == b.get(key):
                    continue
                if any(fnmatch.fnmatchcase(key, p) for p in allowed):
                    notes.append(f"{path}: key {key} {'changed' if key in a and key in b else 'only in one'} (allowed)")
                else:
                    problems.append(f"{path}: key {key} {'differs' if key in a and key in b else 'only in one'}")
            counts["same"] += 1
        elif digest(old[path]) != digest(new[path]):
            problems.append(f"{path}: differs")
        else:
            counts["same"] += 1
    notes.append(f"{counts['same']} identical, {counts['differs']} allowed to differ, {counts['ignored']} ignored")
    return problems, notes


def self_test():
    failures = 0

    def expect(condition, what):
        nonlocal failures
        if not condition:
            failures += 1
            print(f"FAIL {what}")

    with tempfile.TemporaryDirectory() as scratch:
        def tree(name, files):
            root = os.path.join(scratch, name)
            for path, data in files.items():
                os.makedirs(os.path.dirname(os.path.join(root, path)), exist_ok=True)
                open(os.path.join(root, path), "wb").write(data)
            return root

        old = tree("old", {"bin/tool": b"A", "config/g/prints.txt": b"P", "kit.txt": b"version=1\nhash.g=X\nbuild=7\n",
                           "lib/a.a": b"L", "work/log": b"1"})
        new = tree("new", {"bin/tool": b"A", "config/g/layout.cpp": b"N", "kit.txt": b"version=1\nformat=2\nbuild=7\n",
                           "lib/a.a": b"L", "work/log": b"2"})
        rules_path = os.path.join(scratch, "rules.txt")
        open(rules_path, "w").write("config/*/prints.txt gone no fingerprints\nconfig/*/layout.cpp added the record\n"
                                    "kit.txt keys hash.*,format\nwork/* ignore\n")
        problems, notes = compare(old, new, read_rules(rules_path))
        expect(problems == [], f"the allowed differences pass: {problems}")
        expect(any("prints.txt: gone" in n for n in notes), "a gone file is listed")
        expect(any("key format" in n for n in notes), "an allowed key is listed")

        open(os.path.join(new, "lib/a.a"), "wb").write(b"M")
        open(os.path.join(new, "kit.txt"), "wb").write(b"version=2\nformat=2\nbuild=7\n")
        problems, _ = compare(old, new, read_rules(rules_path))
        expect("lib/a.a: differs" in problems, "a changed library fails")
        expect("kit.txt: key version differs" in problems, "a changed key fails")
        expect(not any("X" in p or "M" in p.split(":")[1] for p in problems), "no content in the messages")

        open(os.path.join(new, "config/g/prints.txt"), "wb").write(b"P")
        problems, _ = compare(old, new, read_rules(rules_path))
        expect(any("should be gone" in p for p in problems), "a file that should be gone fails")

        zipped = os.path.join(scratch, "new.zip")
        with zipfile.ZipFile(zipped, "w") as archive:
            for path, read in read_tree(old).items():
                archive.writestr("kit/" + path, read())
        problems, _ = compare(old, zipped, [])
        expect(problems == [], f"a zip of the same folder is identical: {problems}")

        bad = os.path.join(scratch, "bad.txt")
        open(bad, "w").write("x maybe\n")
        try:
            read_rules(bad)
            expect(False, "an unknown rule is refused")
        except ValueError:
            pass

    print(f"tree_parity self-test: {'PASS' if failures == 0 else f'{failures} failure(s)'}")
    return 1 if failures else 0


def main(args):
    if args == ["--self-test"]:
        return self_test()
    if len(args) != 4 or args[2] != "--rules":
        print(__doc__.strip().split("\n\n")[1], file=sys.stderr)
        return 2
    try:
        rules = read_rules(args[3])
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        return 2
    for root in args[:2]:
        if not os.path.exists(root):
            print(f"not found: {root}", file=sys.stderr)
            return 2
    problems, notes = compare(args[0], args[1], rules)
    for line in notes:
        print(f"note: {line}")
    for line in problems:
        print(f"PARITY FAIL {line}")
    print("parity: PASS" if not problems else f"parity: {len(problems)} unexpected difference(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
