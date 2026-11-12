#!/usr/bin/env python3
"""The whole coverage run, every game, every entry of its catalogue (tools/regress/coverage_all.sh is the command;
docs/status/COVERAGE.md). See coverage_all.sh for the usage."""
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
REGRESS = os.path.join(ROOT, "tools", "regress")
ORDER = ("giants", "ssa", "swapforce", "trapteam", "superchargers", "imaginators")
ISO_VARS = {g: g.upper() + "_ISO" for g in ORDER}
HEADER = "kind\tname\tverdict\tlowest_fps\tdetail\tarea\tseconds\n"
BATCH = int(os.environ.get("COVERAGE_FIGURE_BATCH", 20))
LEASE = float(os.environ.get("COVERAGE_LEASE", 600))  # seconds of runs per game slot taken
PER_LEASE = 8
# The seconds of play once an entry is reached (--play overrides): 30 with the hero moving, so that the ~500 entry runs
# and ~1100 figures of the six games fit in about 7 hours on two game slots; Swap Force's 84 Swap Zone pieces and 24
# lockpick puzzles, short rooms, 20; an entry that cannot count as covered (its catalogue cap: loaded alone) 15, enough
# for its faults and frame rate.
PLAY_DEFAULT, PLAY_CAPPED = 30, 15
PLAY = {"swap zones": 20, "lockpick puzzles": 20}
lock = threading.Lock()
children = set()
stopping = False


def log(text):
    with lock:
        print(text, flush=True)


def usage():
    lines = open(os.path.join(REGRESS, "coverage_all.sh")).read().splitlines()[1:]
    print("\n".join(line[2:] for line in lines if line.startswith("#")), file=sys.stderr)
    sys.exit(2)


def read_tsv(path):
    rows = []
    if not os.path.exists(path):
        return rows
    for line in open(path, errors="replace"):
        if line.startswith("#") or not line.strip():
            continue
        fields = line.rstrip("\n").split("\t")
        if fields[0] in ("kind", "area"):
            continue
        rows.append(fields)
    return rows


def figure_test(game):
    build = "build" if game == "ssa" else f"build-{game}"
    for base, _, files in os.walk(os.path.join(ROOT, build)):
        if "ArkeyanFigureTest" in files:
            return os.path.join(base, "ArkeyanFigureTest")
    return ""


def lock_command():
    path = os.environ.get("COVERAGE_LOCK", os.path.join(os.path.dirname(ROOT), "gamelock.sh"))
    return [path] if os.access(path, os.X_OK) else []


class Job:
    def __init__(self, game, rundir, route, arg, extra, done):
        self.args = [game.game, game.iso, rundir, route, arg] + extra
        self.rundir, self.done = rundir, done


class Scheduler:
    """The runs waiting for a game slot. Each of `workers` threads takes up to PER_LEASE of them from the front, and one
    tools/regress/coverage_lease.py under the game lock runs them in a row for up to LEASE seconds (a busy lock's
    queue is waited for once per lease); what the lease did not reach goes back to the front."""

    def __init__(self, workers):
        self.workers, self.queue, self.busy = workers, [], 0
        self.cond = threading.Condition()

    def submit(self, job):
        with self.cond:
            self.queue.append(job)
            self.cond.notify_all()

    def run(self):
        threads = [threading.Thread(target=self.worker) for _ in range(self.workers)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()

    def worker(self):
        while True:
            with self.cond:
                while not self.queue and self.busy and not stopping:
                    self.cond.wait(5)
                if stopping or not self.queue:
                    self.cond.notify_all()
                    return
                take, self.queue = self.queue[:PER_LEASE], self.queue[PER_LEASE:]
                self.busy += 1
            left = self.lease(take)
            with self.cond:
                self.queue[:0] = left
                self.busy -= 1
                self.cond.notify_all()

    def lease(self, jobs):
        """Runs `jobs` in one slot; returns those it did not reach."""
        if stopping:
            return []
        for job in jobs:
            shutil.rmtree(job.rundir, ignore_errors=True)
        fd, listing = tempfile.mkstemp(prefix="lease-", suffix=".json")
        with os.fdopen(fd, "w") as f:
            json.dump([{"id": i, "args": job.args} for i, job in enumerate(jobs)], f)
        cmd = lock_command() + [sys.executable, "-I", os.path.join(REGRESS, "coverage_lease.py"), listing, str(LEASE)]
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True,
                                start_new_session=True)
        with lock:
            children.add(proc)
        reached = set()
        for line in proc.stdout:
            try:
                report = json.loads(line)
            except ValueError:
                continue
            if stopping:
                break
            reached.add(report["id"])
            try:
                jobs[report["id"]].done(report["results"], report["seconds"])
            except Exception as error:  # one entry's report must not stop the run
                log(f"  report of {jobs[report['id']].args[3:5]} failed: {error}")
        proc.wait()
        with lock:
            children.discard(proc)
        os.remove(listing)
        return [job for i, job in enumerate(jobs) if i not in reached]


