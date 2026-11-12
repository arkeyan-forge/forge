#!/usr/bin/env python3
"""Trap Team tour checks on invented disc directories and the public catalogue; never starts a game."""
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import trapteam

ROOT = trapteam.ROOT


def directory(entries):
    data = bytearray()
    for index, (name, sector, size, attr) in enumerate(entries):
        name = name.encode("ascii")
        length = (14 + len(name) + 3) & ~3
        following = (len(data) + length) // 4 if index + 1 < len(entries) else 0
        data += struct.pack("<HHIIBB", 0, following, sector, size, attr, len(name)) + name
        data += bytes((-len(data)) % 4)
    return data


def invented_disc(path):
    # These invented names exercise case folding, extension matching and support-pack filtering.
    files = directory([(name, 50, 1, 0) for name in (
        "Level_900_Sample.arc", "level_900_sample.bld", "challenge_level_900.arc",
        "title.arc", "challengevo.arc", "challengemesh_test.arc", "sample_dialogue.arc",
        "sample_globalincludes.arc", "sample_music_common.arc", "arena_challenge_900_000.arc",
        "arena_challenge_900_001.arc", "not_a_level.txt")])
    root = directory([("LeVeL", 41, len(files), 0x10)])
    data = bytearray(52 * trapteam.xdvdfs.SECTOR)
    offset = 32 * trapteam.xdvdfs.SECTOR
    data[offset:offset + 20] = trapteam.xdvdfs.MAGIC
    struct.pack_into("<II", data, offset + 20, 40, len(root))
    data[40 * trapteam.xdvdfs.SECTOR:40 * trapteam.xdvdfs.SECTOR + len(root)] = root
    data[41 * trapteam.xdvdfs.SECTOR:41 * trapteam.xdvdfs.SECTOR + len(files)] = files
    path.write_bytes(data)


class Tours(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        scratch = ROOT / "private/runs"
        scratch.mkdir(parents=True, exist_ok=True)
        cls.temporary = tempfile.TemporaryDirectory(prefix="trapteam-unit-", dir=scratch)
        cls.work = Path(cls.temporary.name)
        cls.disc = cls.work / "invented.iso"
        invented_disc(cls.disc)
        cls.catalogue = json.loads((ROOT / "resources/collection/catalogue.json").read_text())

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def test_disc_listing_and_no_extraction(self):
        before = self.disc.read_bytes()
        self.assertEqual(trapteam.levels(self.disc), ["arena_challenge_900_000", "challenge_level_900", "level_900_sample"])
        self.assertEqual(self.disc.read_bytes(), before)
        self.assertEqual(list(self.work.iterdir()), [self.disc])

    def test_list_cli_needs_no_game_or_work_directory(self):
        environment = dict(os.environ, TOUR_EXE=str(self.work / "no-game-executable"))
        destination = self.work / "unused"
        commands = [
            ["bash", str(ROOT / "tools/pipelines/tour_trapteam.sh"), "--list", str(self.disc)],
            ["bash", str(ROOT / "tools/pipelines/tour_trapteam.sh"), str(self.disc), str(destination), "--list"],
            ["bash", str(HERE / "coverage.sh"), "trapteam", str(self.disc), str(destination), "--list"],
        ]
        for command in commands:
            result = subprocess.run(command, env=environment, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout.splitlines(), trapteam.levels(self.disc))
            self.assertFalse(destination.exists())

    def test_invalid_disc_is_an_error(self):
        with tempfile.NamedTemporaryFile(dir=self.work) as stream:
            stream.write(bytes(4096))
            stream.flush()
            with self.assertRaises(SystemExit):
                trapteam.levels(stream.name)

    def test_work_directory_rejects_escape_and_scratch_root(self):
        self.assertEqual(trapteam.work_directory(self.work / "runs"), self.work / "runs")
        for path in (ROOT, ROOT / "private", ROOT / ".scratch/test", ROOT / "private/../../escape"):
            with self.assertRaises(ValueError):
                trapteam.work_directory(path)
        link = self.work / "escape"
        link.symlink_to(ROOT, target_is_directory=True)
        try:
            with self.assertRaises(ValueError):
                trapteam.work_directory(link / "outside")
        finally:
            link.unlink()

    def test_every_accepted_type_is_represented(self):
        game = next(g for g in self.catalogue["games"] if g["id"] == "trapteam")
        expected = {t["type"] for t in self.catalogue["toys"] if t["game"] in game["accepts_games"]}
        rows = trapteam.figure_rows(self.catalogue)
        actual = {int(t) for row in rows for t in row[3].split("+")}
        self.assertEqual(actual, expected)
        self.assertEqual(len({row[0] for row in rows}), len(rows))
        traps = {str(t["type"]) for t in self.catalogue["toys"] if t["category"] == "trap" and t["game"] == "trapteam"}
        self.assertTrue(traps)
        self.assertTrue(all(kind == "skip" for _, kind, _, types in rows if types in traps))
        self.assertEqual({types for _, kind, _, types in rows if kind == "skip" and types in traps}, traps)

    def test_elite_mini_and_trap_master_tags(self):
        rows = {row[0]: row for row in trapteam.figure_rows(self.catalogue)}
        for toy in self.catalogue["toys"]:
            if toy["game"] == "trapteam" and toy["category"] in ("mini", "trap_master"):
                self.assertEqual(rows[str(toy["type"])][1], "core")
            for variant in toy.get("variants", []):
                if "id" not in variant:
                    continue
                key = f'{toy["type"]}:{variant["id"][2:]}'
                if variant["kind"] == "eons_elite":
                    if int(variant["id"], 16) >> 12 <= 3:
                        self.assertEqual(rows[key][1], "core")
                    else:
                        self.assertNotIn(key, rows)

    def test_subsets_swappers_and_unknown_requests(self):
        top = next(t for t in self.catalogue["toys"] if t["category"] == "swap_top")
        t = top["type"]
        rows = trapteam.figure_rows(self.catalogue, [str(t - 1000)])
        self.assertTrue(rows)
        self.assertTrue(all(row[3] == f"{t}+{t - 1000}" and row[1] == "swap" for row in rows))
        for key, _, _, _ in rows:
            self.assertIn(":", key)
        self.assertEqual(trapteam.figure_rows(self.catalogue, ["not-a-tag"])[0][1], "skip")
        self.assertTrue(all(row[0] == "0:3810" for row in trapteam.figure_rows(self.catalogue, ["0:3810"])))


if __name__ == "__main__":
    unittest.main()
