#!/usr/bin/env python3
"""The public status page and picture from the coverage files of each game (tools/regress/coverage.sh, coverage_all.sh).

  tools/regress/status_report.py <coverage.tsv>... --platform <name> --build <id> --out docs/status/STATUS.md
                                 [--png docs/status/status.png] [--subset] [--state-lines]
                                 [--catalogue <work dir>] [--coverage docs/status/COVERAGE.md]

Each coverage.tsv starts with `# game <id>` and then the columns kind, name, verdict, lowest_fps, detail (coverage_all.sh
adds area and seconds). Without --catalogue the page has one table per game by kind (levels, figures, credits: checks,
passed, not reached, crashed, lowest frame rate). With --catalogue (coverage_all.sh's work dir, holding
<game>/catalogue.tsv) every table is the game's whole catalogue, by area: entries, ok, crash, hang, not reached, not
testable, not run, lowest frame rate, and the share covered is ok over the catalogue's entries (never over what was
run). --coverage writes the page of every entry not covered, one line each with its reason. A details table lists the
rows that did not pass; status.png is drawn with Pillow. --subset marks the page as a partial run. --state-lines
prints one line per crash or [guard] for office/STATE.md.

The page is published: it names the platform, the build, the level and figure names only. No paths, no host or account
names, no log text. Wording is factual; no tool framing.
"""
import datetime
import os
import re
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:  # the page can still be written; the picture is the one thing lost
    Image = None

GAMES = {"ssa": "Spyro's Adventure", "giants": "Giants", "swapforce": "Swap Force", "trapteam": "Trap Team",
         "superchargers": "SuperChargers", "imaginators": "Imaginators"}
ORDER = {"ssa": 0, "giants": 1, "swapforce": 2, "trapteam": 3, "superchargers": 4, "imaginators": 5}
FULL = ("ok", "crash", "hang", "not_reached", "not_testable")
AREAS = (("level", "Levels"), ("figure", "Figures"), ("credits", "Credits"))
COLORS = {"ok": "#2e7d32", "not_reached": "#f9a825", "crash": "#c62828", "hang": "#6a1b9a", "not_testable": "#9e9e9e"}
VERDICTS = ("ok", "not_reached", "crash", "hang", "not_testable")


def usage():
    print(__doc__.strip().split("\n\n")[1], file=sys.stderr)
    sys.exit(2)


def game_of(path):
    try:
        first = open(path, errors="replace").readline().strip()
    except OSError:
        first = ""
    match = re.match(r"#\s*game[:\s]+([a-z0-9_]+)", first, re.I)
    if match:
        return match.group(1).lower()
    low = path.lower()
    for game in ("trapteam", "swapforce", "giants", "ssa"):
        if game in low:
            return game
    return os.path.splitext(os.path.basename(path))[0].lower()


def read(path):
    rows = []
    try:
        lines = open(path, errors="replace").read().splitlines()
    except OSError as error:
        print(f"cannot read {path}: {error}", file=sys.stderr)
        return rows
    for line in lines:
        if not line.strip() or line.startswith("#"):
            continue
        fields = line.split("\t")
        if fields[0] == "kind":
            continue
        while len(fields) < 5:
            fields.append("")
        kind, name, verdict, fps, detail = fields[:5]
        area = fields[5] if len(fields) > 5 else ""
        raw_kind = kind
        kind = kind if kind in ("level", "figure", "credits") else "level"
        verdict = verdict if verdict in VERDICTS else "not_reached"
        try:
            fps = float(fps)
        except ValueError:
            fps = 0.0
        rows.append({"kind": kind, "name": name, "verdict": verdict, "fps": fps, "detail": detail, "area": area,
                     "entry": (raw_kind, name)})
    return rows


def cell(text):
    return str(text).replace("|", "/").replace("\n", " ").strip()


def read_catalogue(path):
    """area, kind, name, route, arg, cap, reason per entry; [] when the file is missing."""
    entries = []
    try:
        lines = open(path, errors="replace").read().splitlines()
    except OSError:
        return entries
    for line in lines:
        if not line.strip() or line.startswith("#"):
            continue
        fields = line.split("\t")
        if fields[0] == "area":
            continue
        while len(fields) < 7:
            fields.append("")
        entries.append(dict(zip(("area", "kind", "name", "route", "arg", "cap", "reason"), fields[:7])))
    return entries