def kill_group(proc):
    try:
        os.killpg(proc.pid, signal.SIGKILL)
    except OSError:
        pass


def scan(rundir, kind, hint, prefix=""):
    log_path = os.path.join(rundir, "game.log")
    frames = os.path.join(rundir, "frames.txt")
    out = subprocess.run([sys.executable, "-I", os.path.join(REGRESS, "coverage_scan.py"), log_path, frames,
                          "credits" if kind == "credits" else "level", hint, prefix],
                         capture_output=True, text=True).stdout.strip().split("\t")
    while len(out) < 3:
        out.append("")
    return out[0] or hint, out[1] or "0.0", out[2]


def keep(rundir, gamedir, name, shot, crashed=False):
    """What stays of a run: the log lines that matter, one frame; the rest is deleted."""
    safe = re.sub(r"[^A-Za-z0-9._-]+", "_", name)
    os.makedirs(os.path.join(gamedir, "logs"), exist_ok=True)
    os.makedirs(os.path.join(gamedir, "shots"), exist_ok=True)
    try:
        lines = open(os.path.join(rundir, "game.log"), errors="replace").read().splitlines()
    except OSError:
        lines = []
    pattern = re.compile(r"\[guard\]|Fault \(|Indirect call|SIGSEGV|SIGBUS|SIGABRT|assert|\berror\b|missing|"
                         r"\[level\]|\[portal\] placed|\[movie\]|SKY_LEVEL", re.I)
    matter = [l for l in lines if pattern.search(l) and "open \\" not in l][:200]
    tail = 300 if crashed else 30  # a crash or hang keeps the files it opened last and the stack
    with open(os.path.join(gamedir, "logs", safe + ".txt"), "w") as f:
        f.write("\n".join(matter + ["--- last lines ---"] + lines[-tail:]) + "\n")
    if shot and os.path.exists(shot):
        png = os.path.join(gamedir, "shots", safe + ".png")
        if shutil.which("sips"):
            subprocess.run(["sips", "-s", "format", "png", "-Z", "480", shot, "--out", png], capture_output=True)
    shutil.rmtree(rundir, ignore_errors=True)


def challenge_levels(iso):
    """Giants: the challenge levels on the disc (level/challenge_level_NNN.arc), in order."""
    sys.path.insert(0, os.path.join(ROOT, "tools"))
    import toy_tables  # noqa: E402  (the disc reader)
    return sorted(m.group(1) for path in toy_tables.Disc(iso).files
                  if (m := re.fullmatch(r"/level/(challenge_level_\d+)\.arc", path)))


