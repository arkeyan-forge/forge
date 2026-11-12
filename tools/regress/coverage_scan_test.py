#!/usr/bin/env python3
"""Check coverage frame gaps and ending work-directory safety with invented inputs, without running a game.

  python3 -I tools/regress/coverage_scan_test.py
"""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
SCAN = HERE / "coverage_scan.py"
ENDING = HERE.parent / "pipelines" / "ending.sh"


class CoverageScanTest(unittest.TestCase):
    def scan(self, rows, kind="level", hint="ok", log="", prefix=""):
        with tempfile.TemporaryDirectory() as tmp:
            frames = Path(tmp) / "frames.txt"
            frames.write_text("".join(f"{time} 1 {draws}\n" for time, draws in rows))
            log_path = Path(tmp) / "game.log"
            log_path.write_text(log)
            run = subprocess.run([sys.executable, "-I", str(SCAN), str(log_path), str(frames), kind, hint, prefix],
                                 capture_output=True, text=True, timeout=10)
            self.assertEqual(run.returncode, 0, run.stderr)
            fields = run.stdout.rstrip("\n").split("\t")
            self.assertEqual(len(fields), 3)
            return fields

    def test_steady_and_two_second_gap(self):
        steady = [(time, 100) for time in range(0, 4100, 100)]
        paused = [(time + (1900 if time >= 2000 else 0), draws) for time, draws in steady]
        for kind in ("level", "figure", "credits"):
            with self.subTest(kind=kind):
                self.assertEqual(self.scan(steady, kind), ["ok", "10.0", ""])
                verdict, fps, detail = self.scan(paused, kind, prefix="test run")
                self.assertEqual((verdict, fps), ("not_reached", "0.0"))
                self.assertTrue(detail.startswith("test run no frame rate:"))
                self.assertIn("gap", detail)

    def test_one_second_gap_boundary(self):
        for gap, expected in ((999, ("ok", "2.0")), (1000, ("not_reached", "0.0"))):
            with self.subTest(gap=gap):
                rows = [(time + (gap - 100 if time >= 2000 else 0), 100)
                        for time in range(0, 4100, 100)]
                self.assertEqual(tuple(self.scan(rows)[:2]), expected)

    def test_first_and_last_busy_frames_still_bound_scan(self):
        rows = [(-5000, 1)] + [(time, 100) for time in range(0, 4100, 100)] + [(9000, 1)]
        for kind in ("level", "figure"):
            with self.subTest(kind=kind):
                self.assertEqual(self.scan(rows, kind), ["ok", "10.0", ""])
        self.assertEqual(self.scan(rows, "credits")[:2], ["not_reached", "0.0"])

    def test_gap_between_busy_frames_counts(self):
        rows = [(0, 100), (100, 1), (2100, 1), (3000, 100)]
        self.assertEqual(self.scan(rows)[:2], ["not_reached", "0.0"])

    def test_crash_keeps_priority_over_gap(self):
        rows = [(0, 100), (2000, 100), (3000, 100)]
        for hint, log in (("crash", ""), ("ok", "[guard] test fault\n")):
            with self.subTest(hint=hint, log=log):
                self.assertEqual(self.scan(rows, hint=hint, log=log)[:2], ["crash", "0.0"])

    def test_hang_is_kept_and_a_fault_wins(self):
        steady = [(time, 100) for time in range(0, 4100, 100)]
        self.assertEqual(self.scan(steady, hint="hang")[0], "hang")
        self.assertEqual(self.scan(steady, hint="hang", log="Fault (signal 10 bus) at guest address 0x10\n")[0], "crash")

    def test_short_run_still_has_no_frame_rate(self):
        rows = [(time, 100) for time in range(0, 2000, 100)]
        self.assertEqual(self.scan(rows)[:2], ["not_reached", "0.0"])


class EndingWorkTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        pipelines = self.root / "tools" / "pipelines"
        pipelines.mkdir(parents=True)
        self.script = pipelines / "ending.sh"
        shutil.copyfile(ENDING, self.script)
        # Substitute helpers in a temporary tree so even a broken guard cannot start a game.
        (pipelines / "ssa_ending.sh").write_text(
            '#!/usr/bin/env bash\nset -eu\nrm -rf "${2:?}"\nmkdir -p "$2"\nexit 7\n')
        (pipelines / "ssa_ending.sh").chmod(0o700)
        regress = self.root / "tools" / "regress"
        regress.mkdir()
        (regress / "giants_press.py").write_text("raise SystemExit(9)\n")
        self.exe = self.root / "unused-executable"
        self.exe.write_text("#!/usr/bin/env bash\nexit 97\n")
        self.exe.chmod(0o700)
        self.iso = self.root / "disc.iso"
        self.iso.touch()
        self.work = self.root / "work"

    def run_ending(self, game):
        env = os.environ.copy()
        env["TOUR_EXE"] = str(self.exe)
        return subprocess.run(["bash", str(self.script), game, str(self.iso), str(self.work)],
                              env=env, capture_output=True, text=True, timeout=10)

    def snapshot(self):
        return {str(path.relative_to(self.work)): path.read_bytes() if path.is_file() else None
                for path in self.work.rglob("*")}

    def test_nonempty_unmarked_work_is_untouched(self):
        self.work.mkdir()
        (self.work / ".hidden").write_text("keep hidden content\n")
        (self.work / "nested").mkdir()
        (self.work / "nested" / "keep.txt").write_text("keep nested content\n")
        before = self.snapshot()
        for game in ("giants", "ssa", "swapforce"):
            with self.subTest(game=game):
                run = self.run_ending(game)
                self.assertNotEqual(run.returncode, 0)
                self.assertIn("refusing non-empty work directory", run.stderr)
                self.assertEqual(self.snapshot(), before)

    def test_symlink_is_not_an_ownership_marker(self):
        self.work.mkdir()
        target = self.root / "keep.txt"
        target.write_text("keep content\n")
        (self.work / ".ending-work").symlink_to(target)
        run = self.run_ending("giants")
        self.assertNotEqual(run.returncode, 0)
        self.assertIn("refusing non-empty work directory", run.stderr)
        self.assertTrue((self.work / ".ending-work").is_symlink())
        self.assertEqual(target.read_text(), "keep content\n")

    def test_new_or_empty_work_is_marked_and_reusable(self):
        for existing in (False, True):
            with self.subTest(existing=existing):
                if existing:
                    shutil.rmtree(self.work)
                    self.work.mkdir()
                run = self.run_ending("swapforce")
                self.assertEqual(run.returncode, 2)
                self.assertIn("ending: SKIP", run.stdout)
                self.assertTrue((self.work / ".ending-work").is_file())
                (self.work / "previous.txt").write_text("old output\n")
                # Giants stops at the invented press helper, after recreating its work directory.
                run = self.run_ending("giants")
                self.assertEqual(run.returncode, 9, run.stderr)
                self.assertFalse((self.work / "previous.txt").exists())
                self.assertTrue((self.work / ".ending-work").is_file())

    def test_marker_survives_failed_ssa_helper(self):
        for _ in range(2):
            run = self.run_ending("ssa")
            self.assertEqual(run.returncode, 7, run.stderr)
            self.assertTrue((self.work / ".ending-work").is_file())


if __name__ == "__main__":
    unittest.main(verbosity=2)
