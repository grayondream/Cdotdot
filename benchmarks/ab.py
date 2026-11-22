#!/usr/bin/env python3
"""Direct, interleaved A/B runner for two CDD compilers.

The runner builds every selected benchmark with both compiler commands, checks
that both binaries produce identical stdout, then times alternating warmup and
measured rounds on one pinned CPU. It is intentionally separate from
benchmarks/run.py: run.py compares CDD against C++ references, while this tool
isolates a compiler change (or a flag) against the same benchmark set.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import math
import shlex
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from datetime import datetime
from pathlib import Path

BENCH_DIR = Path(__file__).resolve().parent
ROOT = BENCH_DIR.parent
DEFAULT_CDD = ROOT / "build" / "bin" / "cdd"
TIMEOUT_SECONDS = 600


def log(message: str) -> None:
    print(message, flush=True)


def load_bench_table() -> tuple[list[tuple[str, str, str]], list[str]]:
    path = BENCH_DIR / "run.py"
    spec = importlib.util.spec_from_file_location("cdd_bench_run", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load benchmark table from {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return list(module.BENCHMARKS), list(module.TIER_ORDER)


def with_taskset(cmd: list[str], cpu: int | None) -> list[str]:
    if cpu is None:
        return cmd
    return ["taskset", "-c", str(cpu)] + cmd


def run_capture(cmd: list[str], cpu: int | None) -> str:
    proc = subprocess.run(
        with_taskset(cmd, cpu),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=TIMEOUT_SECONDS,
    )
    if proc.returncode != 0:
        raise RuntimeError(
            f"command failed ({proc.returncode}): {shlex.join(cmd)}\n"
            f"{proc.stderr.strip()}"
        )
    return proc.stdout


def run_timed(cmd: list[str], cpu: int | None) -> float:
    start = time.perf_counter_ns()
    proc = subprocess.run(
        with_taskset(cmd, cpu),
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
        timeout=TIMEOUT_SECONDS,
    )
    elapsed_ms = (time.perf_counter_ns() - start) / 1e6
    if proc.returncode != 0:
        raise RuntimeError(
            f"command failed ({proc.returncode}): {shlex.join(cmd)}\n"
            f"{proc.stderr.strip()}"
        )
    return elapsed_ms


def summarize(samples: list[float]) -> dict:
    ordered = sorted(samples)
    median = statistics.median(samples)
    stdev = statistics.stdev(samples) if len(samples) > 1 else 0.0
    return {
        "median_ms": median,
        "min_ms": ordered[0],
        "max_ms": ordered[-1],
        "stdev_ms": stdev,
        "cv_pct": 100.0 * stdev / median if median else None,
        "samples_ms": samples,
    }


def compile_binary(cdd: str, extra_args: str, source: Path, output: Path) -> None:
    cmd = [cdd] + shlex.split(extra_args) + [
        "-O", "2", str(source), "-o", str(output),
    ]
    proc = subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=TIMEOUT_SECONDS,
    )
    if proc.returncode != 0:
        raise RuntimeError(
            f"compile failed ({proc.returncode}): {shlex.join(cmd)}\n"
            f"{proc.stderr.strip()}"
        )
    if not output.is_file():
        raise RuntimeError(f"compile did not produce {output}")


def geomean(ratios: list[float]) -> float | None:
    if not ratios:
        return None
    return math.exp(sum(math.log(ratio) for ratio in ratios) / len(ratios))


def parse_only(values: list[str]) -> set[str]:
    selected: set[str] = set()
    for value in values:
        selected.update(part for part in value.split(",") if part)
    return selected


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--old-cdd", default=str(DEFAULT_CDD))
    parser.add_argument("--new-cdd", default=str(DEFAULT_CDD))
    parser.add_argument("--old-args", default="",
                        help="extra compile flags for the old/baseline binary")
    parser.add_argument("--new-args", default="",
                        help="extra compile flags for the new binary")
    parser.add_argument("--reps", type=int, default=9)
    parser.add_argument("--warmup", type=int, default=5)
    parser.add_argument("--cpu", default="10",
                        help="CPU number for taskset, or 'none' to disable pinning")
    parser.add_argument("--only", action="append", default=[],
                        help="benchmark name (repeatable or comma-separated)")
    parser.add_argument("--out", default=None, help="write the raw report as JSON")
    args = parser.parse_args()

    if args.reps < 1:
        log("error: --reps must be at least 1")
        return 1
    if args.warmup < 0:
        log("error: --warmup must be non-negative")
        return 1

    old_cdd = str(Path(args.old_cdd).resolve())
    new_cdd = str(Path(args.new_cdd).resolve())
    for label, path in (("old", old_cdd), ("new", new_cdd)):
        if not Path(path).is_file():
            log(f"error: {label} CDD compiler not found: {path}")
            return 1

    cpu: int | None
    if args.cpu == "none":
        cpu = None
    else:
        try:
            cpu = int(args.cpu)
        except ValueError:
            log(f"error: invalid --cpu value: {args.cpu}")
            return 1
        if cpu is not None and shutil.which("taskset") is None:
            log("error: taskset not found; use --cpu none")
            return 1

    benchmarks, tier_order = load_bench_table()
    only = parse_only(args.only)
    if only:
        selected = [entry for entry in benchmarks if entry[0] in only]
        unknown = only - {entry[0] for entry in benchmarks}
        if unknown:
            log(f"error: unknown benchmarks: {', '.join(sorted(unknown))}")
            return 1
    else:
        selected = benchmarks
    if not selected:
        log("error: no benchmarks selected")
        return 1

    log(f"old: {old_cdd} {args.old_args}".rstrip())
    log(f"new: {new_cdd} {args.new_args}".rstrip())
    log(f"cpu: {cpu if cpu is not None else 'none'} | warmup={args.warmup} reps={args.reps}")

    report: dict = {
        "schema": 1,
        "generated_at": datetime.now().isoformat(timespec="seconds"),
        "command": shlex.join([sys.executable] + sys.argv),
        "old_cdd": old_cdd,
        "new_cdd": new_cdd,
        "old_args": args.old_args,
        "new_args": args.new_args,
        "cpu": cpu,
        "warmup": args.warmup,
        "reps": args.reps,
        "benchmarks": {},
    }

    errors: list[str] = []
    checksum_failures: list[str] = []
    with tempfile.TemporaryDirectory(prefix="cdd-ab-") as temp_name:
        temp = Path(temp_name)
        for name, tier, description in selected:
            source = BENCH_DIR / "cdd" / f"{name}.cdd"
            if not source.is_file():
                errors.append(f"{name}: missing source {source}")
                continue
            old_bin = temp / f"{name}-old"
            new_bin = temp / f"{name}-new"
            try:
                compile_binary(old_cdd, args.old_args, source, old_bin)
                compile_binary(new_cdd, args.new_args, source, new_bin)
                old_stdout = run_capture([str(old_bin)], cpu)
                new_stdout = run_capture([str(new_bin)], cpu)
            except (OSError, subprocess.TimeoutExpired, RuntimeError) as exc:
                errors.append(f"{name}: {exc}")
                continue

            old_digest = hashlib.sha256(old_stdout.encode()).hexdigest()
            new_digest = hashlib.sha256(new_stdout.encode()).hexdigest()
            checksum_ok = old_digest == new_digest
            if not checksum_ok:
                checksum_failures.append(name)
                log(f"[{tier}] {name}: CHECKSUM MISMATCH")
                continue

            old_samples: list[float] = []
            new_samples: list[float] = []
            try:
                for round_index in range(args.warmup + args.reps):
                    # Alternate the order so either binary can see the same
                    # position after a warmup or a previous measurement.
                    if round_index % 2 == 0:
                        old_ms = run_timed([str(old_bin)], cpu)
                        new_ms = run_timed([str(new_bin)], cpu)
                    else:
                        new_ms = run_timed([str(new_bin)], cpu)
                        old_ms = run_timed([str(old_bin)], cpu)
                    if round_index >= args.warmup:
                        old_samples.append(old_ms)
                        new_samples.append(new_ms)
            except (OSError, subprocess.TimeoutExpired, RuntimeError) as exc:
                errors.append(f"{name}: {exc}")
                continue

            old_summary = summarize(old_samples)
            new_summary = summarize(new_samples)
            ratio = new_summary["median_ms"] / old_summary["median_ms"]
            entry = {
                "tier": tier,
                "description": description,
                "checksum_sha256": old_digest,
                "checksum_ok": True,
                "old": old_summary,
                "new": new_summary,
                "ratio": ratio,
            }
            report["benchmarks"][name] = entry
            log(
                f"[{tier}] {name}: old {old_summary['median_ms']:.2f}ms "
                f"new {new_summary['median_ms']:.2f}ms ratio {ratio:.3f}"
            )

        if errors:
            for error in errors:
                log(f"error: {error}")
            return 1
        if checksum_failures:
            log(f"error: checksum mismatch: {', '.join(checksum_failures)}")
            return 1

        core_ratios = [
            entry["ratio"]
            for name, entry in report["benchmarks"].items()
            if entry["tier"] == "core"
        ]
        all_ratios = [entry["ratio"] for entry in report["benchmarks"].values()]
        report["geomean"] = {
            "core": geomean(core_ratios),
            "all": geomean(all_ratios),
        }
        for tier in tier_order:
            ratios = [
                entry["ratio"]
                for entry in report["benchmarks"].values()
                if entry["tier"] == tier
            ]
            report["geomean"][tier] = geomean(ratios)

    if report["geomean"]["core"] is not None:
        log(f"core geomean: {report['geomean']['core']:.4f}x")
    if report["geomean"]["all"] is not None:
        log(f"all geomean: {report['geomean']['all']:.4f}x")

    if args.out:
        output = Path(args.out)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(report, indent=2) + "\n")
        log(f"report: {output}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