class Game:
    def __init__(self, game, iso, work, fsr, resume, play):
        self.game, self.iso, self.fsr, self.play = game, iso, fsr, play
        self.dir = os.path.join(work, game)
        os.makedirs(self.dir, exist_ok=True)
        self.tsv = os.path.join(self.dir, "coverage.tsv")
        self.catalogue_path = os.path.join(self.dir, "catalogue.tsv")
        subprocess.run([sys.executable, "-I", os.path.join(REGRESS, "coverage_catalogue.py"), game, iso,
                        self.catalogue_path], check=True)
        self.catalogue = [dict(zip(("area", "kind", "name", "route", "arg", "cap", "reason"), r))
                          for r in read_tsv(self.catalogue_path)]
        if not resume or not os.path.exists(self.tsv):
            with open(self.tsv, "w") as f:
                f.write(f"# game {game}\n" + HEADER)
        self.done = {(r[0], r[1]) for r in read_tsv(self.tsv) if len(r) > 2}
        self.last_pass = False
        self.counter = 0

    def row(self, entry, verdict, fps, detail, seconds):
        detail = re.sub(r"\s+", " ", detail.replace("\t", " "))[:1500]
        with lock:
            with open(self.tsv, "a") as f:
                f.write("\t".join([entry["kind"], entry["name"], verdict, f"{float(fps or 0):.1f}", detail,
                                   entry["area"], str(int(seconds))]) + "\n")
            self.done.add((entry["kind"], entry["name"]))
            self.counter += 1
        log(f"  {self.game} {entry['kind']} {entry['name']}: {verdict} ({fps} fps, {int(seconds)} s) {detail[:140]}")

    def extra(self, entry):
        args = ["--play", str(self.play or (PLAY_CAPPED if entry["cap"] else PLAY.get(entry["area"], PLAY_DEFAULT)))]
        if self.fsr:
            args.append("--fsr")
        return args

    def seed(self, name, route_arg, after=None):
        """A save made once per work dir: Giants' hub with every challenge unlocked, SuperChargers' Academy."""
        path = os.path.join(self.dir, "seed-" + name)
        if os.path.isdir(path):
            return path
        rundir = os.path.join(self.dir, "runs", "seed-" + name)
        got = []
        scheduler = Scheduler(1)
        scheduler.submit(Job(self, rundir, "seed", route_arg, ["--play", "40", "--save-to", path],
                             lambda results, seconds: got.extend(results)))
        scheduler.run()
        ok = bool(got) and got[0].get("hint") == "ok" and os.path.isdir(path)
        log(f"  {self.game} seed {name}: {'made' if ok else 'FAILED'} {got[0].get('detail', '')[:200] if got else ''}")
        if ok and after:
            ok = subprocess.run(after + [path], capture_output=True).returncode == 0
        shutil.rmtree(rundir, ignore_errors=True)
        if not ok:
            shutil.rmtree(path, ignore_errors=True)
            return ""
        return path

    def opened_challenges(self):
        """Giants: the challenge levels the rows already written opened, by entry name."""
        found = {}
        for r in read_tsv(self.tsv):
            m = re.search(r"(challenge_level_\d+) opened", r[4] if len(r) > 4 else "")
            if r[0] == "challenge" and m and r[2] != "not_reached":
                found[r[1]] = m.group(1)
        return found

    def cali_chain(self, scheduler, entries, seed):
        """Cali's select offers what the save has unlocked, in its own order, and right stops at the last: the entries
        run one after another, and once one opens a challenge an earlier entry opened, the rest are past the list."""
        if not entries:
            return
        entry, rest = entries[0], entries[1:]

        def after(level):
            opened = self.opened_challenges()
            if level and list(opened.values()).count(level) > 1:
                return self.past_the_list(entries, level)
            self.cali_chain(scheduler, rest, seed)

        self.single(scheduler, entry, seed, after=after)

    def past_the_list(self, entries, level):
        offered = sorted(set(self.opened_challenges().values()))
        missing = [n for n in challenge_levels(self.iso) if n not in offered]
        why = (f"Cali offers {len(offered)} with the unlocked save, not challenge_level_"
               f"{', '.join(n[-3:] for n in missing) or 'none'} (no zero byte of the slot set adds them)")
        for entry in entries:
            if (entry["kind"], entry["name"]) in self.done:
                # the entry that opened the same challenge again: not one of its own
                self.rewrite(entry, "not_reached", f"past Cali's list: it opened {level} again; {why}")
            else:
                self.row(entry, "not_reached", 0, f"past Cali's list: {why}", 0)

    def rewrite(self, entry, verdict, detail):
        with lock:
            lines = open(self.tsv).read().splitlines(True)
            with open(self.tsv, "w") as f:
                for line in lines:
                    fields = line.rstrip("\n").split("\t")
                    if fields[:2] == [entry["kind"], entry["name"]]:
                        fields[2], fields[3], fields[4] = verdict, "0.0", detail
                        line = "\t".join(fields) + "\n"
                    f.write(line)
        log(f"  {self.game} {entry['kind']} {entry['name']}: {verdict} {detail[:140]}")

    def single(self, scheduler, entry, seed="", attempt=1, after=None):
        safe = re.sub(r"[^A-Za-z0-9._-]+", "_", entry["name"])
        rundir = os.path.join(self.dir, "runs", f"{entry['kind']}-{safe}")
        extra = self.extra(entry) + (["--seed", seed] if seed else [])

        def done(results, seconds):
            result = results[0] if results else {"hint": "not_reached", "reached": -1,
                                                 "detail": "the run printed no result", "shot": ""}
            if result.get("hint") in ("not_reached", "hang") and result.get("reached", -1) < 0 and attempt == 1:
                log(f"  {self.game} {entry['name']}: not reached, once more ({result.get('detail', '')[:100]})")
                keep(rundir, self.dir, f"{entry['kind']}-{entry['name']}-first", result.get("shot", ""), True)
                self.single(scheduler, entry, seed, 2, after)
                return
            verdict, fps, detail = scan(rundir, entry["kind"], result.get("hint", "not_reached"),
                                        result.get("detail", ""))
            if result.get("hint") == "ok" and verdict == "not_reached" and "no frame rate" in detail:
                # Reached and played, with a stall of a second or more: a freeze a player sees, shown as 0 fps.
                verdict, detail = "ok", detail.replace("no frame rate: under 2 s of frames or a gap of at least 1 s",
                                                       "a stall of 1 s or more while playing (lowest fps 0)")
            if verdict == "ok" and entry["cap"]:
                verdict, detail = "not_reached", f"{entry['cap']}; it ran without a fault ({detail})"
            elif entry["cap"] and verdict != "ok":
                detail = f"{entry['cap']}; {detail}"
            self.row(entry, verdict, fps, detail, seconds)
            keep(rundir, self.dir, f"{entry['kind']}-{entry['name']}", result.get("shot", ""),
                 verdict in ("crash", "hang"))
            if after is not None:
                m = re.search(r"(challenge_level_\d+) opened", detail)
                after(m.group(1) if m else "")

        scheduler.submit(Job(self, rundir, entry["route"], entry["arg"], extra, done))

    def make_figures(self, entries):
        """The dumps of each figure entry, made once with ArkeyanFigureTest: label -> (files, types), or an error."""
        tool = figure_test(self.game)
        folder = os.path.join(self.dir, "dumps")
        os.makedirs(folder, exist_ok=True)
        made = {}
        for entry in entries:
            safe = re.sub(r"[^A-Za-z0-9._-]+", "_", entry["name"])
            files, types, error = [], [], ""
            for part in entry["arg"].split(";"):
                role, _, spec = part.partition("=")
                path = os.path.join(folder, f"{safe}-{role}.sky")
                if role == "trap":
                    fields = spec.split(":")
                    cmd = [tool, "--make-trap", path, f"{fields[0]}:{fields[1]}"] + (
                        [f"villain={fields[2]}"] if len(fields) > 2 else [])
                else:
                    cmd = [tool, "--make", path, spec] + (["xp=1000", "gold=100"] if role in ("toy", "hero") else [])
                if not os.path.exists(path):
                    res = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT)
                    if res.returncode != 0 or not os.path.exists(path):
                        error = f"cannot make the {role} dump: " + (res.stderr or res.stdout).strip()[-200:]
                        break
                files.append(path)
                types.append(spec.split(":")[0])
            made[entry["name"]] = (files, types, error)
        return made

    def figures(self, entries, seed, scheduler):
        made = self.make_figures(entries)
        ready = []
        for entry in entries:
            files, types, error = made[entry["name"]]
            if error:
                self.row(entry, "not_reached", 0, error, 0)
            else:
                ready.append(entry)
        # Crystals last in a batch: a blank one opens the game's creator, which the next figures would wait behind.
        ready.sort(key=lambda e: "creation crystal" in e["area"])
        batches = [ready[i:i + BATCH] for i in range(0, len(ready), BATCH)]
        for n, batch in enumerate(batches):
            self.batch(scheduler, n, batch, made, seed)

    def batch(self, scheduler, n, entries, made, seed):
        rundir = os.path.join(self.dir, "runs", f"figures-{n}")
        listing = os.path.join(self.dir, f"figures-{n}.txt")
        with open(listing, "w") as f:
            for entry in entries:
                files, types, _ = made[entry["name"]]
                f.write(f"{entry['name']}\t{','.join(files)}\t{','.join(types)}\n")
        extra = (["--fsr"] if self.fsr else []) + (["--seed", seed] if seed else [])

        def done(results, seconds):
            _, fps, _ = scan(rundir, "figure", "ok")
            byname = {r["name"]: r for r in results}
            for entry in entries:
                r = byname.get(entry["name"])
                if r is None:
                    continue  # after a crash: run again in a later batch
                if r["hint"] == "not_reached" and not self.last_pass and \
                        ("never read" in r.get("detail", "") or "not placed" in r.get("detail", "")):
                    continue  # the game was not reading the portal then (a scene, a dialog): once more, later
                self.row(entry, r["hint"], fps if r["hint"] == "ok" else 0, r.get("detail", ""), r.get("seconds", 0))
            keep(rundir, self.dir, f"figures-{n}", results[0].get("shot", "") if results else "",
                 any(r["hint"] in ("crash", "hang") for r in results) or len(results) < len(entries))

        scheduler.submit(Job(self, rundir, "figures", listing, extra, done))

    def run(self, scheduler):
        """Queues every entry of the catalogue that has no row yet; the scheduler runs them."""
        todo = [e for e in self.catalogue if (e["kind"], e["name"]) not in self.done]
        for entry in [e for e in todo if e["route"] == "none"]:
            self.row(entry, "not_testable", 0, entry["reason"], 0)
        todo = [e for e in todo if e["route"] != "none"]
        seed = ""
        if self.game == "giants" and any(e["route"] == "cali" for e in todo):
            seed = self.seed("unlocked", "level_hub_2_stage_1",
                             [sys.executable, "-I", os.path.join(REGRESS, "giants_unlock.py")])
        if self.game == "superchargers" and todo:
            seed = self.seed("academy", "academy")
        cali = [e for e in todo if e["route"] == "cali"]
        if cali and not seed:
            for entry in cali:
                self.row(entry, "not_reached", 0, "the unlocked hub save could not be made", 0)
        elif cali:
            opened = self.opened_challenges()
            if len(set(opened.values())) < len(opened):
                self.past_the_list(cali, "a challenge already opened")
            else:
                self.cali_chain(scheduler, cali, seed)
        for entry in todo:
            if entry["route"] in ("figures", "cali"):
                continue
            self.single(scheduler, entry, seed if self.game == "superchargers" else "")
        figures = [e for e in todo if e["route"] == "figures"]
        if figures:
            self.figures(figures, seed if self.game == "superchargers" else "", scheduler)

    def summary(self, seconds):
        rows = read_tsv(self.tsv)
        names = {(e["kind"], e["name"]) for e in self.catalogue}
        verdicts = {}
        for r in rows:
            if (r[0], r[1]) in names:
                verdicts[(r[0], r[1])] = r[2]
        count = {v: list(verdicts.values()).count(v) for v in ("ok", "crash", "hang", "not_reached", "not_testable")}
        total = len(names)
        missing = total - len(verdicts)
        return (f"{self.game}: {total} entries: {count['ok']} ok ({100.0 * count['ok'] / max(total, 1):.1f} %), "
                f"{count['crash']} crash, {count['hang']} hang, {count['not_reached']} not reached, "
                f"{count['not_testable']} not testable" + (f", {missing} not run" if missing else "") +
                f"; {seconds / 60:.0f} min")


