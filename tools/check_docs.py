#!/usr/bin/env python3
"""Checks the documentation (docs/README.md, "Conventions"). Prints every problem; exit status 1 if any:

- the paths it writes: markdown links, and `code spans` that start with a top-level folder of the repository (runtime/,
  games/, engines/, tools/, docs/...) must exist;
- every page under docs/ and games/*/docs/ starts with the header line
  `> **Purpose:** ... · **Status:** current|design|archive ... · **Tags:** ...`;
- tags are spelled [Xbox] [XDK] [Lux] [Alchemy] [SSA] [Giants] [SwapForce] [V] [?], evidence after the tag in parentheses;
- every page is listed in docs/README.md or in its folder's README.md.

It also counts the doc paths cited in code comments (runtime/, engines/, games/, tools/, CMake) that no longer exist:
those resolve through docs/old_paths.md and are reported as a note, not as broken.

    python3 -I tools/check_docs.py [file.md ...]      (default: public Markdown, .txt.in templates and docs/ JSON)

A span is skipped when it holds a placeholder (<id>, {a,b}, *, ...) that cannot be resolved to one file: for `*` and
`{a,b}` it checks that something matches. Paths under private/ are not checked (git-ignored, derived from the disc).
"""
import glob
import itertools
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOP = ("runtime", "games", "engines", "tools", "docs", "resources", "cmake", "patches", "thirdparty", "ArkeyanRecompLib")
# Files that are not ours to rewrite.
SKIP_FILES = ("THIRD_PARTY_NOTICES.md",)
# Folders of pages written for another repository (their links resolve there).
SKIP_DIRS = ("office/tools/forge_readmes/",)
# Public documentation areas. Private archives are not required to check an exported checkout.
DOC_AREAS = ("architecture", "engines", "getting-started", "porting", "release", "releases", "runtime", "status", "tools")
PRIVATE_FILES = ("docs/MAP.md", "docs/porting/orchestration.md", "docs/porting/pitfalls.md")


def public(file):
    if file.endswith(".md") and "/" not in file:
        return file in ("README.md", "CHANGELOG.md", "THIRD_PARTY_NOTICES.md")
    if file in PRIVATE_FILES or file.startswith(("office/", "docs/guides/")):
        return False
    if file.startswith(".") and not file.startswith(".github/"):
        return False
    parts = file.split("/")
    return not (parts[0] == "docs" and len(parts) > 2 and parts[1] not in DOC_AREAS)


# Paths the docs name that are not in this repository, and why.
KNOWN_MISSING = {
    "docs/install.md": "Arkeyan Launcher's docs",
    "docs/collection_ui.md": "Arkeyan Launcher's docs",
    "docs/portal-compatibility.md": "tsapZ/SwapForceRecomp's docs",
    "docs/updates.md": "Arkeyan Launcher's docs",
    "docs/support.md": "Arkeyan Launcher's docs",
    "docs/STATUS.md": "tsapZ/SwapForceRecomp's docs",
    "docs/menus.md": "tsapZ/SwapForceRecomp's docs",
    "docs/reverse-engineering.md": "tsapZ/SwapForceRecomp's docs",
    "docs/game/": "tsapZ/SwapForceRecomp's docs",
    "docs/audit/<angle>.md": "written per audit",
    "tools/release_kit.sh": "Arkeyan Launcher's tools",
    "tools/sync_collection.sh": "Arkeyan Launcher's tools",
    "resources/fonts/": "Arkeyan Launcher's resources",
    "resources/linux/skylauncher.desktop": "Arkeyan Launcher's resources",
    "resources/icons/skylauncher.svg": "Arkeyan Launcher's resources",
    "games/giants-audio": "a branch name, not a path",
    "docs/launcher_ui.md": "Arkeyan Launcher's docs",
    "docs/portal.md": "another port's documentation",
    "docs/game/giants.md": "another port's documentation",
    "docs/roadmap.md": "another port's documentation",
    "docs/high-framerate.md": "another port's documentation",
    "docs/performance.md": "another port's documentation",
    "docs/native-renderer.md": "another port's documentation",
    "docs/texture-packs.md": "another port's documentation",
    "engines/lux/engine_notes.md": "another port's documentation",
    "runtime/xbox360/input.md": "another port's documentation",
}


