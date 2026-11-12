"""Checks of tools/regress/analyze.py that need no game: `analyze.py sharpness` on synthetic pictures. Run: python3 tools/regress/analyze_test.py"""
import os
import subprocess
import sys
import tempfile
import unittest

import numpy as np
from PIL import Image

ANALYZE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "analyze.py")


def run(*args):
    result = subprocess.run([sys.executable, ANALYZE, *args], capture_output=True, text=True)
    return result.returncode, result.stdout


class SharpnessTest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        rng = np.random.default_rng(1)
        noise = rng.integers(0, 256, (360, 640), dtype=np.uint8)
        self.sharp = self.save("sharp.ppm", noise)
        # the same picture averaged with its neighbour: softer, less detail
        soft = ((noise.astype(np.uint16) + np.roll(noise, 1, axis=1)) // 2).astype(np.uint8)
        self.soft = self.save("soft.ppm", soft)

    def tearDown(self):
        self.dir.cleanup()

    def save(self, name, grey):
        path = os.path.join(self.dir.name, name)
        Image.fromarray(np.stack([grey] * 3, axis=2)).save(path)
        return path

    def test_more_detail_passes_and_less_fails(self):
        code, text = run("sharpness", "20", self.soft, self.sharp)
        self.assertEqual(code, 0, text)
        self.assertIn("+", text)
        code, text = run("sharpness", "20", self.sharp, self.soft)
        self.assertEqual(code, 1, text)

    def test_the_same_picture_gains_nothing(self):
        code, text = run("sharpness", "0", self.sharp, self.sharp)
        self.assertEqual(code, 0, text)
        self.assertIn("+0.0 %", text)
        code, _ = run("sharpness", "1", self.sharp, self.sharp)
        self.assertEqual(code, 1)


if __name__ == "__main__":
    unittest.main()
