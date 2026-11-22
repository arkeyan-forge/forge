#!/usr/bin/env python3
"""Checks that nothing derived from a disc and no personal information is in the tracked tree (the pre-push hook).

    tools/leak_scan.py [--words FILE] [--allow FILE]
    tools/leak_scan.py --self-test

  paths     no private/ or ppc/, no disc, package, key or dump files, no big files (but the pipeline baselines, hashes
            only, and the XenonRecomp configs, addresses only);
  personal  the local word list (default private/kit_scan_words.txt, git-ignored: read, never copied), each word matched
            whole (a short word is part of ordinary words, as in tools/kit_scan.sh), home-directory paths, e-mail
            addresses (licences and notices excepted); the handles `mflr0` and `Suolumi` are let through. A list word that
            is also an ordinary word of one file is let through there by a `path:word` line of the local allowlist
            (default private/leak_scan_allow.txt, git-ignored).
Exit status 1 on any hit.
"""
import argparse
import os
import re
import subprocess
import sys

FORBIDDEN_DIR = re.compile(r"(^|/)(private|ppc)/")
FORBIDDEN_EXT = re.compile(r"\.(iso|xex|xbx|xcp|msix|appx|msixbundle|cer|pfx|p12|pem|key|dmp|sky|ppm|bik|xma|wem|bnk)$", re.I)
ALLOWED_BIG = re.compile(r"(^|/)pipelines/baseline\.zst$|(^|/)config/[A-Za-z]+\.toml$")
BIG = 1_500_000
EMAIL = re.compile(r"[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+\.[A-Za-z]{2,}")
HOME = re.compile(r"/Users/(?!<|\.\.\.|\$|%|Shared)[A-Za-z0-9._-]+|/home/(?!<|\.\.\.|\$)[a-z][A-Za-z0-9._-]*"
                  r"|C:\\Users\\(?!\.\.\.|<)[A-Za-z0-9._-]+")
EMAIL_OK_PATH = ("tools/kit_scan.sh", "THIRD_PARTY_NOTICES.md", "resources/kit-licenses/", "LICENSE", "COPYING", "NOTICE",
                 "AUTHORS")
IDENTITY_OK = ("mflr0@onionmail.org", "suolumi.dev@gmail.com")
HANDLES = ("mflr0", "suolumi")


def tracked(root):
    out = subprocess.run(["git", "-C", root, "ls-files", "-z"], check=True, capture_output=True).stdout
    for rel in out.decode().split("\0"):
        full = os.path.join(root, rel)
        if rel and os.path.isfile(full) and not os.path.islink(full):
            yield rel, full


def word_regex(path):
    if not os.path.isfile(path):
        print(f"leak_scan: no word list at {path}: names are not checked", file=sys.stderr)
        return None
    return compile_words(open(path, encoding="utf-8"))


def compile_words(lines):
    words = [w.strip() for w in lines]
    words = [re.escape(w) for w in words if w and not w.startswith("#") and w.lower() not in HANDLES]
    if not words:
        return None
    return re.compile(r"(?<![A-Za-z0-9_])(?:" + "|".join(words) + r")(?![A-Za-z0-9_])", re.I)


def read_allow(path):
    """`path:word` lines: that word is an ordinary word of that tracked file, not a name."""
    allow = set()
    if os.path.isfile(path):
        for line in open(path, encoding="utf-8"):
            line = line.strip()
            if line and not line.startswith("#") and ":" in line:
                rel, word = line.rsplit(":", 1)
                allow.add((rel.strip(), word.strip().lower()))
    return allow


def word_hit(word_rx, text, rel, allow):
    return word_rx is not None and any((rel, m.group(0).lower()) not in allow for m in word_rx.finditer(text))


def scan(root, word_rx, allow=frozenset()):
    problems = []
    for rel, full in tracked(root):
        if FORBIDDEN_DIR.search(rel) or FORBIDDEN_EXT.search(rel):
            problems.append(("paths", rel, "disc-derived or forbidden file"))
        size = os.path.getsize(full)
        if size > BIG and not ALLOWED_BIG.search(rel):
            problems.append(("paths", rel, f"big file ({size} bytes)"))
        if word_hit(word_rx, rel, rel, allow):
            problems.append(("personal", rel, "word in the file name"))
        data = open(full, "rb").read()
        if b"\0" in data[:8000]:
            if word_hit(word_rx, data.decode("latin-1"), rel, allow):
                problems.append(("personal", rel, "word in a binary file"))
            continue
        email_ok = any(rel.startswith(p) or os.path.basename(rel).startswith(p) for p in EMAIL_OK_PATH)
        for n, line in enumerate(data.decode("utf-8", "replace").split("\n"), 1):
            if word_hit(word_rx, line, rel, allow):
                problems.append(("personal", f"{rel}:{n}", "word from the local list"))
            if HOME.search(line):
                problems.append(("personal", f"{rel}:{n}", "home-directory path"))
            if not email_ok and any(e not in IDENTITY_OK for e in EMAIL.findall(line)):
                problems.append(("personal", f"{rel}:{n}", "e-mail address"))
    return problems


def self_test():
    failures = 0

    def expect(condition, what):
        nonlocal failures
        if not condition:
            print(f"leak_scan self-test: FAIL {what}")
            failures += 1

    rx = compile_words(["# a comment", "", "Zorbo", "mflr0", "Quix.e"])
    expect(rx is not None, "a list with words compiles")
    expect(word_hit(rx, "by Zorbo here", "a.cpp", set()), "the word alone is a hit")
    expect(word_hit(rx, "zorbo", "a.cpp", set()), "case is ignored")
    expect(word_hit(rx, "saves/zorbo/x", "a.cpp", set()), "a path segment is a hit")
    expect(not word_hit(rx, "Zorbos and unzorbo and zorbo_x", "a.cpp", set()), "a word inside a longer word is not")
    expect(not word_hit(rx, "mflr0 commits", "a.cpp", set()), "the project's handles are let through")
    expect(not word_hit(rx, "Quixxe", "a.cpp", set()) and word_hit(rx, "Quix.e", "a.cpp", set()),
           "a list word is matched literally")
    expect(not word_hit(rx, "say Zorbo", "docs/a.md", {("docs/a.md", "zorbo")}), "an allowed file:word passes")
    expect(word_hit(rx, "say Zorbo", "docs/b.md", {("docs/a.md", "zorbo")}), "only in the file it names")
    expect(compile_words(["# only a comment", "mflr0"]) is None, "a list of handles only checks nothing")
    print(f"leak_scan self-test: {'PASS' if failures == 0 else f'{failures} failure(s)'}")
    return 1 if failures else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--words")
    ap.add_argument("--allow")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.self_test:
        return self_test()
    root = subprocess.run(["git", "rev-parse", "--show-toplevel"], check=True, capture_output=True, text=True).stdout.strip()
    words = args.words or os.path.join(root, "private", "kit_scan_words.txt")
    allow = read_allow(args.allow or os.path.join(root, "private", "leak_scan_allow.txt"))
    problems = scan(root, word_regex(words), allow)
    for kind, where, what in problems[:60]:
        print(f"  [{kind}] {where}: {what}")
    print(f"leak_scan: {len(problems)} hit(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