def catalogue_stats(entries, rows):
    """Per area of the catalogue (in its order): the verdict counts of its entries, 'not run' when an entry has no row."""
    found = {row["entry"]: row for row in rows}
    stats, order = {}, []
    for entry in entries:
        area = entry["area"]
        if area not in stats:
            order.append(area)
            stats[area] = {v: 0 for v in FULL}
            stats[area].update({"total": 0, "not_run": 0, "rates": []})
        stat = stats[area]
        stat["total"] += 1
        row = found.get((entry["kind"], entry["name"]))
        if row is None:
            stat["not_run"] += 1
            continue
        stat[row["verdict"]] += 1
        if row["verdict"] == "ok" and row["fps"] > 0:
            stat["rates"].append(row["fps"])
    return [(area, stats[area]) for area in order]


def percent(part, whole):
    return f"{100.0 * part / whole:.1f} %" if whole else "-"


def sanitize(text):
    text = re.sub(r"(?<![\w.])/(?:Users|home|private|var|tmp|Volumes)/\S+", "<path>", text)
    text = re.sub(r"[A-Za-z]:\\\\?Users\S+", "<path>", text)
    return " ".join(text.split())


def why(entry, row):
    """One line: why this entry is not covered."""
    if row is None:
        return "not run"
    if row["verdict"] == "not_testable":
        return entry["reason"] or "not testable automatically"
    detail = sanitize(row["detail"])
    if row["verdict"] == "not_reached" and entry["cap"]:
        return entry["cap"] + ("" if "without a fault" in detail else f" ({detail[:160]})")
    if row["verdict"] in ("crash", "hang"):
        # The cause in a few words, no log text (the page is published): the logs stay with the run.
        cause = ("no new frame for 45 s" if row["verdict"] == "hang" else
                 "a [guard] line" if "[guard]" in detail else
                 "the game ended by itself" if "ended by itself" in detail else "a fault")
        return f"{row['verdict']}: {cause}" + (f" ({entry['cap']})" if entry["cap"] else "")
    first = re.split(r"; last log lines:", detail)[0]
    return f"{row['verdict'].replace('_', ' ')}: {first[:220]}"


def catalogue_page(games, catalogues, platform, build, subset, today):
    lines = [
        "# Status",
        "",
        f"> **Purpose:** every entry of each game's catalogue (levels, hubs, challenges, arenas, modes, credits, figures, "
        f"read from the disc) and how its automatic run went · **Status:** current ({today}) · **Tags:** none (runtime "
        "checks); entry names are [SSA], [Giants], [SwapForce], [TrapTeam], [SuperChargers], [Imaginators]",
        "",
        f"Measured on {cell(platform)}, build {cell(build)}. Covered: the entry was reached the game's way and played "
        "with the hero moving, with no fault, no [guard] line and no hang. The share covered counts every entry of the "
        "catalogue, run or not. A frame rate is the lowest one-second window of a run; other work on the same machine "
        "can lower it. What is not covered, and why: [COVERAGE.md](COVERAGE.md).",
        "",
    ]
    if subset:
        lines += ["**Subset:** this page covers a part of the run.", ""]
    summary = ["| Game | Entries | Covered | Crash | Hang | Not reached | Not testable | Not run |",
               "|---|---:|---:|---:|---:|---:|---:|---:|"]
    body = []
    for key, display, rows in games:
        entries = catalogues.get(key.split("-")[0], [])
        per = catalogue_stats(entries, rows)
        body += [f"## {display}", "",
                 "| Area | Entries | OK | Crash | Hang | Not reached | Not testable | Not run | Lowest fps |",
                 "|---|---:|---:|---:|---:|---:|---:|---:|---:|"]
        total = {v: 0 for v in FULL + ("total", "not_run")}
        rates = []
        for area, stat in per:
            low = min(stat["rates"]) if stat["rates"] else None
            rates += stat["rates"]
            body.append(f'| {cell(area)} | {stat["total"]} | {stat["ok"]} | {stat["crash"]} | {stat["hang"]} | '
                        f'{stat["not_reached"]} | {stat["not_testable"]} | {stat["not_run"]} | {fps_text(low)} |')
            for k in total:
                total[k] += stat[k]
        body.append(f'| All | {total["total"]} | {total["ok"]} ({percent(total["ok"], total["total"])}) | '
                    f'{total["crash"]} | {total["hang"]} | {total["not_reached"]} | {total["not_testable"]} | '
                    f'{total["not_run"]} | {fps_text(min(rates) if rates else None)} |')
        body.append("")
        summary.append(f'| {cell(display)} | {total["total"]} | {total["ok"]} ({percent(total["ok"], total["total"])})'
                       f' | {total["crash"]} | {total["hang"]} | {total["not_reached"]} | {total["not_testable"]} | '
                       f'{total["not_run"]} |')
    return lines + summary + [""] + body


