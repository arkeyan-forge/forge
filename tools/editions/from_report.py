"""Turn a pasted edition report (Arkeyan-Builder --edition-report, the launcher's "Find my edition") into a release entry.

usage: python3 -I tools/editions/from_report.py <report.txt> [--write]
       python3 -I tools/editions/from_report.py --self-test

Prints the release entry for games/<id>/info.cpp and for the launcher's src/launcher/launcher_games.cpp, and the
game.txt lists with the new hashes appended (docs/tools/edition_finder.md). A header-only edition (the report's result is
"supported, the same game code as ...") is already accepted by every check: its entry only names it. A disc of other
code also has an "Addresses on this disc" section: --write then makes games/<id>/editions/<media ID>/addresses.h, the
game's addresses.h with this disc's addresses ("sure", "call graph", "likely"; a "likely" one is marked to check) and
every address not found commented out, for a build of that edition (-DARKEYAN_EDITION=<media ID>). The report holds ids
and hashes only; nothing here reads a disc.
"""
import os
import re
import sys

ROOT = __file__.rsplit("/tools/", 1)[0]
AREAS = ((0x000000FF, "North America"), (0x0000FF00, "Japan and Asia"), (0x00FF0000, "Europe and Australia"))


def parse(text):
    report = {"addresses": {}, "missing": []}
    section = None
    for line in text.splitlines():
        line = line.strip().strip("`")
        if line.startswith("Addresses on this disc:"):
            section = "addresses"
        elif line == "Not found:":
            section = "missing"
        elif line.startswith("- ") and section == "addresses":
            name, ours, theirs, confidence = line[2:].split(" ", 3)
            report["addresses"][name] = (int(ours, 16), int(theirs, 16), confidence)
        elif line.startswith("- ") and section == "missing":
            report["missing"].append(line[2:].split(" ", 1)[0])
        elif ": " in line and not line.startswith("- "):
            key, value = line.split(": ", 1)
            report[key] = value
    match = re.match(r"(.*) \((\w+)\)$", report.get("Game", ""))
    if not match:
        raise SystemExit("error: the report names no game of the kit (its Game: line)")
    report["name"], report["id"] = match.groups()
    report["xex"] = re.search(r"SHA-256 ([0-9a-f]{64})", report["default.xex"]).group(1)
    image = re.search(r"SHA-256 ([0-9a-f]{64})", report.get("Loaded image", ""))
    if not image:
        raise SystemExit("error: the report has no loaded image hash (its executable could not be decrypted)")
    report["image"] = image.group(1)
    a, b, c, d = (int(part) for part in report["Version"].split("."))
    report["version"] = a << 28 | b << 24 | c << 8 | d
    region = re.match(r"([0-9A-F]{8})", report.get("Region", ""))
    report["label"] = label(int(region.group(1), 16) if region else None)
    return report


def label(region):
    if region is None:
        return "Other region"
    if region == 0xFFFFFFFF:
        return "Region free"
    names = [name for mask, name in AREAS if region & mask]
    return ", ".join(names) or "Other region"


def entries(report):
    same = re.match(r"supported, the same game code as (.*) \(only", report.get("Result", ""))
    note = (f"the same loaded image as {same.group(1)}: only the XEX header differs" if same
            else "another game code: its addresses in editions/%s/addresses.h" % report["Media ID"])
    release = '{ "%s", 0x%s, 0x%s, 0x%08X,\n            "%s",\n            "%s" },' % (
        report["label"], report["Title ID"], report["Media ID"], report["version"], report["xex"], report["image"])
    return (f"// games/{report['id']}/info.cpp and src/launcher/launcher_games.cpp (the {report['name']} releases):\n"
            f"        // {report['label']} edition, from a player's report ({note}).\n        {release}\n"
            f"// games/{report['id']}/game.txt: append to each list\n"
            f"xex_sha256=...,{report['xex']}\nimage_sha256=...,{report['image']}\nreleases=...;{report['label']}\n")


