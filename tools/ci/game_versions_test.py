"""tools/ci/game_versions.py on an invented repository. Run: python3 -I tools/ci/game_versions_test.py"""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().with_name("game_versions.py")
GAMES = ("ssa", "giants", "swapforce", "trapteam", "superchargers", "imaginators")


class GameVersions(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.git("init", "-q")
        for game in GAMES:
            self.write(f"games/{game}/game.txt", f"id={game}\nversion=0.1.0\n")
        self.write("runtime/render.cpp", "a\n")
        self.commit("v1.0.0")

    def git(self, *arguments):
        subprocess.run(["git", "-c", "user.name=test", "-c", "user.email=test@invalid", *arguments], cwd=self.root,
                       check=True, capture_output=True)

    def write(self, path, text):
        (self.root / path).parent.mkdir(parents=True, exist_ok=True)
        (self.root / path).write_text(text)

    def commit(self, tag=None):
        self.git("add", "-A")
        self.git("commit", "-q", "-m", "change")
        if tag:
            self.git("tag", tag)

    def bump(self, *games):
        for game in games:
            self.write(f"games/{game}/game.txt", f"id={game}\nversion=0.1.1\n")

    def run_check(self):
        result = subprocess.run([sys.executable, "-I", str(SCRIPT)], cwd=self.root, capture_output=True, text=True)
        return result.returncode, result.stderr

    def test_runtime_change_without_bump_fails_for_every_game(self):
        self.write("runtime/render.cpp", "b\n")
        self.bump("trapteam")
        self.commit("v1.0.1")
        code, errors = self.run_check()
        self.assertEqual(code, 1)
        for game in set(GAMES) - {"trapteam"}:
            self.assertIn(f"{game}: 1 file(s) changed since v1.0.0", errors)
        self.assertNotIn("trapteam", errors)

    def test_change_with_bump_passes(self):
        self.write("runtime/render.cpp", "b\n")
        self.bump(*GAMES)
        self.commit()
        self.assertEqual(self.run_check(), (0, ""))

    def test_one_game_and_its_engine_only(self):
        self.write("games/swapforce/fix.cpp", "b\n")
        self.write("engines/lux/hook.cpp", "b\n")
        self.commit()
        code, errors = self.run_check()
        self.assertEqual(code, 1)
        self.assertIn("swapforce:", errors)
        self.assertIn("ssa:", errors)
        self.assertNotIn("giants", errors)

    def test_docs_tests_and_dev_tools_need_no_bump(self):
        for path in ("docs/a.md", "README.md", "runtime/render_test.cpp", "tools/ci/run.sh", "tools/regress/a.sh"):
            self.write(path, "b\n")
        self.commit()
        self.assertEqual(self.run_check()[0], 0)

    def test_builder_change_needs_a_bump(self):
        self.write("tools/arkeyan-builder/main.cpp", "b\n")
        self.commit()
        self.assertEqual(self.run_check()[0], 1)

    def test_lower_version_fails(self):
        self.write("games/giants/game.txt", "id=giants\nversion=0.0.9\n")
        self.commit()
        code, errors = self.run_check()
        self.assertEqual(code, 1)
        self.assertIn("giants: version 0.0.9 is lower than 0.1.0", errors)

    def test_tag_on_head_is_not_its_own_base(self):
        self.write("runtime/render.cpp", "b\n")
        self.commit("v1.0.1")
        self.git("tag", "v1.0.1-again")
        code, errors = self.run_check()
        self.assertEqual(code, 1)
        self.assertIn("since v1.0.0", errors)


if __name__ == "__main__":
    unittest.main()
