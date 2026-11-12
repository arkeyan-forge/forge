#!/usr/bin/env python3
"""Moves a documentation page and rewrites every reference to it (docs/README.md, "Old paths").

    python3 -I tools/docs_move.py <old path> <new path>      (paths from the repository root)
    python3 -I tools/docs_move.py --refs <old path> <new path>   (only rewrite references: the file was already moved)

Runs `git mv`, then, in every tracked text file outside thirdparty/: rewrites markdown links whose target resolves to
the old file (the link stays relative to the file that holds it, anchors are kept) and every mention of the old path
from the root (`docs/a/b.md` in code spans, comments and scripts). Prints the files it changed. The old-path table of
docs/old_paths.md is left alone: add the row "old -> new" there by hand.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEXT = re.compile(r"\.(md|txt|json|sh|py|cmake|cpp|h|mm|in|toml|inc)$|CMakeLists\.txt$")
OLD_TABLE = "\n## Old paths and where their content went"


def tracked():
    out = subprocess.run(["git", "ls-files"], cwd=ROOT, capture_output=True, text=True, check=True).stdout.split("\n")
    return [f for f in out if f and not f.startswith("thirdparty/") and TEXT.search(f)]


def rewrite(file, old, new):
    path = os.path.join(ROOT, file)
    text = open(path, encoding="utf-8").read()
    head, sep, tail = text.partition(OLD_TABLE) if file == "docs/old_paths.md" else (text, "", "")
    folder = os.path.dirname(file)
    # The moved page's own links were written from its old folder.
    written_from = os.path.dirname(old) if file == new else folder

    def link(m):
        target, anchor = m.group(2).partition("#")[::2]
        if not target or re.match(r"[a-z]+:", target):
            return m.group(0)
        resolved = os.path.normpath(os.path.join(written_from, target))
        if resolved == old:
            resolved = new
        elif written_from == folder:
            return m.group(0)
        rel = os.path.relpath(resolved, folder or ".")
        return f"{m.group(1)}({rel}{'#' + anchor if anchor else ''})"

    changed = re.sub(r"(\]\s?)\(([^)\s]+)\)", link, head)
    changed = re.sub(r"(?<![\w./-])" + re.escape(old) + r"(?![\w-])", new, changed)
    if changed != head:
        open(path, "w", encoding="utf-8").write(changed + sep + tail)
        return True
    return False


def main():
    args = sys.argv[1:]
    refs_only = args[:1] == ["--refs"]
    if refs_only:
        args = args[1:]
    if len(args) != 2:
        print(__doc__)
        return 2
    old, new = (os.path.normpath(a) for a in args)
    if not refs_only:
        os.makedirs(os.path.join(ROOT, os.path.dirname(new)), exist_ok=True)
        subprocess.run(["git", "mv", old, new], cwd=ROOT, check=True)
    for file in tracked():
        if rewrite(file, old, new):
            print(file)
    return 0


sys.exit(main())
