#!/bin/bash
# Published macOS launchers -> the unpublished 0.8.0 zip, entirely offline.
# Inputs: private/update/{v0.6.1,v0.7.0,candidate}/ArkeyanLauncher-macOS.zip.
# No builds, games, network, shared data, or changes to the input archives.
# Exit 0 = all paths proved; 1 = failure; 2 = isolation/tooling blocked.
# Tagged docs/updates.md and ui/update_page.cpp in both versions document
# SKY_LAUNCHER_TEST_UPDATE: it calls UpdateLauncher at step 4, then exits.
# Its final shot is named 24-update-failed even when the update succeeds.
# Both tags use SKY_LAUNCHER_* (env.h also accepts SKYLAUNCHER_*).
# Restart is a new direct invocation; clicking Restart now stays manual.
set -euo pipefail
cd "$(dirname "$0")/../.."
exec python3 -I - "$@" <<'PY'
import configparser
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import signal
import stat
import subprocess
import sys
import tempfile
import time
import zipfile

if len(sys.argv) != 1:
    sys.exit("usage: bash tools/regress/launcher_update.sh")
root = Path.cwd().resolve()
base = root / "private/update"
versions = ("0.6.1", "0.7.0")
asset = "ArkeyanLauncher-macOS.zip"

def require(ok, message):
    if not ok:
        raise RuntimeError(message)

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def tree(path):
    result = {}
    for p in sorted(path.rglob("*")):
        kind = "link" if p.is_symlink() else "dir" if p.is_dir() else "file"
        value = os.readlink(p) if p.is_symlink() else digest(p) if p.is_file() else ""
        result[str(p.relative_to(path))] = (kind, stat.S_IMODE(p.lstat().st_mode), value)
    return result

def archive_version(path):
    with zipfile.ZipFile(path) as z:
        # Refuse path escapes before ditto sees the archive.
        for entry in z.infolist():
            parts = Path(entry.filename).parts
            require(not entry.filename.startswith("/") and ".." not in parts,
                    "unsafe archive entry")
            require(not stat.S_ISLNK(entry.external_attr >> 16), "archive symlink refused")
        plists = [n for n in z.namelist() if n.endswith(".app/Contents/Info.plist")]
        require(len(plists) == 1, "expected exactly one launcher bundle")
        info = plistlib.loads(z.read(plists[0]))
        return info["CFBundleShortVersionString"]

def ini(path):
    c = configparser.ConfigParser(interpolation=None, strict=False)
    c.read(path)
    return {s: dict(c[s]) for s in c.sections()}

# Refuse symlinked output parents, including private/ itself.
for part in (root / "private", base, base / "run", base / "shots"):
    require(not part.is_symlink(), "symlinked scratch parent refused")
    part.mkdir(exist_ok=True)
run = Path(tempfile.mkdtemp(prefix="check-", dir=base / "run"))
shots = base / "shots" / run.name
shots.mkdir()
report = run / "results.txt"

def clean(text):
    # Logs and reports carry relative paths, never account or home paths.
    for value, replacement in ((str(root), "<clone>"),
                               (str(Path.home()), "<home>"),
                               (os.environ.get("USER", ""), "<account>")):
        if value:
            text = text.replace(value, replacement)
    return text

def say(message):
    message = clean(message)
    print(message, flush=True)
    with report.open("a") as f:
        f.write(message + "\n")

def capture(args, **kwargs):
    p = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                       text=True, timeout=60, **kwargs)
    return p.returncode, clean(p.stdout)

def file_url(path):
    # Encode all letters too: fixture JSON contains no literal home/account path.
    return "file://" + "".join("/" if b == 47 else "%%%02X" % b
                                for b in os.fsencode(path))

def release(folder, mode):
    folder.mkdir()
    shutil.copyfile(base / "candidate" / asset, folder / asset)
    checksum = "0" * 64 if mode == "bad-hash" else digest(folder / asset)
    (folder / "SHA256SUMS").write_text(checksum + "  " + asset + "\n")
    names = ["SHA256SUMS"] + ([] if mode == "missing-asset" else [asset])
    data = {"tag_name": "v0.8.0", "prerelease": False, "draft": False,
            "html_url": file_url(folder / "latest.json"),
            "body": "Offline launcher update regression.",
            "assets": [{"name": n, "size": (folder / n).stat().st_size,
                        "browser_download_url": file_url(folder / n)} for n in names]}
    (folder / "latest.json").write_text(json.dumps(data, indent=2) + "\n")

