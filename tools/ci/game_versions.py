#!/usr/bin/env python3
"""Refuse kits whose game changed since the previous release while its version stayed the same.

The launcher rebuilds an installed game only when the release offers a different version (games/<id>/game.txt), so a
fix shipped without a bump never reaches the players who built that game before.
Usage: tools/ci/game_versions.py [--since <ref>]  (default: the nearest v* tag behind HEAD, not on HEAD itself)
"""
import argparse
import re
import subprocess
import sys

GAMES = ("ssa", "giants", "swapforce", "trapteam", "superchargers", "imaginators")
ENGINES = {"ssa": "lux"} | {game: "alchemy" for game in GAMES if game != "ssa"}
# The tools whose changes reach a built game: the builder and what the kit packs or runs to build it.
KIT_TOOLS = ("tools/arkeyan-builder/", "tools/abi/", "tools/shaders/", "tools/thirdparty/", "tools/ffmpeg/",
             "tools/linux/", "tools/parity/", "tools/make_kit.sh", "tools/prepare_game.sh", "tools/build_windows.sh",
             "tools/build_linux.sh")
NOT_BUILT = (".github/", "docs/", "tests/", "tools/git-hooks/")


def git(*arguments):
    return subprocess.run(["git", *arguments], check=True, capture_output=True, text=True).stdout


def affects(path, game):
    """Whether a changed path can change what the kit builds for this game."""
    if path.endswith(".md") or path.startswith(NOT_BUILT) or re.search(r"_test\.[a-z]+$", path):
        return False
    if path.startswith("tools/"):
        return path.startswith(KIT_TOOLS)
    if path.startswith("games/"):
        return path.startswith(f"games/{game}/")
    if path.startswith("engines/"):
        return path.startswith(f"engines/{ENGINES[game]}/")
    return True


def version(ref, game):
    text = git("show", f"{ref}:games/{game}/game.txt")
    match = re.search(r"^version=([0-9]+)\.([0-9]+)\.([0-9]+)$", text, re.M)
    if match is None:
        raise ValueError(f"games/{game}/game.txt at {ref}: no version=X.Y.Z")
    return tuple(int(part) for part in match.groups())


def previous_release():
    excluded = [f"--exclude={tag}" for tag in git("tag", "--points-at", "HEAD", "--list", "v*").split()]
    return git("describe", "--tags", "--abbrev=0", "--match", "v*", *excluded, "HEAD").strip()


def problems(since):
    changed = git("diff", "--name-only", since, "HEAD").split()
    found = []
    for game in GAMES:
        paths = [path for path in changed if affects(path, game)]
        old, new = version(since, game), version("HEAD", game)
        shown = ".".join(map(str, new))
        if new < old:
            found.append(f"{game}: version {shown} is lower than {'.'.join(map(str, old))} in {since}")
        elif paths and new == old:
            found.append(f"{game}: {len(paths)} file(s) changed since {since} (first: {paths[0]}), version still "
                         f"{shown}: bump games/{game}/game.txt")
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--since", help="the previous release's tag or commit")
    args = parser.parse_args()
    since = args.since or previous_release()
    found = problems(since)
    for line in found:
        print(f"error: {line}", file=sys.stderr)
    if not found:
        print(f"game versions: every changed game is bumped since {since}")
    return 1 if found else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, subprocess.CalledProcessError) as error:
        print(f"error: {error.stderr.strip() if isinstance(error, subprocess.CalledProcessError) else error}",
              file=sys.stderr)
        sys.exit(1)
