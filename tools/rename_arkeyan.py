#!/usr/bin/env python3
"""Renames the shared project's "SSA" names to "Arkeyan" in a git checkout (docs/naming.md).

"SSA" is Spyro's Adventure only. The runtime, its libraries, tests, build variables and macros say Arkeyan. The names
players' machines or old installs still depend on are kept (KEPT, ENV_NAMES), and so are the historical pages (SKIP).

    tools/rename_arkeyan.py [<checkout>]          rewrite the tracked text files, git mv the renamed paths
    tools/rename_arkeyan.py --check [<checkout>]  list what is left to rename (exit 1 if anything is); changes nothing

The same table runs on the workshop, on another branch before a merge, and on Arkeyan Launcher. Deterministic: a
second run changes nothing. What it can't decide (prose that says "SSA" or "SSARecomp" for the project) is done by hand.
"""
import os
import re
import subprocess
import sys

# Fixed table, old -> new, matched whole (a "lib" or "-D" or "Fake" just before is allowed: libSSAFfx.a, -DSSA_GAME).
TABLE = {
    "SSARecompLib": "ArkeyanRecompLib",
    "SSARecompGpu": "ArkeyanRecompGpu",
    "SSARecompGpuTest": "ArkeyanRecompGpuTest",
    "SSARecompTest": "ArkeyanRecompTest",
    "SSARecompXboxTest": "ArkeyanRecompXboxTest",
    "SSAXenonUtils": "ArkeyanXenonUtils",
    "SSAArtReader": "ArkeyanArtReader",
    "SSAFfx": "ArkeyanFfx",
    "SSAMiniz": "ArkeyanMiniz",
    "SSAShaderCache": "ArkeyanShaderCache",
    "SSAGameInputs": "ArkeyanGameInputs",
    "SSADecoderCompare": "ArkeyanDecoderCompare",
    "SSADecoderReference": "ArkeyanDecoderReference",
    "SSAXenonRecompSampleGen": "ArkeyanXenonRecompSampleGen",
    # The runtime's own launcher, separate from Arkeyan Launcher's ArkeyanLauncher / ArkeyanLauncherTest.
    "SSALauncherUi": "ArkeyanRuntimeLauncherUi",
    "SSALauncherTest": "ArkeyanRuntimeLauncherTest",
    # Arkeyan-Builder's tests (it was SkyBuild).
    "SSASkyBuildTest": "ArkeyanBuilderTest",
    "SSASkyBuildGuardTest": "ArkeyanBuilderGuardTest",
    "SSAPortalTest_figure": "ArkeyanPortalTest_figure",
    "FAKE_SSABUILD": "FAKE_ARKEYAN_BUILDER",
    # CMake functions.
    "ssa_setup_ffmpeg": "arkeyan_setup_ffmpeg",
    "ssa_apply_patch": "arkeyan_apply_patch",
    "ssa_ffmpeg_reject": "arkeyan_ffmpeg_reject",
    "ssa_ffmpeg_check_license": "arkeyan_ffmpeg_check_license",
}
# Every other SSA<Name>Test -> Arkeyan<Name>Test, and every SSA_<NAME> (CMake variable, macro) -> ARKEYAN_<NAME>,
# but for the kept names below.
TEST = re.compile(r"SSA([A-Z]\w*Test)$")

PATHS = {"SSARecompLib": "ArkeyanRecompLib"}  # top-level folders and files to git mv

# Kept on purpose (docs/naming.md says why).
KEPT = {
    "SSARecomp": "old data folder, logs, macOS bundle id, Xbox package identity, Windows resource, tsapZ/SSARecomp",
    "SSABuild": "the builder's name in kits made for 0.4.x launchers",
    "SSAK": "the kernel's binding marker value",
    "SSAO": "screen-space ambient occlusion",
    "SSAP": "pipelines.bin's magic, in players' data folders",
    "SSA_ISO": "Spyro's Adventure's disc",
    "SSA_symbols": "Spyro's Adventure's XenonRecomp symbols",
    "SSA_switch_tables": "Spyro's Adventure's XenonRecomp switch tables",
}
# Environment variables: the current name is SKY_<X>; the runtime and the launcher still read SSA_<X> when it is unset
# (runtime/host/env.h). Kept as old spellings: these, every SSA_LAUNCHER_<X>, and any SSA_<X> on a line that also names
# SKY_<X> (a fallback read or its test).
ENV_NAMES = set("""AUDIO_DUMP COLLECTION D3D_CAPTURE DISPLAY_NAME DUMP_DRAWS DUMP_FRAME FIGURES FRAME_TIMES GPU_BACKEND
    GPU_SNAPSHOT GPU_SNAPSHOT_EVERY GPU_SNAPSHOT_FROM GPU_SNAPSHOT_SECONDS GPU_THREAD LANGUAGE LEVEL LOG LOG_SHADER
    MIGRATE_FROM PACKAGE_NAME PERF_LOG PLAYER_NAME PORTAL PORTAL_LOG PORTAL_PLACE PORTRAITS PRESS PRESS_FILE PRESS2 SAVE_DIR
    SETTINGS SHADER_OUT SKIP_MOVIES TOYBOX_SCRIPT TOYBOX_SHOTS TRACE_FILES VBLANK_LOG VOICE_LANGUAGE XBOX_TOOLS WATCH
    PERF_NOFLUSH PERF_STATS PERF_TRACE PERF_VERIFY GODMODE HP_MULT SKIP_ALL_CUTSCENES TOYBOX_NO_KEYBOARD TEMP_DUMPAT
    TEMP_DUMPFRAMES X XX YY ZZ WW ENVTEST_A ENVTEST_D""".split())

