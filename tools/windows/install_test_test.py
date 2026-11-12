"""Synthetic install/image checks and a fake-Docker CLI test; never starts Wine or a game."""
import importlib.util
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location("install_check", HERE / "install_check.py")
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)


def picture(path, black=False):
    pixels = bytes([0, 0, 0] * 80 * 80) if black else bytes([40, 80, 120, 180, 140, 90] * 40 * 80)
    path.write_bytes(b"P6\n80 80\n255\n" + pixels)


class InstallTest(unittest.TestCase):
    def setUp(self):
        scratch = ROOT / ".scratch/installguard"
        scratch.mkdir(parents=True, exist_ok=True)
        temp = tempfile.TemporaryDirectory(dir=scratch)
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.out = self.root / "out"
        self.kit = self.root / "kit"
        metadata = self.kit / "config/giants"
        metadata.mkdir(parents=True)
        (metadata / "game.txt").write_text("id=giants\nruntime=GuardRecomp\nmovie_shaders=2\n")
        (self.kit / "Arkeyan-Builder.exe").write_text("synthetic")
        self.install_log = self.root / "install.log"

    def install(self):
        work = self.out / "game/arkeyan-work"
        shaders = work / "shaders/input/runtime"
        shaders.mkdir(parents=True)
        for name in ("0000000000000001.ps", "0000000000000002.vs"):
            (shaders / name).write_bytes(struct.pack(">III", 0x102A1101 if name.endswith("vs") else 0x102A1100, 0x40, 8) + b"s" * 60)
        (work / "movies").mkdir()
        (work / "movies/done.txt").write_text("2\n")
        (work / "ppc").mkdir()
        (work / "ppc/shader_cache.cpp").write_text("\t{ 0x1, entry },\n\t{ 0x2, entry },\n")
        (self.out / "game/GuardRecomp.exe").write_text("synthetic")
        self.install_log.write_text("@done Z:/work/game/GuardRecomp.exe\n")
        return shaders

    def test_completion_count_files_and_translations(self):
        shaders = self.install()
        check.installed(self.kit, "giants", self.out, self.install_log)
        (shaders / "0000000000000002.vs").unlink()
        with self.assertRaises(check.CheckError):
            check.installed(self.kit, "giants", self.out, self.install_log)
        (shaders / "notes.txt").write_text("not a shader")
        with self.assertRaises(check.CheckError):
            check.installed(self.kit, "giants", self.out, self.install_log)

    def test_error_or_missing_done_fails(self):
        self.install()
        for text in ("", "@error incomplete\n@done Z:/work/game/GuardRecomp.exe\n"):
            self.install_log.write_text(text)
            with self.assertRaises(check.CheckError):
                check.installed(self.kit, "giants", self.out, self.install_log)

    def test_black_blank_truncated_and_late_black_pictures_fail(self):
        run = self.out / "run-1"
        run.mkdir(parents=True)
        (run / "status").write_text("124")
        (run / "stdout.log").write_text("running")
        for second in (120, 150, 175):
            picture(run / f"shot_t{second}.ppm")
        check.played(self.out, 180, 1)
        picture(run / "shot_t175.ppm", black=True)
        with self.assertRaises(check.CheckError):
            check.played(self.out, 180, 1)
        (run / "shot_t175.ppm").write_bytes(b"P6\n80 80\n255\n" + b"\xff" * 12)
        with self.assertRaises(check.CheckError):
            check.played(self.out, 180, 1)

    def test_missing_translation_early_exit_or_missing_presents_fails(self):
        run = self.out / "run-1"
        run.mkdir(parents=True)
        (run / "status").write_text("124")
        for second in (120, 150, 175):
            picture(run / f"shot_t{second}.ppm")
        (run / "runtime.log").write_text("no translated pixel shader")
        with self.assertRaises(check.CheckError):
            check.played(self.out, 180, 1)
        (run / "runtime.log").write_text("")
        (run / "status").write_text("0")
        with self.assertRaises(check.CheckError):
            check.played(self.out, 180, 1)
        (run / "status").write_text("124")
        (run / "shot_t175.ppm").unlink()
        with self.assertRaises(check.CheckError):
            check.played(self.out, 180, 1)

    def cli(self, scenario):
        bindir = self.root / "bin"
        bindir.mkdir(exist_ok=True)
        shim = bindir / "docker"
        shim.write_text('''#!/usr/bin/env python3
import os, sys, struct
from pathlib import Path
args = sys.argv[1:]
if args[:2] == ['image', 'inspect']: sys.exit(0)
assert '--pull=never' in args and args[args.index('--network') + 1] == 'none'
out = Path(next(a[:-6] for a in args if a.endswith(':/work')))
scenario = os.environ['FAKE_SCENARIO']
if any('/kit/Arkeyan-Builder.exe' in a for a in args):
    assert any('--jobs 2' in a for a in args)
    witness = out / 'resume-witness'
    if scenario in ('resume', 'twice') and not witness.exists():
        witness.write_text('keep work')
        print('rosetta error: invalid gdt selector'); sys.exit(1)
    if scenario == 'twice':
        print('rosetta error: invalid gdt selector'); sys.exit(1)
    work = out / 'game/arkeyan-work'
    for part in ('shaders/input/runtime', 'movies', 'ppc'): (work / part).mkdir(parents=True, exist_ok=True)
    for name in ('0000000000000001.ps', '0000000000000002.vs'):
        (work / 'shaders/input/runtime' / name).write_bytes(struct.pack('>III', 0x102A1101 if name.endswith('vs') else 0x102A1100, 0x40, 8) + b's' * 60)
    (work / 'movies/done.txt').write_text('2\\n')
    (work / 'ppc/shader_cache.cpp').write_text('\\t{ 0x1, entry },\\n\\t{ 0x2, entry },\\n')
    (out / 'game/GuardRecomp.exe').write_text('synthetic')
    print('@done Z:/work/game/GuardRecomp.exe')
else:
    attempt = next(a.split('=', 1)[1] for a in args if a.startswith('ATTEMPT='))
    if scenario == 'runtime_resume' and attempt == '1':
        print('rosetta error: invalid gdt selector'); sys.exit(1)
    run = out / ('run-' + attempt)
    (run / 'status').write_text('124')
    (run / 'runtime.log').write_text('no translated shader' if scenario == 'missing' else '')
    for second in (120, 150, 175):
        pixels = bytes([40, 80, 120, 180, 140, 90] * 40 * 80)
        (run / ('shot_t%d.ppm' % second)).write_bytes(b'P6\\n80 80\\n255\\n' + pixels)
''')
        shim.chmod(0o755)
        tool = self.root / "toolchain/bin"
        tool.mkdir(parents=True, exist_ok=True)
        (tool / "x86_64-w64-mingw32-clang++.exe").write_text("synthetic")
        iso = self.root / "synthetic.iso"
        iso.write_text("not a disc")
        env = dict(os.environ, PATH=f"{bindir}:{os.environ['PATH']}", WINDOWS_KIT=str(self.kit),
                   WINDOWS_TOOLCHAIN=str(tool.parent), FAKE_SCENARIO=scenario, RUN_SECONDS="180")
        return subprocess.run(["bash", str(HERE / "install_test.sh"), "giants", str(iso), str(self.out)],
                              env=env, capture_output=True, text=True)

    def test_cli_resumes_rosetta_once_and_reports_it(self):
        result = self.cli("resume")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("resuming install once", result.stdout)
        self.assertIn("Rosetta resumes: 1", result.stdout)
        self.assertTrue((self.out / "resume-witness").exists())

    def test_cli_runtime_rosetta_resumes_once(self):
        result = self.cli("runtime_resume")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("resuming game once", result.stdout)
        self.assertIn("Rosetta resumes: 1", result.stdout)

    def test_cli_repeated_rosetta_and_missing_shaders_fail(self):
        for scenario in ("twice", "missing"):
            with self.subTest(scenario=scenario):
                shutil.rmtree(self.out, ignore_errors=True)
                result = self.cli(scenario)
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertNotIn("ALL PASS", result.stdout)

    def test_refuse_outside_scratch_or_existing_content(self):
        with self.assertRaises(check.CheckError):
            check.prepare(ROOT, ROOT / "tools")
        self.out.mkdir()
        (self.out / "keep").write_text("keep")
        with self.assertRaises(check.CheckError):
            check.prepare(ROOT, self.out)


if __name__ == "__main__":
    unittest.main()
