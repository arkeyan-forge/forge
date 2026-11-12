"""CLI/section checks with fake builders and units; no game or compiler is started."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class Sections(unittest.TestCase):
    def setUp(self):
        scratch = ROOT / ".scratch" / "installguard"
        scratch.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=scratch)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "tools/regress").mkdir(parents=True)
        shutil.copyfile(ROOT / "tools/test_all.sh", self.root / "tools/test_all.sh")
        (self.root / "tools/regress/giants_press.py").write_text("print('')\n")
        (self.root / "tools/regress/f3mouse_unit.py").write_text("print('')\n")
        (self.root / "tools/regress/pid_timer_test.sh").write_text("exit 0\n")
        (self.root / "tools/regress/movie_targets.py").write_text("print('')\n")
        bindir = self.root / "bin"
        bindir.mkdir()
        cmake = bindir / "cmake"
        cmake.write_text('''#!/bin/sh
printf 'called\\n' >> cmake-called
[ "$1" = --build ] || exit 0
dir=$2
mkdir -p "$dir"
while [ "$1" != --target ]; do shift; done
shift
for target do
    case $target in
        SSA*Test) printf '#!/bin/sh\\necho "1 checks, 0 failures"\\n' > "$dir/$target"; chmod +x "$dir/$target" ;;
    esac
done
''')
        cmake.chmod(0o755)
        self.env = dict(os.environ, PATH=f"{bindir}:{os.environ['PATH']}")

    def run_cli(self, *args):
        return subprocess.run(["bash", "tools/test_all.sh", *args], cwd=self.root,
                              env=self.env, capture_output=True, text=True)

    def test_unknown_and_missing_sections_fail_before_any_work(self):
        for args in (("--only", "typo"), ("--only",), ("--only", ""), ("--only", "--quick")):
            with self.subTest(args=args):
                result = self.run_cli(*args)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertNotIn("ALL PASS", result.stdout)
                self.assertFalse((self.root / "cmake-called").exists())
                self.assertFalse((self.root / ".scratch/regress").exists())

    def test_trapteam_units_run_and_runtime_coverage_is_explicit(self):
        disc = self.root / "invented.iso"
        disc.write_bytes(b"not a game")
        self.env["TRAPTEAM_ISO"] = str(disc)
        result = self.run_cli("--only", "trapteam")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("##### Trap Team", result.stdout)
        self.assertIn("unit ArkeyanProgressTest", result.stdout)
        self.assertIn("SKIP", result.stdout)
        self.assertIn("runtime", result.stdout)
        self.assertNotIn("##### SSA", result.stdout)

    def test_trapteam_without_disc_is_a_visible_skip(self):
        self.env.pop("TRAPTEAM_ISO", None)
        result = self.run_cli("--only", "trapteam", "--quick")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("SKIP", result.stdout)
        self.assertIn("TRAPTEAM_ISO", result.stdout)
        self.assertFalse((self.root / "cmake-called").exists())


if __name__ == "__main__":
    unittest.main()
