#!/usr/bin/env python3
"""Emit public runtime headers, compare local translations, and run host ABI unit probes."""
import argparse
from contextlib import contextmanager
import hashlib
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
XENON = Path("thirdparty/XenonRecomp")
TEMPLATE = XENON / "XenonUtils/ppc_context.h"
WORK = Path(".scratch/abi")
GAMES = ("ssa", "giants", "swapforce", "trapteam", "superchargers", "imaginators")


class Failure(Exception):
    pass


def run(command):
    temporary = WORK / "tmp"
    temporary.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ, TMPDIR=str(temporary), TMP=str(temporary), TEMP=str(temporary))
    result = subprocess.run([str(part) for part in command], capture_output=True, env=environment)
    if result.returncode:
        # Neither compiler/parser diagnostics nor a private header's contents belong in comparison output.
        raise Failure()
    return result.stdout


def ensure_patch():
    command = ["git", "-C", XENON, "apply"]
    patch = "../../patches/xenonrecomp.patch"
    applied = subprocess.run([str(p) for p in command + ["--reverse", "--check", patch]], capture_output=True)
    if applied.returncode:
        run(command + ["--check", patch])
        run(command + [patch])


def compiler(cxx):
    return [cxx] if Path(cxx).is_file() else shlex.split(cxx)


def build(cxx, fixture=False):
    ensure_patch()
    WORK.mkdir(parents=True, exist_ok=True)
    name = "ArkeyanPublicHeadersFixture" if fixture else "ArkeyanPublicHeaders"
    target = WORK / name
    sources = [Path("tools/abi/translation_fixture.cpp" if fixture else "tools/abi/emit.cpp"),
               XENON / "XenonRecomp/recompiler_config.cpp"]
    includes = [XENON / path for path in ("XenonRecomp", "XenonUtils", "XenonAnalyse", "thirdparty/disasm",
                                        "thirdparty/simde", "thirdparty/fmt/include", "thirdparty/tomlplusplus/include",
                                        "thirdparty/xxHash")]
    includes += [Path("tools/xenonrecomp")]
    if fixture:
        sources += [XENON / path for path in ("XenonRecomp/recompiler.cpp", "XenonAnalyse/function.cpp",
                                            "XenonUtils/disasm.cpp", "XenonUtils/image.cpp")]
        sources += [Path("tools/xenonrecomp/ppc_decoder.cpp")]
    # Build from relative paths without debug records or machine-specific build files.
    flags = ["-std=c++17", "-DFMT_HEADER_ONLY", "-DXXH_INLINE_ALL", "-Wno-switch", "-Wno-unused-variable",
             "-Wno-null-arithmetic", "-include", XENON / "XenonRecomp/pch.h"]
    if fixture:
        flags += ["-ffunction-sections", "-fdata-sections",
                  "-Wl,-dead_strip" if sys.platform == "darwin" else "-Wl,--gc-sections"]
    run(compiler(cxx) + flags + ["-I" + str(path) for path in includes] + sources + ["-o", target])
    return target


def config_for(game):
    metadata = dict(line.split("=", 1) for line in (Path("games") / game / "game.txt").read_text().splitlines()
                    if "=" in line and not line.startswith("#"))
    return Path("games") / game / "config" / metadata["toml"]


def emit(emitter, config, destination):
    run([emitter, config, TEMPLATE, destination])


def policies(path):
    # Compare whole directive lines, preserving order and values and detecting extra/duplicate policy definitions.
    return re.findall(rb"^[ \t]*#[ \t]*define[ \t]+PPC_CONFIG_[A-Z0-9_]+\b[^\r\n]*", path.read_bytes(), re.M)


def ids(headers):
    # kit.txt's ABI ids of a format-2 kit; Arkeyan-Builder's PolicyLines computes the same from the player's translation.
    context = hashlib.sha256((headers / "ppc_context.h").read_bytes()).hexdigest()
    policy = hashlib.sha256(b"".join(line + b"\n" for line in policies(headers / "ppc_config.h"))).hexdigest()
    return context, policy


def probe(cxx, headers, destination):
    run(compiler(cxx) + ["-std=c++20", "-fno-strict-aliasing", "-I" + str(headers),
                         "-isystem", XENON / "thirdparty/simde", "tools/abi/layout_probe.cpp", "-o", destination])
    return run([destination])


@contextmanager
def scratch():
    WORK.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=WORK) as folder:
        yield Path(folder)


def compare_headers(public, translated, cxx, folder):
    if (public / "ppc_context.h").read_bytes() != (translated / "ppc_context.h").read_bytes():
        raise Failure()
    if policies(public / "ppc_config.h") != policies(translated / "ppc_config.h"):
        raise Failure()
    if probe(cxx, public, folder / "public-probe") != probe(cxx, translated, folder / "translated-probe"):
        raise Failure()