active = None
def stop():
    global active
    if active is not None:
        if active.poll() is None:
            active.terminate()
            try:
                active.wait(timeout=3)
            except subprocess.TimeoutExpired:
                active.kill()
                active.wait()
        active = None

def interrupted(signum, frame):
    stop()
    raise SystemExit(128 + signum)

signal.signal(signal.SIGINT, interrupted)
signal.signal(signal.SIGTERM, interrupted)

try:
    require(sys.platform == "darwin", "macOS required")
    for command in ("ditto", "sandbox-exec", "xcrun"):
        require(shutil.which(command), "missing tool: " + command)
    for version, folder in [(v, "v" + v) for v in versions] + [("0.8.0", "candidate")]:
        require(archive_version(base / folder / asset) == version, "archive version mismatch: " + folder)
        if folder != "candidate":
            lines = (base / folder / "SHA256SUMS").read_text().splitlines()
            expected = [line.split()[0] for line in lines
                        if len(line.split()) == 2 and line.split()[1].lstrip("*") == asset]
            require(expected == [digest(base / folder / asset)], "published archive checksum mismatch: " + folder)
    for mode in ("good", "bad-hash", "missing-asset"):
        release(run / ("release-" + mode), mode)
    say("PASS fixtures: published inputs and candidate versions checked; local assets and SHA256SUMS prepared")
    say("PASS archive versions: 0.6.1, 0.7.0; candidate 0.8.0 (running versions await screenshots)")

    # All child processes, including curl, inherit this deny-network/write fence.
    # No fallback to an unguarded launcher if the OS refuses nested sandboxing.
    profile = run / "isolation.sb"
    profile.write_text('(version 1)\n(allow default)\n(deny network*)\n'
                       '(deny file-write*)\n(allow file-write* (literal "/dev/null")'
                       ' (subpath (param "RUN")) (subpath (param "SHOTS")))\n')
    sandbox = ["sandbox-exec", "-D", "RUN=" + str(run), "-D", "SHOTS=" + str(shots), "-f", str(profile)]
    code, output = capture(sandbox + ["/usr/bin/true"])
    (run / "sandbox.txt").write_text(output)
    if code:
        say("FAIL isolation: sandbox-exec refused; no launcher started (" + output.strip() + ")")
        for version in versions:
            say("BLOCKED " + version + " -> 0.8.0, checksum refusal, missing asset: isolation unavailable")
        say("MANUAL: all runtime evidence and screenshots pending; sandbox prevented the dry start")
        sys.exit(2)

    # Apple Vision reads the version actually drawn by the running executable,
    # and the error/restart text. A plist or a replaced binary alone is no pass.
    swift = run / "read_screen.swift"
    swift.write_text('''import Foundation
import Vision
let url = URL(fileURLWithPath: CommandLine.arguments[1])
let request = VNRecognizeTextRequest()
request.recognitionLevel = .accurate
request.usesLanguageCorrection = false
try VNImageRequestHandler(url: url).perform([request])
for item in request.results ?? [] {
    if let text = item.topCandidates(1).first?.string { print(text) }
}
''')
    build_env = dict(os.environ, HOME=str(run), CFFIXED_USER_HOME=str(run), TMPDIR=str(run),
                     CLANG_MODULE_CACHE_PATH=str(run / "module-cache"),
                     SWIFT_MODULECACHE_PATH=str(run / "module-cache"))
    code, output = capture(["xcrun", "swiftc", str(swift), "-o", str(run / "read_screen")], env=build_env)
    (run / "ocr-build.txt").write_text(output)
    require(code == 0, "Apple Vision OCR helper could not compile; see ocr-build.txt")

    def ocr(path):
        code, output = capture([str(run / "read_screen"), str(path)], env=build_env)
        require(code == 0, "screenshot OCR failed")
        path.with_suffix(".txt").write_text(output)
        return output

    def screen_version(text, expected):
        # v0.8.0 is also in the offer: inspect only the first sidebar lines.
        require(re.search(r"(?<![0-9])v?" + re.escape(expected) + r"(?![0-9])",
                          "\n".join(text.splitlines()[:4])),
                "running version not proved in screenshot sidebar: " + expected)

    def launch(case, app, label, update=False, first_only=False, until=None):
        global active
        target = shots / case.name / label
        target.mkdir(parents=True)
        # Start from a minimal environment; never inherit a game/test hook.
        env = {"PATH": "/usr/bin:/bin:/usr/sbin:/sbin", "LANG": "en_US.UTF-8",
               "HOME": str(case / "home"), "CFFIXED_USER_HOME": str(case / "home"),
               "TMPDIR": str(case / "tmp"), "XDG_DATA_HOME": str(case / "data"),
               "XDG_CACHE_HOME": str(case / "cache"), "XDG_CONFIG_HOME": str(case / "home"),
               "SDL_VIDEO_DRIVER": "offscreen", "SDL_AUDIO_DRIVER": "dummy",
               "SKYLANDERS_DATA": str(case / "data"), "SKY_COLLECTION": str(case / "collection"),
               "SKY_SAVE_DIR": str(case / "save"), "SKY_SETTINGS": str(case / "launcher.ini"),
               "SKY_LAUNCHER_INI": str(case / "launcher.ini"), "SKY_LAUNCHER_DATA": str(case / "data"),
               "SKY_LAUNCHER_DESKTOP": str(case / "desktop"), "SKY_LAUNCHER_LOG_DIR": str(case / "logs"),
               "SKY_LAUNCHER_KIT": str(case / "no-kit"), "SKY_LAUNCHER_RUNTIME": str(case / "no-runtime"),
               "SKY_LAUNCHER_LANGUAGE": "en", "SKY_LAUNCHER_SHOTS": str(target),
               "SKY_LAUNCHER_NO_UPDATE_CHECK": "1",
               "SKY_LAUNCHER_UPDATE_URL": file_url(run / ("release-" + case.name.split("-", 1)[1]) / "latest.json")}
        for key in ("HOME", "CFFIXED_USER_HOME", "TMPDIR", "SKYLANDERS_DATA", "SKY_COLLECTION",
                    "SKY_SAVE_DIR", "SKY_SETTINGS", "SKY_LAUNCHER_INI", "SKY_LAUNCHER_DATA",
                    "SKY_LAUNCHER_DESKTOP", "SKY_LAUNCHER_LOG_DIR"):
            require(Path(env[key]).is_relative_to(case), "isolation variable outside scratch: " + key)
        if update:
            env["SKY_LAUNCHER_TEST_UPDATE"] = "1"
        if until:
            # A player's start: the launcher skips its update cleanup under the shots or the no-check variable.
            del env["SKY_LAUNCHER_SHOTS"], env["SKY_LAUNCHER_NO_UPDATE_CHECK"]
        exe = app / "Contents/MacOS/ArkeyanLauncher"
        active = subprocess.Popen(sandbox + [str(exe)], cwd=case,
                                  env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        marker = target / ("20-update-banner.png" if update else "01-no-iso.png")
        deadline = time.monotonic() + 45
        timed_out = False
        try:
            while active.poll() is None:
                if first_only and marker.is_file() and marker.stat().st_size > 8:
                    break
                if until and until():
                    break
                if time.monotonic() > deadline:
                    timed_out = True
                    break
                time.sleep(0.05)
            # Stop only our owned PID. No restart via LaunchServices/open.
            if first_only or timed_out or until:
                active.terminate()
            try:
                output = active.communicate(timeout=3)[0]
            except subprocess.TimeoutExpired:
                active.kill()
                output = active.communicate()[0]
            returncode = active.returncode
        finally:
            stop()
        (case / (label + ".txt")).write_text(clean(output.decode(errors="replace")))
        require(not timed_out, label + " timed out; no pass inferred from app files")
        require(first_only or until or returncode == 0, label + " launcher exited unsuccessfully")
        require(until or marker.is_file(), label + " produced no screen (offscreen startup/hook unavailable)")
        return target

    for version in versions:
        for mode in ("good", "bad-hash", "missing-asset"):
            case = run / (version + "-" + mode)
            case.mkdir()
            for name in ("home", "data", "desktop", "collection", "save", "tmp", "logs", "cache", "app"):
                (case / name).mkdir()
            code, output = capture(["ditto", "-x", "-k", str(base / ("v" + version) / asset), str(case / "app")])
            require(code == 0, "ditto failed: " + output)
            apps = list((case / "app").glob("*.app"))
            require(len(apps) == 1, "expected one extracted app")
            app = apps[0]
            settings = case / "launcher.ini"
            settings.write_text("[launcher]\nlanguage=en\nui_scale=1.5\nreduce_motion=1\nframe_rates=1\ngiants_cap=1\n"
                                "[collection]\nmoved=1\n[update]\nauto_check=0\nkit_repo=\n"
                                "[ssa.video]\nfps_limit=30\nresolution_scale=2\nshow_fps=1\n"
                                "[ssa.audio]\nvolume=37\n[ssa.input]\nkey_a=Space\n"
                                "[install." + "a" * 64 + "]\nruntime=data/SSA/fake-installed\nkit=0.6.1\nversion=0.9.0\n")
            (case / "data/SSA").mkdir()
            (case / "data/SSA/fake-installed").write_text("Not executable; no game is ever started.\n")
            shortcut = case / "desktop/Update Fixture.app/Contents/MacOS"
            shortcut.mkdir(parents=True)
            (shortcut / "launch").write_text("#!/bin/sh\nexit 0\n")
            (case / "collection/fixture.txt").write_text("Synthetic collection sentinel.\n")
            before_settings = ini(settings)
            before_desktop = tree(case / "desktop")
            before_collection = tree(case / "collection")
            before_data = digest(case / "data/SSA/fake-installed")
            before_app = tree(app)

            def preserved():
                current = ini(settings)
                for section, entries in before_settings.items():
                    for key, value in entries.items():
                        require(current.get(section, {}).get(key) == value,
                                "setting/install record changed: " + section + "." + key)
                require(tree(case / "desktop") == before_desktop, "desktop shortcut folder changed")
                require(tree(case / "collection") == before_collection, "collection changed")
                require(digest(case / "data/SSA/fake-installed") == before_data, "fake install changed")

            dry = launch(case, app, "dry", first_only=True)
            screen_version(ocr(dry / "01-no-iso.png"), version)
            preserved()
            require(tree(app) == before_app, "dry start changed the old bundle")
            say("PASS isolation " + version + " " + mode + ": dry offscreen start, scratch settings/data/desktop/collection, network denied")
            screens = launch(case, app, "update", update=True)
            screen_version(ocr(screens / "20-update-banner.png"), version)
            final = screens / "24-update-failed.png"
            require(final.is_file(), "update hook did not reach a final screen")
            text = ocr(final).lower()
            preserved()
            if mode == "good":
                require("restart now" in text, "successful update did not offer Restart now")
                require(app.with_name(app.name + ".old").is_dir(), "old bundle backup missing before restart")
                require(tree(app) != before_app, "bundle was not swapped")
                # Independent candidate executable identity, in addition to the runtime version screen.
                with zipfile.ZipFile(base / "candidate" / asset) as z:
                    name = next(n for n in z.namelist() if n.endswith("/Contents/MacOS/ArkeyanLauncher"))
                    expected = hashlib.sha256(z.read(name)).hexdigest()
                require(digest(app / "Contents/MacOS/ArkeyanLauncher") == expected, "replacement differs from candidate")
                restarted = launch(case, app, "restart", first_only=True)
                screen_version(ocr(restarted / "01-no-iso.png"), "0.8.0")
                old = app.with_name(app.name + ".old")
                launch(case, app, "player-start", until=lambda: not old.exists())
                require(not old.exists(), "a player's start did not clean old bundle")
                require(not (case / "app/.skylauncher-update").exists(), "a player's start did not clean the download")
                preserved()
                say("PASS " + version + " -> 0.8.0: Update now hook, verified swap, scripted restart, running version screenshot; settings/install/desktop/collection preserved")
            else:
                if mode == "bad-hash":
                    require("damaged" in text or "checksum" in text or "match" in text,
                            "checksum rejection was not shown")
                else:
                    require("has no" in text and "arkeyanlauncher-macos.zip" in text,
                            "missing asset message was not shown")
                require(tree(app) == before_app, "rejected update changed old app")
                require(not app.with_name(app.name + ".old").exists(), "rejected update left backup")
                require(not (case / "app/.skylauncher-update").exists(), "rejected download was not cleaned")
                restarted = launch(case, app, "still-working", first_only=True)
                screen_version(ocr(restarted / "01-no-iso.png"), version)
                preserved()
                say("PASS " + version + " " + mode + ": visible refusal, unchanged old app, running version " + version + " after restart; player files preserved")
    say("MANUAL: click Restart now (harness restarts directly); non-writable Applications/DMG fallback is outside this writable-copy test")
    say("PASS all update paths; screenshots: " + str(shots.relative_to(root)))
except (RuntimeError, OSError, subprocess.SubprocessError, KeyError, ValueError, zipfile.BadZipFile) as error:
    say("FAIL: " + str(error))
    say("STOP: launcher left unfixed; remaining paths unproved; evidence: " + str(run.relative_to(root)))
    sys.exit(1)
finally:
    stop()
PY