def tracked(patterns):
    out = subprocess.run(["git", "ls-files", "--cached", "--others", "--exclude-standard"], cwd=ROOT, capture_output=True, text=True, check=True).stdout.split("\n")
    return sorted({f for f in out if f and os.path.isfile(os.path.join(ROOT, f))
                   and not f.startswith("thirdparty/") and public(f)
                   and any(re.search(p, f) for p in patterns)})


def expand(path):
    """`a/{b,c}.h` -> a/b.h, a/c.h"""
    m = re.search(r"\{([^{}]*,[^{}]*)\}", path)
    if not m:
        return [path]
    return list(itertools.chain.from_iterable(expand(path[:m.start()] + alt + path[m.end():]) for alt in m.group(1).split(",")))


def exists(path):
    path = path.rstrip("/.,:;)") if path not in KNOWN_MISSING else path
    if "/private/" in path + "/" or path.startswith("thirdparty/"):  # derived from the disc / submodules, not in git
        return True
    if path in KNOWN_MISSING or "[]" in path or re.search(r"/[A-Z]$", path):  # a JSON path, a placeholder ("docs/X")
        return True
    stem = re.match(r"(.*\.)(\w+)/(\w+)$", path)  # "a.cpp/h" and "a.h/cpp": both files
    if stem and not os.path.exists(os.path.join(ROOT, path)):
        return all(os.path.exists(os.path.join(ROOT, stem.group(1) + ext)) for ext in stem.groups()[1:])
    path = re.sub(r"[:#]\d+(-\d+)?$", "", path)  # file.cpp:123
    for candidate in expand(path):
        if "<" in candidate or "$" in candidate or "..." in candidate:
            head = re.split(r"[<$]|\.\.\.", candidate)[0]
            head = head[:head.rfind("/") + 1] if "/" in head else head
            if not os.path.exists(os.path.join(ROOT, head)):
                return False
            continue
        if any(c in candidate for c in "*?"):
            if not glob.glob(os.path.join(ROOT, candidate)):
                return False
        elif not os.path.exists(os.path.join(ROOT, candidate)):
            # "foo.*" and "foo.{h,cpp}" style: the stem with any extension
            return False
    return True


HEADER = re.compile(r"^> \*\*Purpose:\*\* .+ · \*\*Status:\*\* (current|design|archive)\b.* · \*\*Tags:\*\* .+$")
# A tag with more inside the brackets than the tag itself ("[V partial]", "[Lux/SSA]"); "[V] (partial)" is the form.
BAD_TAG = re.compile(r"\[(Xbox|XDK|Lux|Alchemy|SSA|Giants|SwapForce|V|\?)[^\]\w(][^\]\[]*\](?!\()|\[(Proj|Tool|GPU|game|generic)\]")


def is_page(file):
    return file.endswith(".md") and (file.startswith("docs/") or re.match(r"games/[^/]+/docs/", file))


def header_problems(file, text):
    lines = [line for line in text.split("\n") if line.strip()]
    if len(lines) < 2 or not lines[0].startswith("# ") or not HEADER.match(lines[1]):
        return [(1, "header: the line after the title must be '> **Purpose:** ... · **Status:** ... · **Tags:** ...'")]
    return []


def tag_problems(text):
    problems, fence = [], False
    for number, line in enumerate(text.split("\n"), 1):
        if line.lstrip().startswith("```"):
            fence = not fence
        if fence:
            continue
        for match in BAD_TAG.finditer(re.sub(r"`[^`]*`", "", line)):
            problems.append((number, f"tag {match.group(0)[:40]}: write [V] (evidence), [Lux] [SSA]..."))
    return problems


def listed(file):
    """A page is listed in docs/README.md or in its own folder's README.md."""
    if os.path.basename(file) == "README.md" and file.startswith("docs/"):
        indexes = ["docs/README.md", os.path.join(os.path.dirname(os.path.dirname(file)), "README.md")]
    else:
        indexes = ["docs/README.md", os.path.join(os.path.dirname(file), "README.md")]
    for index in indexes:
        path = os.path.join(ROOT, index)
        if index == file or not os.path.exists(path):
            continue
        folder = os.path.dirname(index)
        for target in re.findall(r"\]\(([^)\s#]+)", open(path, encoding="utf-8").read()):
            if os.path.normpath(os.path.join(folder, target)) == file:
                return True
    return False


