#!/usr/bin/env python3
"""Check that COVERAGE_PIPELINES keeps each run's pipeline list before the run folder is deleted, without a game.

  python3 -I tools/regress/coverage_pipelines_test.py
"""
import importlib.util
import os
from pathlib import Path
import tempfile
import unittest

HERE = Path(__file__).resolve().parent


def load(pipelines):
    os.environ["COVERAGE_PIPELINES"] = pipelines
    spec = importlib.util.spec_from_file_location("coverage_all", HERE / "coverage_all.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class CoveragePipelinesTest(unittest.TestCase):
    def run_folder(self, base, with_list=True):
        rundir = Path(base) / "runs" / "zone"
        (rundir / "save").mkdir(parents=True)
        (rundir / "game.log").write_text("[level] zone\n")
        if with_list:
            (rundir / "save" / "pipelines.bin").write_bytes(b"SSAP" + bytes(8) + b"record")
        return rundir

    def test_list_kept_then_run_deleted(self):
        with tempfile.TemporaryDirectory() as base:
            kept = Path(base) / "kept"
            coverage = load(str(kept))
            rundir = self.run_folder(base)
            coverage.keep(str(rundir), str(Path(base) / "superchargers"), "zone-sky1/sky1", "")
            self.assertEqual((kept / "superchargers" / "zone-sky1_sky1.bin").read_bytes()[-6:], b"record")
            self.assertFalse(rundir.exists())

    def test_nothing_kept_without_the_variable_or_a_list(self):
        with tempfile.TemporaryDirectory() as base:
            coverage = load("")
            rundir = self.run_folder(base)
            coverage.keep(str(rundir), str(Path(base) / "imaginators"), "zone-hub", "")
            self.assertFalse(rundir.exists())
            kept = Path(base) / "kept"
            coverage = load(str(kept))
            rundir = self.run_folder(base, with_list=False)
            coverage.keep(str(rundir), str(Path(base) / "imaginators"), "zone-hub", "")
            self.assertFalse(kept.exists())


if __name__ == "__main__":
    unittest.main()
