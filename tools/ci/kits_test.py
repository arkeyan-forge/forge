"""Archive and draft-release checks with invented files and a fake release service; no network or programs run."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import zipfile

SPEC = importlib.util.spec_from_file_location("kits", Path(__file__).with_name("kits.py"))
kits = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(kits)


class Kits(unittest.TestCase):
    def setUp(self):
        scratch = Path(".scratch/ci-tests")
        scratch.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=scratch)
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.candidates = self.root / "candidates"
        self.candidates.mkdir()

    def kit(self, target, change=None):
        name = f"skylanders-kit-{target}"
        metadata = f"platform={target}\ngames={','.join(kits.GAMES)}\nkit_format=2\nlayout_record=1\n"
        metadata += "".join(f"game_version.{game}=1.2.3\n" for game in kits.GAMES)
        metadata += "abi.context=" + "b" * 64 + "\n"
        metadata += "".join(f"abi.policy.{game}=" + "c" * 64 + "\n" for game in kits.GAMES)
        files = {"kit.txt": metadata.encode(), "README.txt": b"invented fixture\n",
                 "relink/README.txt": b"invented relink fixture\n"}
        if change:
            change(files)
        manifest = "".join(f"{hashlib.sha256(data).hexdigest()}  {path}\n" for path, data in files.items())
        files["files.sha256"] = manifest.encode()
        path = self.candidates / f"{name}.zip"
        with zipfile.ZipFile(path, "w") as archive:
            for filename, data in files.items():
                archive.writestr(f"{name}/{filename}", data)
        return path

    def test_archive_inventory_and_corruption(self):
        path = self.kit("linux-x64")
        self.assertEqual(kits.inventory(path), dict.fromkeys(kits.GAMES, "1.2.3"))
        with zipfile.ZipFile(path, "a") as archive:
            archive.writestr(f"{path.stem}/res/extra.txt", "unlisted")
        with self.assertRaisesRegex(ValueError, "inventory differs"):
            kits.inventory(path)
        self.kit("linux-x64")
        with zipfile.ZipFile(path, "r") as archive:
            files = {name: archive.read(name) for name in archive.namelist()}
        files[f"{path.stem}/README.txt"] = b"changed\n"
        with zipfile.ZipFile(path, "w") as archive:
            for name, data in files.items():
                archive.writestr(name, data)
        with self.assertRaisesRegex(ValueError, "checksum mismatch"):
            kits.inventory(path)

    def test_rejects_traversal_symlinks_and_unwanted_inputs(self):
        for filename in ("../escape", "/absolute", "config/ssa/fingerprints.txt", "image.bin",
                         "lib/ssa/runtime/guest_layout_record.cpp.o"):
            with self.subTest(filename=filename):
                path = self.kit("linux-x64", lambda files: files.update({filename: b"invented"}))
                with self.assertRaises(ValueError):
                    kits.inventory(path)
        path = self.kit("linux-x64")
        with zipfile.ZipFile(path, "a") as archive:
            entry = zipfile.ZipInfo(f"{path.stem}/link")
            entry.external_attr = 0o120777 << 16
            archive.writestr(entry, "README.txt")
        with self.assertRaisesRegex(ValueError, "symlink"):
            kits.inventory(path)

    def test_format_2_requires_layout_and_every_public_abi_id(self):
        for key in ("kit_format", "layout_record", "abi.context", *(f"abi.policy.{game}" for game in kits.GAMES)):
            with self.subTest(key=key):
                def remove(files):
                    files["kit.txt"] = b"\n".join(line for line in files["kit.txt"].split(b"\n")
                                                  if not line.startswith(key.encode() + b"="))
                with self.assertRaises(ValueError):
                    kits.inventory(self.kit("linux-x64", remove))
        for before, after in ((b"kit_format=2", b"kit_format=3"), (b"layout_record=1", b"layout_record=0"),
                              (b"abi.context=" + b"b" * 64, b"abi.context=invalid")):
            with self.subTest(after=after):
                with self.assertRaises(ValueError):
                    kits.inventory(self.kit("linux-x64", lambda files: files.update({
                        "kit.txt": files["kit.txt"].replace(before, after)})))

    def test_format_2_rejects_obsolete_header_fields(self):
        for key in ("public_headers", "ppc_context_sha256", "ppc_config_sha256.ssa", "ppc_policy_sha256.ssa"):
            with self.subTest(key=key):
                with self.assertRaisesRegex(ValueError, "legacy header metadata"):
                    kits.inventory(self.kit("linux-x64", lambda files: files.update({
                        "kit.txt": files["kit.txt"] + f"{key}=invented\n".encode()})))

    @unittest.skipUnless(all(shutil.which(program) for program in ("unzip", "shasum", "strings")),
                         "archive scanning tools unavailable")
    def test_check_helper_scans_and_reextracts_without_running_kit_programs(self):
        helper = self.root / "tools/ci/check_kit.sh"
        helper.parent.mkdir(parents=True)
        for name in ("check_kit.sh", "kits.py"):
            shutil.copyfile(Path(__file__).with_name(name), helper.with_name(name))
        shutil.copy(Path(__file__).parents[1] / "kit_scan.sh", helper.parent.parent / "kit_scan.sh")
        path = self.kit("windows-x64")
        distribution = self.root / "dist"
        distribution.mkdir()
        shutil.copyfile(path, distribution / path.name)
        with zipfile.ZipFile(path) as archive:
            archive.extractall(distribution)
        words = self.root / "scan-words.txt"
        words.write_text("synthetic_scan_sentinel\n")
        temporary = self.root / "temporary"
        temporary.mkdir()
        environment = dict(os.environ, KIT_SCAN_USER="synthetic_scan_sentinel", KIT_SCAN_WORDS=str(words.resolve()),
                           TMPDIR=str(temporary.resolve()))
        result = subprocess.run(["sh", "tools/ci/check_kit.sh", "windows-x64"], cwd=self.root,
                                env=environment, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout.count("kit_scan: clean:"), 2)
        self.assertFalse(list((self.root / ".scratch/ci").iterdir()))

    def test_destination_and_checksum_validation(self):
        kits.destination("release-owner/launcher", "v1.2.3")
        for repository, tag in (("../bad", "v1.2.3"), ("release-owner/launcher", "v1.2.3; command")):
            with self.assertRaises(ValueError):
                kits.destination(repository, tag)
        line = "a" * 64 + "  Launcher.zip\n"
        self.assertEqual(kits.sums(line), {"Launcher.zip": "a" * 64})
        for invalid in (line + line, "broken", "a" * 64 + "  ../escape\n"):
            with self.assertRaises(ValueError):
                kits.sums(invalid)

    def test_toolchain_checksum_precedes_extraction(self):
        helper = self.root / "tools/ci/fetch_llvm_mingw.sh"
        helper.parent.mkdir(parents=True)
        shutil.copyfile(Path(__file__).with_name(helper.name), helper)
        archive = "llvm-mingw-20260922-ucrt-ubuntu-22.04-x86_64.tar.xz"
        dependencies = self.root / "public-deps"
        downloads = dependencies / "toolchains/download"
        downloads.mkdir(parents=True)
        (downloads / archive).write_bytes(b"invented public toolchain fixture")
        binaries = self.root / "fake-bin"
        binaries.mkdir()
        scripts = {
            "uname": "printf 'Linux\\n'\n",
            "curl": "exit 90\n",
            "tar": '''test "$1" = -xJf && test "$3" = -C || exit 1
installation=$4/llvm-mingw-20260922-ucrt-ubuntu-22.04-x86_64
mkdir -p "$installation/bin"
printf 'invented compiler\\n' > "$installation/bin/x86_64-w64-mingw32-clang"
chmod +x "$installation/bin/x86_64-w64-mingw32-clang"
printf 'invented license\\n' > "$installation/LICENSE.TXT"
'''
        }
        for name, script in scripts.items():
            path = binaries / name
            path.write_text("#!/bin/sh\n" + script)
            path.chmod(0o755)
        environment = dict(os.environ, PATH=str(binaries.resolve()) + os.pathsep + os.environ["PATH"],
                           KIT_DEPS=str(dependencies.resolve()), LLVM_MINGW_ARCHIVE=archive, LLVM_MINGW_SHA256="0" * 64)
        installation = downloads.parent / archive.removesuffix(".tar.xz")
        result = subprocess.run(["sh", str(helper)], env=environment, capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(installation.exists())
        environment["LLVM_MINGW_SHA256"] = hashlib.sha256((downloads / archive).read_bytes()).hexdigest()
        result = subprocess.run(["sh", str(helper)], env=environment, capture_output=True)
        self.assertEqual(result.returncode, 0)
        self.assertTrue((installation / "LICENSE.TXT").is_file())
        self.assertEqual(Path(result.stdout.decode().strip()), installation.resolve())
        self.assertFalse((self.root / "private").exists())

    def service(self, draft=True):
        self.assets = {"SHA256SUMS": ("a" * 64 + "  Launcher.zip\n").encode(),
                       "Launcher.zip": b"invented launcher asset"}
        self.notes = "Release notes\n<!-- games: ssa=0.0.1 -->"
        self.calls = []

        def fake(*args):
            self.calls.append(args)
            if args[0] == "api":
                return json.dumps({"id": 7, "tag_name": "v1.2.3", "draft": draft, "body": self.notes,
                                   "assets": [{"name": name} for name in self.assets]})
            action = args[1]
            if action == "download":
                name = args[args.index("--pattern") + 1]
                folder = Path(args[args.index("--dir") + 1])
                (folder / name).write_bytes(self.assets[name])
            elif action == "upload":
                for filename in args[5:]:
                    if filename != "--clobber":
                        path = Path(filename)
                        self.assets[path.name] = path.read_bytes()
            elif action == "edit":
                self.notes = Path(args[args.index("--notes-file") + 1]).read_text()
            else:
                self.fail("unexpected release operation")
            return ""

        return patch.object(kits, "gh", fake)

    def test_publish_preserves_launcher_sums_and_updates_notes(self):
        for target in kits.TARGETS:
            self.kit(target)
        with self.service():
            kits.publish("release-owner/launcher", "v1.2.3", self.candidates)
            checksums = kits.sums(self.assets["SHA256SUMS"].decode())
            self.assertEqual(checksums["Launcher.zip"], "a" * 64)
            for name in kits.NAMES:
                self.assertEqual(checksums[name], hashlib.sha256(self.assets[name]).hexdigest())
            self.assertEqual(self.notes.count("<!-- games:"), 1)
            self.assertIn("ssa=1.2.3", self.notes)
            self.assertIn("Release notes", self.notes)
            kits.publish("release-owner/launcher", "v1.2.3", self.candidates)
            self.assets[kits.NAMES[0]] = b"different existing candidate"
            self.calls.clear()
            with self.assertRaisesRegex(ValueError, "different kit"):
                kits.publish("release-owner/launcher", "v1.2.3", self.candidates)
            self.assertFalse(any(call[:2] == ("release", "upload") for call in self.calls))

    def test_publish_fails_before_writes_for_published_release_or_disagreement(self):
        for target in kits.TARGETS:
            self.kit(target)
        with self.service(draft=False):
            with self.assertRaisesRegex(ValueError, "draft"):
                kits.publish("release-owner/launcher", "v1.2.3", self.candidates)
            self.assertFalse(any(call[:2] == ("release", "upload") for call in self.calls))
        path = self.kit("linux-x64", lambda files: files.update({"kit.txt": files["kit.txt"].replace(
            b"game_version.ssa=1.2.3", b"game_version.ssa=1.2.4")}))
        with self.service():
            with self.assertRaisesRegex(ValueError, "disagree"):
                kits.publish("release-owner/launcher", "v1.2.3", self.candidates)
            self.assertEqual(self.calls, [])
        path.unlink()
        with self.assertRaisesRegex(ValueError, "exactly"):
            kits.publish("release-owner/launcher", "v1.2.3", self.candidates)


if __name__ == "__main__":
    unittest.main()