CODE = re.compile(r"\.(cpp|h|mm|sh|py|cmake|toml|inc)$|CMakeLists\.txt$")


def code_references():
    """Doc paths cited by code that are gone: (resolved through the old-path table, not resolved)."""
    table = open(os.path.join(ROOT, "docs/old_paths.md"), encoding="utf-8").read().split("## Old paths and where their content went")[-1]
    old = set(re.findall(r"^\| `([^`]+)`", table, re.M))
    resolved, missing = [], []
    for file in tracked([CODE.pattern]):
        if file.startswith("tools/check_docs.py") or file.startswith("tools/docs_move.py"):
            continue
        text = open(os.path.join(ROOT, file), encoding="utf-8", errors="replace").read()
        for path in re.findall(r"(?<![\w/.-])((?:docs|games/\w+/docs)/[\w./-]+\.md)", text):
            if os.path.exists(os.path.join(ROOT, path)) or path in KNOWN_MISSING:
                continue
            (resolved if path in old else missing).append(f"{file}: {path}")
    return resolved, missing


def check(file):
    problems = []
    text = open(os.path.join(ROOT, file), encoding="utf-8", errors="replace").read()
    if is_page(file):
        problems += header_problems(file, text)
        problems += tag_problems(text.split("\n## Old paths and where their content went")[0])
        if file != "docs/README.md" and not listed(file):
            problems.append((1, "not listed in docs/README.md or in its folder's README.md"))
    # Only the retired-path catalogue may list paths that are gone on purpose.
    if file == "docs/old_paths.md":
        text = text.split("\n## Old paths and where their content went")[0]
    folder = os.path.dirname(file)
    for number, line in enumerate(text.split("\n"), 1):
        for target in re.findall(r"\]\(([^)\s]+)\)", line):
            if re.match(r"[a-z]+:|#|mailto:", target):
                continue
            target = target.split("#")[0]
            if target and not os.path.exists(os.path.join(ROOT, folder, target)):
                problems.append((number, f"link {target}"))
        for span in re.findall(r"`([^`\n]+)`", line):
            for token in span.split():
                token = token.strip("'\"(),;")
                if token.startswith("./"):
                    token = token[2:]
                if "/" in token and token.split("/")[0] in TOP and not exists(token):
                    problems.append((number, f"path {token}"))
    return problems


def main():
    args = sys.argv[1:]
    if "--list-old" in args:
        args = [a for a in args if a != "--list-old"]
        resolved, missing = code_references()
        for line in resolved:
            print(line)
        for line in missing:
            print(f"note: code cites a missing page: {line}")
        print(f"note: {len(resolved)} doc path(s) in code comments are old paths (resolved by the old-path table), "
              f"{len(missing)} missing")
        return 0
    files = args or tracked([r"\.md$", r"^docs/.*\.json$", r"\.txt\.in$"])
    bad = 0
    for file in files:
        if os.path.basename(file) in SKIP_FILES or file.startswith(SKIP_DIRS):
            continue
        for number, what in check(file):
            print(f"{file}:{number}: {what}")
            bad += 1
    if not args:
        resolved, missing = code_references()
        for line in missing:
            print(f"note: code cites a missing page: {line}")
        print(f"note: {len(resolved)} doc path(s) in code comments are old paths (resolved by the old-path table), "
              f"{len(missing)} missing")
        # The README's game table is written from the games' status pages (tools/game_status.py): one source of truth.
        status = subprocess.run([sys.executable, "-I", os.path.join(ROOT, "tools", "game_status.py"), "--check"],
            capture_output=True, text=True)
        if status.returncode != 0:
            print(status.stdout.strip() or status.stderr.strip())
            bad += 1
    print(f"{len(files)} file(s) checked, {bad} broken")
    return 1 if bad else 0


sys.exit(main())