def edition_header(header, report):
    """The game's addresses.h with this disc's addresses."""
    found, missing = report["addresses"], set(report["missing"])
    out = [f"// {report['name']}, {report['label']} edition (media ID {report['Media ID']}): games/{report['id']}/"
           "addresses.h with this disc's addresses, made by tools/editions/from_report.py from its edition report.\n"
           "// Every address below was found by its fingerprint (docs/tools/edition_finder.md); \"check\" marks a likely one.\n"]
    lines = header.splitlines(keepends=True)
    i = 0
    while i < len(lines):
        # A definition with its continued lines.
        block = lines[i]
        while block.rstrip("\n").endswith("\\") and i + 1 < len(lines):
            i += 1
            block += lines[i]
        i += 1
        hook = re.search(r"#define\s+(GAME_HOOK_\w+)\s+(?:\\\s*)?sub_([0-9A-F]{8})", block)
        names = [hook.group(1)] if hook else re.findall(r"(\w+)\s*=\s*0x[0-9A-Fa-f]{8}\b", block) if "constexpr" in block else []
        for name in names:
            if name in found:
                ours, theirs, confidence = found[name]
                block = block.replace("%08X" % ours, "%08X" % theirs).replace("%08x" % ours, "%08X" % theirs)
                if confidence not in ("sure", "call graph"):
                    block = block.rstrip("\n") + f" // check: {confidence}\n"
            elif name in missing:
                block = "".join("// NOT FOUND on this edition: " + part for part in block.splitlines(keepends=True))
        out.append(block)
    return "".join(out)


def self_test():
    report = parse("""Edition report (Arkeyan-Builder 0.9.5)
Game: Skylanders Trap Team (trapteam)
Title ID: 41560911
Media ID: 44D8BCCF
Version: 0.0.0.14
Region: 000000FF (North America)
Languages: text en, fr; voices en, fr
default.xex: 1000 bytes, SHA-256 """ + "a" * 64 + """
Loaded image: 4096 bytes at 82000000, SHA-256 """ + "b" * 64 + """
Supported releases:
- Europe: different game code
Result: not supported yet: the game code differs from every supported release
Addresses on this disc: 2 of 3 found (the supported release's, then this disc's)
- GAME_HOOK_A 82100000 82100040 sure
- Global 82800000 82800100 likely
Not found:
- GAME_HOOK_B 82200000 not found
""")
    assert report["id"] == "trapteam" and report["label"] == "North America" and report["version"] == 0x0E
    text = entries(report)
    assert '{ "North America", 0x41560911, 0x44D8BCCF, 0x0000000E,' in text and "b" * 64 in text
    header = ("#pragma once\n#define GAME_HOOK_A sub_82100000 // a\n#define GAME_HOOK_B \\\n    sub_82200000\n"
              "namespace game\n{\n    constexpr uint32_t Global = 0x82800000;\n    constexpr uint32_t Count = 12;\n}\n")
    made = edition_header(header, report)
    assert "#define GAME_HOOK_A sub_82100040 // a\n" in made
    assert "// NOT FOUND on this edition: #define GAME_HOOK_B \\\n// NOT FOUND on this edition:     sub_82200000\n" in made
    assert "constexpr uint32_t Global = 0x82800100; // check: likely\n" in made and "Count = 12;" in made
    header_only = parse(text_of_header_only())
    assert "only the XEX header differs" in entries(header_only)
    print("from_report: 8 checks passed")


def text_of_header_only():
    return ("Game: Skylanders Swap Force (swapforce)\nTitle ID: 415608E9\nMedia ID: 12345678\nVersion: 0.0.0.14\n"
            "Region: 000000FF (North America)\ndefault.xex: 1 bytes, SHA-256 " + "c" * 64 + "\nLoaded image: 2 bytes at "
            "82000000, SHA-256 " + "d" * 64 + "\nResult: supported, the same game code as USA, Australia (only the disc "
            "header differs)\n")


def main(argv):
    if argv[1:] == ["--self-test"]:
        return self_test()
    if len(argv) < 2:
        raise SystemExit(__doc__)
    report = parse(open(argv[1], encoding="utf-8").read())
    print(entries(report), end="")
    if report["addresses"] or report["missing"]:
        found = sum(1 for _, _, c in report["addresses"].values() if not c.startswith("ambiguous"))
        print(f"// {found} addresses found, {len(report['missing'])} not found")
        if "--write" in argv:
            header = open(f"{ROOT}/games/{report['id']}/addresses.h", encoding="utf-8").read()
            path = f"{ROOT}/games/{report['id']}/editions/{report['Media ID']}/addresses.h"
            os.makedirs(os.path.dirname(path), exist_ok=True)
            open(path, "w", encoding="utf-8").write(edition_header(header, report))
            print(f"// wrote {path[len(ROOT) + 1:]}: build it with -DARKEYAN_EDITION={report['Media ID']}")


if __name__ == "__main__":
    main(sys.argv)