def coverage_page(games, catalogues, today):
    lines = [
        "# Coverage: what is not covered",
        "",
        f"> **Purpose:** every catalogue entry that the automatic runs do not cover, one line each with its reason · "
        f"**Status:** current ({today}) · **Tags:** none (runtime checks); entry names are per game",
        "",
        "The catalogue and the run: [STATUS.md](STATUS.md) (tools/regress/coverage_all.sh). An entry is covered only "
        "when it was reached the game's way and played; everything else is here, never counted as covered.",
        "",
    ]
    for key, display, rows in games:
        entries = catalogues.get(key.split("-")[0], [])
        found = {row["entry"]: row for row in rows}
        missing = [(e, found.get((e["kind"], e["name"]))) for e in entries
                   if found.get((e["kind"], e["name"])) is None or found[(e["kind"], e["name"])]["verdict"] != "ok"]
        lines += [f"## {display} ({len(missing)} of {len(entries)})", ""]
        area = None
        for entry, row in missing:
            if entry["area"] != area:
                area = entry["area"]
                lines += ["", f"**{cell(area)}**", ""] if lines[-1] != "" else [f"**{cell(area)}**", ""]
            lines.append(f"- {cell(entry['name'])}: {cell(why(entry, row))}")
        lines.append("")
    return lines


def area_stats(rows):
    stats = {}
    for kind, label in AREAS:
        found = [row for row in rows if row["kind"] == kind]
        rates = [row["fps"] for row in found if row["fps"] > 0]
        stats[kind] = {
            "label": label,
            "total": len(found),
            "ok": sum(row["verdict"] == "ok" for row in found),
            "not_reached": sum(row["verdict"] == "not_reached" for row in found),
            "crash": sum(row["verdict"] == "crash" for row in found),
            "fps": min(rates) if rates else None,
        }
    return stats


def fps_text(value):
    return "-" if value is None else f"{value:.1f}"


def draw_png(games, path, platform, build, subset):
    if Image is None:
        print("status picture skipped: Pillow is not installed", file=sys.stderr)
        return
    width = 980
    row_height = 66
    height = 150 + sum(58 + row_height * len(AREAS) for _, _, _ in games)
    image = Image.new("RGB", (width, height), "#ffffff")
    draw = ImageDraw.Draw(image)
    try:
        font = ImageFont.load_default(size=16)
        small = ImageFont.load_default(size=13)
        title_font = ImageFont.load_default(size=24)
    except TypeError:
        font = small = title_font = ImageFont.load_default()

    draw.text((24, 20), "Coverage by game", fill="#202020", font=title_font)
    draw.text((24, 54), f"{platform} - build {build}" + (" - subset of the run" if subset else ""), fill="#606060", font=small)
    legend_x = 24
    for verdict, label in (("ok", "passed"), ("not_reached", "not reached"), ("crash", "crashed")):
        draw.rectangle((legend_x, 82, legend_x + 14, 96), fill=COLORS[verdict])
        draw.text((legend_x + 20, 80), label, fill="#202020", font=small)
        legend_x += 130

    y = 125
    for game, display, rows in games:
        stats = area_stats(rows)
        draw.text((24, y), display, fill="#202020", font=font)
        y += 34
        for kind, label in AREAS:
            stat = stats[kind]
            draw.text((40, y + 5), label, fill="#404040", font=small)
            x, bar_width = 150, 560
            draw.rectangle((x, y, x + bar_width, y + 22), fill="#eeeeee")
            if stat["total"]:
                at = x
                for verdict in ("ok", "not_reached", "crash"):
                    part = int(bar_width * stat[verdict] / stat["total"])
                    if part:
                        draw.rectangle((at, y, at + part, y + 22), fill=COLORS[verdict])
                        at += part
                text = f'{stat["ok"]}/{stat["total"]} passed, lowest {fps_text(stat["fps"])} fps'
            else:
                text = "not run"
            draw.text((x + bar_width + 16, y + 4), text, fill="#404040", font=small)
            y += row_height
        y += 24

    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    image.save(path)


def state_lines(games):
    for game, display, rows in games:
        for row in rows:
            if row["verdict"] != "crash" and "[guard]" not in row["detail"]:
                continue
            detail = re.sub(r"(?<![\w.])/(?:Users|home|private|var|tmp)/\S+", "<path>", row["detail"])
            detail = re.sub(r"[A-Za-z]:\\\\?Users\S+", "<path>", detail)
            detail = " ".join(detail.split())[:240]
            print(f"- {display}: {row['kind']} {row['name']} - {row['verdict']}"
                  + (f": {detail}" if detail else ""))