def main():
    global stopping
    args = sys.argv[1:]
    if not args or args[0].startswith("-"):
        usage()
    work = os.path.abspath(args.pop(0))
    games, fsr, resume, jobs, play, only = list(ORDER), False, False, 2, 0, ""
    while args:
        flag = args.pop(0)
        if flag == "--games":
            games = [g for g in args.pop(0).split(",") if g]
        elif flag == "--fsr":
            fsr = True
        elif flag == "--resume":
            resume = True
        elif flag == "--jobs":
            jobs = int(args.pop(0))
        elif flag == "--play":
            play = float(args.pop(0))
        elif flag == "--only":
            only = args.pop(0)
        else:
            usage()
    private = os.path.realpath(os.path.join(ROOT, "private"))
    if not os.path.realpath(work).startswith(private + os.sep):
        print("the work dir must be below private/ (logs and frames are disc-derived)", file=sys.stderr)
        sys.exit(2)
    for g in games:
        if g not in ORDER:
            usage()
        if not os.path.isfile(os.environ.get(ISO_VARS[g], "")):
            print(f"set {ISO_VARS[g]} to the {g} disc image (. ../isos.env)", file=sys.stderr)
            sys.exit(2)

    def stop(signum, frame):
        global stopping
        stopping = True
        with lock:
            procs = list(children)
        # A run stops its game on SIGTERM; a run still waiting for its slot (gamelock.sh survives a TERM) is killed.
        for proc in procs:
            try:
                os.killpg(proc.pid, signal.SIGTERM)
            except OSError:
                pass
        threading.Timer(10, lambda: [kill_group(proc) for proc in procs]).start()
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    os.makedirs(work, exist_ok=True)
    started = time.time()
    lines = []
    for g in games:
        t = time.time()
        log(f"== {g}" + (" (upscaler fsr3)" if fsr else ""))
        game = Game(g, os.environ[ISO_VARS[g]], work, fsr, resume, play)
        if only:
            game.catalogue = [e for e in game.catalogue if re.search(only, e["name"]) or re.search(only, e["area"])]
        # A figure batch cut short by a crash leaves its later figures unrun: once more, in new batches.
        for attempt in range(4):
            game.last_pass = attempt == 3
            scheduler = Scheduler(jobs)
            game.run(scheduler)
            scheduler.run()
            if stopping or not any(e["route"] == "figures" and (e["kind"], e["name"]) not in game.done
                                   for e in game.catalogue):
                break
        lines.append(game.summary(time.time() - t))
        log(lines[-1])
        if stopping:
            break
    total = time.time() - started
    with open(os.path.join(work, "summary.txt"), "a") as f:
        f.write(time.strftime("%Y-%m-%d %H:%M") + (" fsr3" if fsr else "") + "\n" + "\n".join(lines) +
                f"\nwall time: {total / 3600:.1f} h\n")
    print("\n".join(lines))
    print(f"wall time: {int(total // 3600)} h {int(total % 3600 // 60):02d} min" + (" (stopped)" if stopping else ""))


if __name__ == "__main__":
    main()