def compare_game(game, emitter, cxx):
    translated = Path("private") / game / "ppc"
    if not translated.is_dir():
        print(f"SKIP {game}")
        return True
    try:
        with scratch() as folder:
            public = folder / "public"
            emit(emitter, config_for(game), public)
            compare_headers(public, translated, cxx, folder)
        print(f"PASS {game}")
        return True
    except (Failure, OSError):
        print(f"FAIL {game}")
        return False


def fixture_case(label, config, emitter, fixture, cxx):
    try:
        with scratch() as folder:
            public, translated = folder / "public", folder / "translated"
            emit(emitter, config, public)
            run([fixture, config, TEMPLATE, translated])
            compare_headers(public, translated, cxx, folder)
            policy = (public / "ppc_config.h").read_bytes()
            if b"PPC_IMAGE_" in policy or b"PPC_CODE_" in policy or b"ppc_detail.h" in policy:
                raise Failure()
        print(f"PASS {label}")
        return True
    except (Failure, OSError):
        print(f"FAIL {label}")
        return False


def rejection_cases(emitter, fixture, cxx):
    with scratch() as folder:
        public, translated = folder / "public", folder / "translated"
        config = folder / "policy.toml"
        config.write_text("[main]\n")
        emit(emitter, config, public)
        run([fixture, config, TEMPLATE, translated])
        originals = {name: (translated / name).read_bytes() for name in ("ppc_context.h", "ppc_config.h")}
        mutations = (("missing-context", "ppc_context.h", None), ("missing-config", "ppc_config.h", None),
                     ("changed-context", "ppc_context.h", b"\n"),
                     ("extra-policy", "ppc_config.h", b"#define PPC_CONFIG_UNEXPECTED\n"),
                     ("changed-policy", "ppc_config.h", b"#define PPC_CONFIG_SKIP_LR 1\n"))
        passed = []
        for label, name, suffix in mutations:
            path = translated / name
            if suffix is None:
                path.unlink()
            else:
                path.write_bytes(originals[name] + suffix)
            try:
                compare_headers(public, translated, cxx, folder)
                passed.append(False)
                print(f"FAIL {label}")
            except (Failure, OSError):
                passed.append(True)
                print(f"PASS {label}")
            path.write_bytes(originals[name])
        return passed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    emit_parser = commands.add_parser("emit", help="write the two public headers for one game")
    emit_parser.add_argument("game", choices=GAMES)
    emit_parser.add_argument("destination", type=Path)
    check_parser = commands.add_parser("check", help="compare every available private translation, including ABI probes")
    test_parser = commands.add_parser("test", help="exercise translation with invented input and public policies")
    test_parser.add_argument("--fixture", type=Path)
    ids_parser = commands.add_parser("ids", help="print a header folder's ABI ids for kit.txt (context=, policy=)")
    ids_parser.add_argument("headers", type=Path)
    for subparser in (emit_parser, check_parser, test_parser):
        subparser.add_argument("--emitter", type=Path, help="use an already built ArkeyanPublicHeaders")
        subparser.add_argument("--cxx", default=os.environ.get("CXX", "clang++"), help="host compiler for the probes")
    args = parser.parse_args()
    if args.command == "ids":
        context, policy = ids(args.headers)
        print(f"context={context}\npolicy={policy}")
        return 0
    # Resolve user-supplied paths before selecting the repository as the working directory.
    emitter = args.emitter.resolve() if args.emitter else None
    fixture = args.fixture.resolve() if getattr(args, "fixture", None) else None
    destination = args.destination.resolve() if args.command == "emit" else None
    os.chdir(ROOT)
    try:
        emitter = emitter or build(args.cxx)
        if args.command == "emit":
            emit(emitter, config_for(args.game), destination)
            print(f"PASS {args.game}")
            return 0
        if args.command == "check":
            # Evaluate every game even when one comparison fails.
            return int(not all([compare_game(game, emitter, args.cxx) for game in GAMES]))
        fixture = fixture or build(args.cxx, fixture=True)
        passed = [fixture_case(game, config_for(game), emitter, fixture, args.cxx) for game in GAMES]
        # Omitted options and each LR/MSR layout combination cover defaults and conditional fields.
        with scratch() as folder:
            profiles = {"defaults": "[main]\n", "empty": ""}
            for lr in ("false", "true"):
                for msr in ("false", "true"):
                    profiles[f"skip-lr-{lr}-msr-{msr}"] = f"[main]\nskip_lr = {lr}\nskip_msr = {msr}\n"
            for label, contents in profiles.items():
                config = folder / "policy.toml"
                config.write_text(contents)
                passed.append(fixture_case(label, config, emitter, fixture, args.cxx))
        passed += rejection_cases(emitter, fixture, args.cxx)
        return int(not all(passed))
    except (Failure, OSError):
        if args.command == "check":
            for game in GAMES:
                verdict = "FAIL" if (Path("private") / game / "ppc").is_dir() else "SKIP"
                print(f"{verdict} {game}")
        else:
            print(f"FAIL {args.game}" if args.command == "emit" else "FAIL public headers")
        return 1


if __name__ == "__main__":
    sys.exit(main())