# Historical records and other projects' files keep the names they had.
SKIP = re.compile(r"^(docs/(?:[^/]+/)*history/|office/|CHANGELOG\.md$|games/[^/]+/docs/history\.md$"
                  r"|games/[^/]+/config/|thirdparty/[^/]+/|private/|tools/rename_arkeyan\.py$)")

NAME = re.compile(r"(?:(?<![A-Za-z0-9_])|(?<=-D)|(?<=lib)|(?<=Fake))"
                  r"(FAKE_SSABUILD|ssa_[a-z_]+|SSA_\w*[A-Za-z0-9]|SSA[A-Z]\w*)(?![A-Za-z0-9])")


def new_name(old, line):
    """The new spelling of `old` on `line`, or None to keep it."""
    if old in KEPT:
        return None
    if old in TABLE:
        return TABLE[old]
    m = TEST.match(old)
    if m:
        return "Arkeyan" + m.group(1)
    if old.startswith("SSA_"):
        rest = old[4:]
        if rest in ENV_NAMES or rest == "LAUNCHER" or rest.startswith("LAUNCHER_") or re.search(r"\bSKY_" + rest + r"\b", line):
            return None
        return "ARKEYAN_" + rest
    return None


def left(old, line):
    """True when `old` should have been renamed (for --check): an SSA name that is neither kept nor known."""
    if old.startswith("ssa_"):
        return old in TABLE
    return new_name(old, line) is not None


def files(root):
    out = subprocess.run(["git", "-C", root, "ls-files", "-z"], check=True, capture_output=True).stdout
    return [p for p in out.decode().split("\0") if p and not SKIP.match(p) and os.path.isfile(os.path.join(root, p))]


def read_text(path):
    with open(path, "rb") as f:
        data = f.read()
    if b"\0" in data:
        return None
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError:
        return None


def main(argv):
    check = "--check" in argv
    args = [a for a in argv if a != "--check"]
    root = args[0] if args else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    changed = 0
    remaining = {}
    for rel in files(root):
        path = os.path.join(root, rel)
        text = read_text(path)
        if text is None:
            continue
        lines = text.splitlines(keepends=True)
        out = []
        for number, line in enumerate(lines, 1):
            if check:
                for m in NAME.finditer(line):
                    if left(m.group(1), line):
                        remaining.setdefault(m.group(1), []).append("%s:%d" % (rel, number))
                continue

            def swap(m):
                if m.group(1).startswith("ssa_") and m.group(1) not in TABLE:
                    return m.group(0)
                new = new_name(m.group(1), line)
                return new if new else m.group(0)

            out.append(NAME.sub(swap, line))
        if not check and "".join(out) != text:
            with open(path, "w", encoding="utf-8", newline="") as f:
                f.write("".join(out))
            changed += 1
    if check:
        for name in sorted(remaining):
            print("%-32s %3d  %s" % (name, len(remaining[name]), " ".join(remaining[name][:3])))
        print("rename_arkeyan: %d names left" % len(remaining))
        moved = [old for old in PATHS if os.path.exists(os.path.join(root, old))]
        for old in moved:
            print("rename_arkeyan: path left: %s" % old)
        return 1 if remaining or moved else 0
    for old, new in PATHS.items():
        if os.path.exists(os.path.join(root, old)):
            subprocess.run(["git", "-C", root, "mv", old, new], check=True)
    print("rename_arkeyan: %d files rewritten" % changed)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