def main(argv):
    files, platform, build, out, png, subset, states = [], None, None, None, None, False, False
    catalogue_dir, coverage_out = None, None
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg in ("--platform", "--build", "--out", "--png", "--catalogue", "--coverage"):
            i += 1
            if i >= len(argv):
                usage()
            value = argv[i]
            if arg == "--platform":
                platform = value
            elif arg == "--build":
                build = value
            elif arg == "--out":
                out = value
            elif arg == "--catalogue":
                catalogue_dir = value
            elif arg == "--coverage":
                coverage_out = value
            else:
                png = value
        elif arg == "--subset":
            subset = True
        elif arg == "--state-lines":
            states = True
        elif arg in ("-h", "--help"):
            usage()
        elif arg.startswith("-"):
            usage()
        else:
            files.append(arg)
        i += 1
    if not files or (out is not None and (platform is None or build is None)):
        usage()
    if states and out is None:
        files = [f for f in files]
    if out is not None and png is None:
        png = os.path.join(os.path.dirname(os.path.abspath(out)), "status.png")

    games, seen = [], set()
    for path in files:
        game = game_of(path)
        key = game if game not in seen else f"{game}-{len(seen)}"
        seen.add(game)
        games.append((key, GAMES.get(game, game.replace("_", " ").title()), read(path)))
    games.sort(key=lambda item: (ORDER.get(item[0], 9), item[1]))

    if states:
        state_lines(games)

    if out is None:
        return 0

    today = datetime.date.today().isoformat()
    if catalogue_dir is not None:
        catalogues = {key: read_catalogue(os.path.join(catalogue_dir, key, "catalogue.tsv")) for key, _, _ in games}
        page = catalogue_page(games, catalogues, platform, build, subset, today)
        os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
        open(out, "w").write("\n".join(page).rstrip() + "\n")
        if coverage_out:
            open(coverage_out, "w").write("\n".join(coverage_page(games, catalogues, today)).rstrip() + "\n")
        draw_png(games, png, platform, build, subset)
        return 0
    lines = [
        "# Status",
        "",
        f"> **Purpose:** the levels, figures and credits each game reached in the automatic coverage runs, and their "
        f"lowest frame rate · **Status:** current ({today}) · **Tags:** none (runtime checks); level and figure names are "
        "[SSA], [Giants], [SwapForce]" + (", [TrapTeam]" if "trapteam" in seen else ""),
        "",
        f"Measured on {cell(platform)}, build {cell(build)}. A frame rate is the lowest one-second window of a run; "
        "other work on the same machine can lower it.",
        "",
    ]
    if subset:
        lines += ["**Subset:** this page covers a part of the levels, figures and credits, not the full run.", ""]

    for _, display, rows in games:
        stats = area_stats(rows)
        lines += [f"## {display}", ""]
        lines += ["| Area | Checks | Passed | Not reached | Crashed | Lowest fps |",
                  "|---|---:|---:|---:|---:|---:|"]
        for kind, _ in AREAS:
            stat = stats[kind]
            lines.append(f'| {cell(stat["label"])} | {stat["total"]} | {stat["ok"]} | {stat["not_reached"]} | '
                         f'{stat["crash"]} | {fps_text(stat["fps"])} |')
        total = sum(stat["total"] for stat in stats.values())
        ok = sum(stat["ok"] for stat in stats.values())
        missed = sum(stat["not_reached"] for stat in stats.values())
        crashed = sum(stat["crash"] for stat in stats.values())
        rates = [stat["fps"] for stat in stats.values() if stat["fps"] is not None]
        lines.append(f"| All | {total} | {ok} | {missed} | {crashed} | {fps_text(min(rates) if rates else None)} |")
        lines.append("")
        failed = [row for row in rows if row["verdict"] != "ok"]
        if failed:
            lines += ["| Area | Name | Result | Lowest fps |", "|---|---|---|---:|"]
            labels = dict(AREAS)
            for row in failed:
                lines.append(f'| {cell(labels.get(row["kind"], row["kind"]))} | {cell(row["name"])} | '
                             f'{cell(row["verdict"])} | {fps_text(row["fps"] if row["fps"] > 0 else None)} |')
            lines.append("")

    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    open(out, "w").write("\n".join(lines).rstrip() + "\n")
    draw_png(games, png, platform, build, subset)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
