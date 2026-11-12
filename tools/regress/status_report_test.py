#!/usr/bin/env python3
"""Unit check for tools/regress/status_report.py with a small invented coverage file: no game run needed.

  tools/regress/status_report_test.py

Writes a two-game coverage.tsv in a temporary folder, runs the report with --subset --state-lines, and checks the page
(one table per game, counts, the subset note), the picture (Pillow), the state lines (one for the crash and its [guard]
line) and that no input path reaches the page. Exit code 0: pass.
"""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPORT = os.path.join(HERE, "status_report.py")


def coverage(path, game, rows):
    with open(path, "w") as out:
        out.write(f"# game {game}\nkind\tname\tverdict\tlowest_fps\tdetail\n")
        for row in rows:
            out.write("\t".join(row) + "\n")


def main():
    with tempfile.TemporaryDirectory() as tmp:
        ssa = os.path.join(tmp, "ssa.tsv")
        giants = os.path.join(tmp, "giants.tsv")
        coverage(ssa, "ssa", [
            ("level", "level_027", "ok", "30.0", ""),
            ("level", "level_021", "crash", "12.5", "[guard] bad indirect call at 0x1234"),
            ("figure", "Whirlwind", "not_reached", "0", "the game never read slot 0"),
            ("credits", "credits", "ok", "29.9", ""),
        ])
        coverage(giants, "giants", [
            ("level", "level_213_tutotial", "ok", "60.0", ""),
            ("figure", "Swarm", "ok", "59.5", ""),
        ])
        page = os.path.join(tmp, "out", "STATUS.md")
        png = os.path.join(tmp, "out", "status.png")
        run = subprocess.run([sys.executable, REPORT, ssa, giants, "--platform", "Test platform", "--build",
                              "test-build", "--out", page, "--png", png, "--subset", "--state-lines"],
                             capture_output=True, text=True)
        if run.returncode != 0:
            print(run.stderr)
            return 1
        text = open(page).read()
        checks = [
            ("two game tables", text.count("## ") == 2 and "Spyro's Adventure" in text and "Giants" in text),
            ("counts", "| Levels | 2 | 1 | 0 | 1 | 12.5 |" in text),
            ("subset note", "subset" in text.lower()),
            ("failure row", "level_021" in text and "crash" in text),
            ("no input path", tmp not in text and "coverage.tsv" not in text),
            ("no agent wording", "agent" not in text.lower() and " ai " not in text.lower()),
            ("picture", os.path.getsize(png) > 1000),
            ("state line", run.stdout.count("\n") == 1 and "level_021" in run.stdout and "[guard]" in run.stdout),
        ]
        # The catalogue mode (coverage_all.sh): the share covered is over the catalogue, not over what ran.
        work = os.path.join(tmp, "work")
        os.makedirs(os.path.join(work, "giants"))
        with open(os.path.join(work, "giants", "catalogue.tsv"), "w") as out:
            out.write("# game giants\narea\tkind\tname\troute\targ\tcap\treason\n")
            out.write("story\tlevel\tlevel_200\tlevel\tlevel_200\t\t\n")
            out.write("story\tlevel\tlevel_201\tlevel\tlevel_201\t\t\n")
            out.write("heroic challenges\tchallenge\tchallenge_level_016\tcali\t0\t\t\n")
            out.write("modes\tmode\tBattle Mode\tnone\t\t\tmenus not scripted\n")
        full = os.path.join(work, "giants", "coverage.tsv")
        with open(full, "w") as out:
            out.write("# game giants\nkind\tname\tverdict\tlowest_fps\tdetail\tarea\tseconds\n")
            out.write("level\tlevel_200\tok\t58.0\treached at 29 s\tstory\t67\n")
            out.write(f"challenge\tchallenge_level_016\thang\t0\tno frame for 45 s; last log lines: {tmp}/x\t"
                      "heroic challenges\t200\n")
            out.write("mode\tBattle Mode\tnot_testable\t0\tmenus not scripted\tmodes\t0\n")
        page2 = os.path.join(tmp, "out2", "STATUS.md")
        missing = os.path.join(tmp, "out2", "COVERAGE.md")
        run2 = subprocess.run([sys.executable, REPORT, full, "--platform", "Test platform", "--build", "b", "--out",
                               page2, "--png", os.path.join(tmp, "out2", "s.png"), "--catalogue", work, "--coverage",
                               missing], capture_output=True, text=True)
        text2 = open(page2).read() if run2.returncode == 0 else ""
        text3 = open(missing).read() if run2.returncode == 0 else ""
        checks += [
            ("catalogue table", "| story | 2 | 1 | 0 | 0 | 0 | 0 | 1 | 58.0 |" in text2),
            ("covered over the catalogue", "| All | 4 | 1 (25.0 %) | 0 | 1 | 0 | 1 | 1 |" in text2),
            ("coverage page", "- level_201: not run" in text3 and "- Battle Mode: menus not scripted" in text3
             and "- challenge_level_016: hang: no new frame for 45 s" in text3 and "level_200" not in text3),
            ("no path in the coverage page", tmp not in text3),
        ]
        bad = [name for name, good in checks if not good]
        for name, good in checks:
            print(("ok:   " if good else "FAIL: ") + name)
        if os.path.exists(png) and open(png, "rb").read(8) != b"\x89PNG\r\n\x1a\n":
            print("FAIL: picture is not a PNG")
            bad.append("picture is a PNG")
        if bad:
            print(f"status report test: {len(bad)} FAILED")
            return 1
        print("status report test: PASS")
        return 0


if __name__ == "__main__":
    sys.exit(main())
