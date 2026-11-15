#!/usr/bin/env python3
"""Checks that nothing derived from a disc and no personal information is in the tracked tree (the pre-push hook).

    tools/leak_scan.py [--words FILE]

  paths     no private/ or ppc/, no disc, package, key or dump files, no big files (but the pipeline baselines, hashes
            only, and the XenonRecomp configs, addresses only);
  personal  the local word list (default private/kit_scan_words.txt, git-ignored: read, never copied), home-directory
            paths, e-mail addresses (licences and notices excepted); the handles `mflr0` and `Suolumi` are let through.
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
    words = [w.strip() for w in open(path, encoding="utf-8")]
    words = [re.escape(w) for w in words if w and not w.startswith("#") and w.lower() not in HANDLES]
    return re.compile("|".join(words), re.I) if words else None


def scan(root, word_rx):
    problems = []
    for rel, full in tracked(root):
        if FORBIDDEN_DIR.search(rel) or FORBIDDEN_EXT.search(rel):
            problems.append(("paths", rel, "disc-derived or forbidden file"))
        size = os.path.getsize(full)
        if size > BIG and not ALLOWED_BIG.search(rel):
            problems.append(("paths", rel, f"big file ({size} bytes)"))
        if word_rx and word_rx.search(rel):
            problems.append(("personal", rel, "word in the file name"))
        data = open(full, "rb").read()
        if b"\0" in data[:8000]:
            if word_rx and word_rx.search(data.decode("latin-1")):
                problems.append(("personal", rel, "word in a binary file"))
            continue
        email_ok = any(rel.startswith(p) or os.path.basename(rel).startswith(p) for p in EMAIL_OK_PATH)
        for n, line in enumerate(data.decode("utf-8", "replace").split("\n"), 1):
            if word_rx and word_rx.search(line):
                problems.append(("personal", f"{rel}:{n}", "word from the local list"))
            if HOME.search(line):
                problems.append(("personal", f"{rel}:{n}", "home-directory path"))
            if not email_ok and any(e not in IDENTITY_OK for e in EMAIL.findall(line)):
                problems.append(("personal", f"{rel}:{n}", "e-mail address"))
    return problems


def main():
    root = subprocess.run(["git", "rev-parse", "--show-toplevel"], check=True, capture_output=True, text=True).stdout.strip()
    ap = argparse.ArgumentParser()
    ap.add_argument("--words", default=os.path.join(root, "private", "kit_scan_words.txt"))
    args = ap.parse_args()
    problems = scan(root, word_regex(args.words))
    for kind, where, what in problems[:60]:
        print(f"  [{kind}] {where}: {what}")
    print(f"leak_scan: {len(problems)} hit(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
