#!/usr/bin/env python3
"""Validate public kit archives and attach a complete set to an existing launcher draft release."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import tempfile
import zipfile

TARGETS = ("windows-x64", "macos-arm64", "linux-x64")
NAMES = tuple(f"skylanders-kit-{target}.zip" for target in TARGETS)
GAMES = ("ssa", "giants", "swapforce", "trapteam", "superchargers", "imaginators")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def destination(repository, tag):
    require(re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9-]*/[A-Za-z0-9][A-Za-z0-9_.-]*", repository),
            "invalid launcher repository")
    require(re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+", tag), "launcher tag must be vX.Y.Z")


def sums(text):
    result = {}
    for line in text.splitlines():
        match = re.fullmatch(r"([0-9a-f]{64}) [ *]([^/\\\r\n]+)", line)
        require(match is not None, "malformed SHA256SUMS line")
        digest, name = match.groups()
        require(name not in result, "duplicate checksum entry")
        result[name] = digest
    return result


def sha256(stream):
    digest = hashlib.sha256()
    for block in iter(lambda: stream.read(1024 * 1024), b""):
        digest.update(block)
    return digest.hexdigest()


def inventory(path):
    require(path.name in NAMES, "unexpected kit archive name")
    root = path.stem
    with zipfile.ZipFile(path) as archive:
        files = {}
        seen = set()
        for entry in archive.infolist():
            parts = PurePosixPath(entry.filename).parts
            require(parts and parts[0] == root and ".." not in parts and "\\" not in entry.filename
                    and all(ord(character) >= 32 and ord(character) != 127 for character in entry.filename),
                    "unsafe kit archive path")
            require(entry.filename == "/".join(parts) + ("/" if entry.is_dir() else ""),
                    "noncanonical kit archive path")
            require(entry.filename not in seen, "duplicate kit archive entry")
            seen.add(entry.filename)
            mode = entry.external_attr >> 16
            require(not stat.S_ISLNK(mode), "symlink in kit archive")
            if entry.is_dir():
                continue
            require(len(parts) > 1, "kit root must be a directory")
            name = "/".join(parts[1:])
            require(parts[1] in {"Arkeyan-Builder", "Arkeyan-Builder.exe", "README.txt", "kit.txt", "files.sha256",
                                 "bin", "config", "share", "include", "res", "licenses", "lib", "commands",
                                 "relink"},
                    "unexpected kit top-level entry")
            require(name and not re.search(r"\.(xex|iso|lsa)$|(^|/)(image\.bin|shader_cache\.cpp|"
                                           r"ppc_recomp[^/]*|fingerprints\.txt)$", name), "forbidden kit input")
            require(not name.endswith(("guest_layout_record.cpp.o", "guest_layout_record.cpp.obj")),
                    "layout record object in kit")
            files[name] = entry
        require("files.sha256" in files and "kit.txt" in files, "kit manifest missing")
        # files.sha256 permits subdirectories, unlike release SHA256SUMS.
        listed = {}
        for line in archive.read(files["files.sha256"]).decode("utf-8").splitlines():
            match = re.fullmatch(r"([0-9a-f]{64}) [ *](.+)", line)
            require(match is not None, "malformed files.sha256 line")
            digest, name = match.groups()
            require(name not in listed, "duplicate kit file checksum")
            listed[name] = digest
        require(set(listed) == set(files) - {"files.sha256"}, "kit file inventory differs from manifest")
        for name, digest in listed.items():
            with archive.open(files[name]) as stream:
                require(sha256(stream) == digest, "kit file checksum mismatch")
        metadata = {}
        for line in archive.read(files["kit.txt"]).decode("utf-8").splitlines():
            key, separator, value = line.partition("=")
            require(separator and key not in metadata, "malformed kit metadata")
            metadata[key] = value
        require(metadata.get("platform") == root.removeprefix("skylanders-kit-"), "wrong kit platform")
        require(metadata.get("games", "").split(",") == list(GAMES), "kit must contain all six games in order")
        require(metadata.get("layout_record") == "1", "candidate needs layout_record=1")
        require(metadata.get("kit_format") == "2", "candidate needs kit_format=2")
        require(not any(key.startswith(("ppc_config_sha256.", "ppc_policy_sha256."))
                        or key in {"ppc_context_sha256", "public_headers"} for key in metadata),
                "legacy header metadata in format 2 kit")
        for key in ("abi.context", *(f"abi.policy.{game}" for game in GAMES)):
            require(re.fullmatch(r"[0-9a-f]{64}", metadata.get(key, "")), "public ABI id missing or invalid")
        versions = {game: metadata.get(f"game_version.{game}", "") for game in GAMES}
        require(all(re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", value) for value in versions.values()),
                "missing or invalid game version")
        print(f"Verified {path.name}: {len(files)} files")
        return versions


def gh(*arguments):
    return subprocess.run(["gh", *arguments], check=True, capture_output=True, text=True).stdout


def asset_revisions(release):
    # Download counters change during verification; identity, content timestamps and sizes must stay fixed.
    return {asset["name"]: (asset.get("id"), asset.get("updated_at"), asset.get("size")) for asset in release["assets"]}


def publish(repository, tag, folder):
    destination(repository, tag)
    require({path.name for path in folder.iterdir()} == set(NAMES), "expected exactly the three kit ZIPs")
    versions = [inventory(folder / name) for name in NAMES]
    require(all(version == versions[0] for version in versions), "kits disagree on game versions")
    release = json.loads(gh("api", f"repos/{repository}/releases/tags/{tag}"))
    require(release["draft"] and release["tag_name"] == tag, "target release must already exist and be a draft")
    assets = {asset["name"] for asset in release["assets"]}
    require("SHA256SUMS" in assets, "launcher assets and SHA256SUMS must be attached first")
    Path(".scratch/ci").mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=".scratch/ci") as temporary:
        work = Path(temporary)
        gh("release", "download", tag, "--repo", repository, "--pattern", "SHA256SUMS", "--dir", str(work))
        checksums = sums((work / "SHA256SUMS").read_text())
        launcher_assets = set(checksums) - set(NAMES)
        require(launcher_assets and launcher_assets <= assets, "launcher assets or checksum entries missing")
        for name in NAMES:
            with (folder / name).open("rb") as stream:
                digest = sha256(stream)
            if name in assets:
                existing = work / "existing"
                existing.mkdir(exist_ok=True)
                gh("release", "download", tag, "--repo", repository, "--pattern", name, "--dir", str(existing))
                with (existing / name).open("rb") as stream:
                    require(sha256(stream) == digest,
                            "draft already has a different kit; remove it explicitly before retrying")
            checksums[name] = digest
        (work / "SHA256SUMS").write_text("".join(f"{digest}  {name}\n" for name, digest in sorted(checksums.items())))
        games = "<!-- games: " + " ".join(f"{game}={versions[0][game]}" for game in GAMES) + " -->"
        notes = re.sub(r"<!-- games:.*?-->", "", release.get("body") or "", flags=re.S).rstrip()
        (work / "notes.md").write_text(notes + "\n\n" + games + "\n")
        # Recheck immediately before mutation; the owner must leave this draft unpublished throughout the job.
        current = json.loads(gh("api", f"repos/{repository}/releases/{release['id']}"))
        require(current["draft"], "release was published while candidates were checked")
        require(asset_revisions(current) == asset_revisions(release) and current.get("body") == release.get("body"),
                "draft changed while candidates were checked; retry after the other release writer finishes")
        missing = [str(folder / name) for name in NAMES if name not in assets]
        if missing:
            gh("release", "upload", tag, "--repo", repository, *missing)
        gh("release", "upload", tag, "--repo", repository, str(work / "SHA256SUMS"), "--clobber")
        gh("release", "edit", tag, "--repo", repository, "--notes-file", str(work / "notes.md"))
        # Read back the assets, including SHA256SUMS, before declaring the draft ready for owner review.
        verify = work / "verify"
        verify.mkdir()
        for name in (*NAMES, "SHA256SUMS"):
            gh("release", "download", tag, "--repo", repository, "--pattern", name, "--dir", str(verify))
        require(sums((verify / "SHA256SUMS").read_text()) == checksums, "uploaded SHA256SUMS differs")
        for name in NAMES:
            with (verify / name).open("rb") as stream:
                require(sha256(stream) == checksums[name],
                        "uploaded kit checksum mismatch")
        require(json.loads(gh("api", f"repos/{repository}/releases/{release['id']}"))["draft"],
                "release must remain a draft for owner review")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("destination")
    check = commands.add_parser("inventory")
    check.add_argument("archive", type=Path)
    attach = commands.add_parser("publish")
    attach.add_argument("folder", type=Path)
    args = parser.parse_args()
    if args.command == "inventory":
        inventory(args.archive)
    elif args.command == "destination":
        destination(os.environ.get("KITS_LAUNCHER_REPO", ""), os.environ.get("LAUNCHER_TAG", ""))
    else:
        publish(os.environ.get("KITS_LAUNCHER_REPO", ""), os.environ.get("LAUNCHER_TAG", ""), args.folder)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError, zipfile.BadZipFile, subprocess.CalledProcessError) as error:
        # Subprocess output can contain credentials or runner paths; keep failure messages local to this helper.
        if isinstance(error, ValueError):
            print(f"error: {error}")
        else:
            print("error: kit validation or release operation failed")
        raise SystemExit(1)
