#!/usr/bin/env python3
"""CDD performance baseline runner.

Two references are measured next to CDD:
  * clang++ (LLVM backend, closest to CDD's own optimizer)
  * g++     (GCC backend, second opinion)

For every workload the same algorithm, data types and workload size are used
on both sides.  The runner records runtime, compile time and object/executable
size, and writes both raw JSON and a Markdown report.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import platform
import re
import shlex
import shutil
import statistics
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

BENCH_DIR = Path(__file__).resolve().parent
ROOT = BENCH_DIR.parent
WORK = BENCH_DIR / ".work"
DEFAULT_CDD = ROOT / "build" / "bin" / "cdd"

# name, tier, description
BENCHMARKS = [
    ("bench_fib", "core", "recursive fib(40)"),
    ("bench_sieve", "core", "prime sieve up to 2e8"),
    ("bench_matmul", "core", "768^3 float64 matmul (i-k-j)"),
    ("bench_nbody", "core", "5-body simulation, 5M steps"),
    ("bench_sort", "core", "quicksort 8M int32 (LCG data)"),
    ("bench_hashmap", "core", "open addressing, 8M inserts + 8M lookups"),
    ("bench_nqueens", "core", "13-queens backtracking"),
    ("bench_binary_search", "core", "2M searches over 16M sorted int32"),
    ("bench_mandelbrot", "core", "1000x1000 float64 escape-time"),
    ("bench_class", "core", "4M IVec3 x 64 by-value method passes"),
    ("bench_str_append", "stdlib", "string append, 8M x 16 bytes"),
    ("bench_vec_push", "stdlib", "Vec<int32> push, 32M elements"),
    ("bench_hashmap_std", "stdlib", "std::HashMap, 4M puts + 4M gets"),
    ("bench_deque", "stdlib", "std::Deque, 32M push_back/pop_front"),
    ("bench_malloc_churn", "alloc", "malloc/free churn, 6M x 64 bytes"),
    ("bench_empty", "startup", "empty program (process startup)"),
]

TIER_ORDER = ["core", "stdlib", "alloc", "startup"]
PERF_EVENTS = ["cycles", "instructions", "cache-misses", "branches", "branch-misses", "task-clock"]

# Representative workloads for the optimization-level matrix.
MATRIX_BENCHMARKS = [
    "bench_fib",
    "bench_matmul",
    "bench_nbody",
    "bench_hashmap",
    "bench_nqueens",
    "bench_str_append",
]
MATRIX_LEVELS = [0, 1, 2, 3]


def log(msg: str) -> None:
    print(msg, flush=True)


def shell_join(cmd: list[str]) -> str:
    return shlex.join(cmd)


def which_tool(*names: str) -> str | None:
    for name in names:
        path = shutil.which(name)
        if path:
            return path
    return None


def size_tool() -> list[str]:
    for candidate in ("llvm-size", "/usr/lib/llvm-21/bin/llvm-size", "size"):
        if candidate.startswith("/"):
            if Path(candidate).exists():
                return [candidate]
        else:
            path = shutil.which(candidate)
            if path:
                return [path]
    raise RuntimeError("no llvm-size/size tool found")


def find_gcc_install_dir(clang: str) -> str | None:
    base = Path("/usr/lib/gcc/x86_64-linux-gnu")
    if not base.is_dir():
        return None
    candidates = [child for child in base.iterdir() if child.is_dir()]
    for candidate in sorted(candidates, key=lambda p: p.name, reverse=True):
        probe = subprocess.run(
            [clang, f"--gcc-install-dir={candidate}", "-x", "c++", "-fsyntax-only", "-"],
            input="#include <cstdio>\n#include <string>\n#include <vector>\nint main() {}\n",
            text=True,
            capture_output=True,
        )
        if probe.returncode == 0:
            return str(candidate)
    return None


def cpp_compiler_specs(args: argparse.Namespace) -> list[dict]:
    if args.cpp_compiler:
        raw = args.cpp_compiler
    else:
        raw = []
        clang = which_tool("clang++-22", "clang++")
        if clang:
            cmd = [clang]
            probe = subprocess.run(
                [clang, "-x", "c++", "-fsyntax-only", "-"],
                input="#include <cstdio>\n",
                text=True,
                capture_output=True,
            )
            if probe.returncode != 0:
                gcc_dir = find_gcc_install_dir(clang)
                if gcc_dir:
                    cmd.append(f"--gcc-install-dir={gcc_dir}")
            raw.append(shell_join(cmd))
        gxx = which_tool("g++", "c++")
        if gxx:
            raw.append(shell_join([gxx]))
    specs = []
    used = set()
    for item in raw:
        cmd = shlex.split(item)
        if not cmd:
            continue
        base = Path(cmd[0]).name
        if base.startswith("clang"):
            short = "clang"
        elif base.startswith("g++") or base.startswith("gcc"):
            short = "gcc"
        else:
            short = re.sub(r"[^A-Za-z0-9_]+", "_", base)
        unique = short
        suffix = 2
        while unique in used:
            unique = f"{short}{suffix}"
            suffix += 1
        used.add(unique)
        specs.append({"label": unique, "cmd": cmd})
    return specs


def cdd_exe_cmd(cdd: str, src: Path, out: Path, opt: int) -> list[str]:
    return [cdd, str(src), "-O", str(opt), "-o", str(out)]


def cdd_obj_cmd(cdd: str, src: Path, out: Path, opt: int) -> list[str]:
    return [cdd, "-c", str(src), "-O", str(opt), "-o", str(out)]


def cdd_syntax_cmd(cdd: str, src: Path) -> list[str]:
    return [cdd, "-fsyntax-only", str(src)]


def cpp_exe_cmd(spec: dict, src: Path, out: Path, opt: int) -> list[str]:
    return spec["cmd"] + [f"-O{opt}", "-g", str(src), "-o", str(out)]


def cpp_obj_cmd(spec: dict, src: Path, out: Path, opt: int) -> list[str]:
    return spec["cmd"] + [f"-O{opt}", "-g", "-c", str(src), "-o", str(out)]


def cpp_syntax_cmd(spec: dict, src: Path) -> list[str]:
    return spec["cmd"] + ["-fsyntax-only", str(src)]


def cdd_ir_cmd(cdd: str, src: Path, opt: int) -> list[str]:
    return [cdd, "-S", "-O", str(opt), str(src)]


def cpp_ir_cmd(spec: dict, src: Path, out: Path, opt: int) -> list[str]:
    return spec["cmd"] + [f"-O{opt}", "-g", "-S", "-emit-llvm", str(src), "-o", str(out)]


def measure_ir(cmd: list[str], path: Path, capture_stdout: bool = False) -> dict | None:
    try:
        if capture_stdout:
            proc = subprocess.run(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                timeout=300,
            )
            if proc.returncode == 0:
                path.write_text(proc.stdout)
        else:
            proc = subprocess.run(
                cmd,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                text=True,
                timeout=300,
            )
    except (OSError, subprocess.TimeoutExpired):
        return None
    if proc.returncode != 0 or not path.exists():
        return None
    text = path.read_text(errors="replace")
    return {"bytes": path.stat().st_size, "lines": text.count("\n")}


def with_taskset(cmd: list[str], cpu: int | None) -> list[str]:
    if cpu is None:
        return cmd
    return ["taskset", "-c", str(cpu)] + cmd


def run_capture(cmd: list[str], timeout: int = 300) -> subprocess.CompletedProcess:
    return subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=timeout,
    )


def run_timed(cmd: list[str], timeout: int = 300) -> float:
    t0 = time.perf_counter_ns()
    proc = subprocess.run(
        cmd,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
        timeout=timeout,
    )
    elapsed_ms = (time.perf_counter_ns() - t0) / 1e6
    if proc.returncode != 0:
        raise RuntimeError(
            f"command failed ({proc.returncode}): {shell_join(cmd)}\n{proc.stderr.strip()}"
        )
    return elapsed_ms


def interleaved_measure(cmds: list[tuple[str, list[str]]], reps: int, warmup: int) -> dict:
    """Run one round of every implementation at a time to reduce drift."""
    samples: dict[str, list[float]] = {label: [] for label, _ in cmds}
    for round_idx in range(warmup + reps):
        for label, cmd in cmds:
            elapsed = run_timed(cmd)
            if round_idx >= warmup:
                samples[label].append(elapsed)
    return samples


def summarize(samples: list[float]) -> dict:
    ordered = sorted(samples)
    n = len(ordered)
    median = statistics.median(samples)
    stdev = statistics.stdev(samples) if n > 1 else 0.0
    def pct(p: float) -> float:
        idx = max(0, min(n - 1, int(round(p * (n - 1)))))
        return ordered[idx]
    return {
        "median_ms": median,
        "min_ms": ordered[0],
        "max_ms": ordered[-1],
        "p10_ms": pct(0.10),
        "p90_ms": pct(0.90),
        "stdev_ms": stdev,
        "cv_pct": 100.0 * stdev / median if median else None,
        "samples_ms": samples,
    }


def max_rss_kb(cmd: list[str], cpu: int | None) -> int | None:
    time_bin = "/usr/bin/time"
    if not Path(time_bin).exists():
        return None
    full = with_taskset([time_bin, "-f", "MAXRSS=%M"] + cmd, cpu)
    try:
        proc = subprocess.run(
            full,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            text=True,
            timeout=300,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    if proc.returncode != 0:
        return None
    for line in proc.stderr.splitlines():
        if line.startswith("MAXRSS="):
            try:
                return int(line.split("=", 1)[1])
            except ValueError:
                return None
    return None


def stripped_bytes(path: Path) -> int | None:
    strip = which_tool("strip")
    if not strip:
        return None
    out_dir = WORK / "stripped"
    out_dir.mkdir(parents=True, exist_ok=True)
    copy = out_dir / path.name
    try:
        shutil.copy2(path, copy)
        proc = subprocess.run(
            [strip, "--strip-all", str(copy)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            text=True,
            timeout=120,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    if proc.returncode != 0:
        return None
    return copy.stat().st_size


def perf_counters(cmd: list[str], cpu: int | None) -> dict | None:
    full = with_taskset(["perf", "stat", "-x,", "-e", ",".join(PERF_EVENTS), "--"] + cmd, cpu)
    try:
        proc = subprocess.run(
            full,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            text=True,
            timeout=300,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    if proc.returncode != 0:
        return None
    counters = {}
    for line in proc.stderr.splitlines():
        parts = line.split(",")
        if len(parts) < 3:
            continue
        event = parts[2]
        value = parts[0]
        if event in PERF_EVENTS:
            try:
                number = float(value)
            except ValueError:
                continue
            counters[event] = number if event == "task-clock" else int(number)
    return counters or None


def section_sizes(path: Path) -> dict:
    proc = run_capture(size_tool() + ["-A", str(path)])
    if proc.returncode != 0:
        return {}
    sizes = {}
    text_total = data_total = bss_total = rodata_total = debug_total = 0
    for line in proc.stdout.splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[0].startswith("."):
            try:
                value = int(parts[1], 0)
            except ValueError:
                continue
            section = parts[0]
            sizes[section] = sizes.get(section, 0) + value
            if section.startswith(".text"):
                text_total += value
            elif section.startswith(".data"):
                data_total += value
            elif section.startswith(".bss"):
                bss_total += value
            elif section.startswith(".rodata"):
                rodata_total += value
            elif section.startswith(".debug"):
                debug_total += value
    sizes["_text"] = text_total
    sizes["_data"] = data_total
    sizes["_bss"] = bss_total
    sizes["_rodata"] = rodata_total
    sizes["_debug"] = debug_total
    sizes["_total_text_data_bss"] = text_total + data_total + bss_total
    sizes["_file_bytes"] = path.stat().st_size if path.exists() else 0
    return sizes


def read_governor(cpu: int | None) -> str | None:
    if cpu is None:
        return None
    path = Path(f"/sys/devices/system/cpu/cpu{cpu}/cpufreq/scaling_governor")
    try:
        return path.read_text().strip()
    except OSError:
        return None


def cpu_busy_snapshot(duration: float = 0.6) -> dict[int, float]:
    def snap() -> dict[int, list[int]]:
        out: dict[int, list[int]] = {}
        try:
            with open("/proc/stat") as f:
                for line in f:
                    if line.startswith("cpu") and line[3].isdigit():
                        parts = line.split()
                        out[int(parts[0][3:])] = [int(x) for x in parts[1:]]
        except OSError:
            pass
        return out

    before = snap()
    time.sleep(duration)
    after = snap()
    busy = {}
    for cpu_id in before:
        if cpu_id not in after:
            continue
        delta = [b - a for a, b in zip(before[cpu_id], after[cpu_id])]
        total = sum(delta)
        idle = delta[3] + (delta[4] if len(delta) > 4 else 0)
        if total > 0:
            busy[cpu_id] = 100.0 * (total - idle) / total
    return busy


def choose_cpu(preference: str) -> tuple[int | None, dict[int, float]]:
    try:
        affinity = sorted(os.sched_getaffinity(0))
    except OSError:
        affinity = list(range(os.cpu_count() or 1))
    if preference in ("none", "-1"):
        return None, {}
    busy = cpu_busy_snapshot()
    if preference != "auto":
        requested = int(preference)
        return (requested if requested in affinity else affinity[0]), busy
    candidates = affinity or [0]
    return min(candidates, key=lambda c: busy.get(c, 100.0)), busy


def first_line(cmd: list[str]) -> str:
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=20)
    except (OSError, subprocess.TimeoutExpired):
        return "unavailable"
    text = (proc.stdout or proc.stderr).strip().splitlines()
    return text[0] if text else "unknown"


def git_info() -> dict:
    def git(*args: str) -> str:
        try:
            proc = subprocess.run(
                ["git", "-C", str(ROOT)] + list(args),
                capture_output=True,
                text=True,
                timeout=10,
            )
        except (OSError, subprocess.TimeoutExpired):
            return ""
        return proc.stdout.strip()

    return {
        "commit": git("rev-parse", "HEAD"),
        "short_commit": git("rev-parse", "--short", "HEAD"),
        "branch": git("rev-parse", "--abbrev-ref", "HEAD"),
        "dirty": bool(git("status", "--porcelain")),
    }


def build_environment(cpu: int | None, cdd: Path, specs: list[dict], cpu_busy: dict[int, float]) -> dict:
    cpuinfo = ""
    try:
        cpuinfo = Path("/proc/cpuinfo").read_text()
    except OSError:
        pass
    model = ""
    for line in cpuinfo.splitlines():
        if line.lower().startswith("model name"):
            model = line.split(":", 1)[1].strip()
            break
    build_type = ""
    llvm_config = os.environ.get("LLVM_CONFIG")
    cache = ROOT / "build" / "CMakeCache.txt"
    if cache.exists():
        for line in cache.read_text().splitlines():
            if line.startswith("CMAKE_BUILD_TYPE:"):
                build_type = line.split("=", 1)[1]
            elif line.startswith("LLVM_DIR:PATH="):
                llvm_dir = line.split("=", 1)[1]
                candidate = Path(llvm_dir).parent.parent.parent / "bin" / "llvm-config"
                if candidate.exists():
                    llvm_config = str(candidate)
    if not llvm_config:
        llvm_config = which_tool("llvm-config-22") or which_tool("llvm-config")
    llvm_version = first_line([llvm_config, "--version"]) if llvm_config else "unknown"
    try:
        affinity = sorted(os.sched_getaffinity(0))
    except OSError:
        affinity = []
    return {
        "date_utc": datetime.now(timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z"),
        "hostname": platform.node(),
        "kernel": platform.release(),
        "machine": platform.machine(),
        "cpu_model": model,
        "cpu_count": os.cpu_count(),
        "cpu_affinity": affinity,
        "cpu_pinned": cpu,
        "cpu_governor": read_governor(cpu),
        "cpu_busy_at_start": {str(k): round(v, 1) for k, v in sorted(cpu_busy.items())},
        "load_average": [round(x, 2) for x in os.getloadavg()] if hasattr(os, "getloadavg") else [],
        "llvm_config_version": llvm_version,
        "cdd_path": str(cdd),
        "cdd_binary_bytes": cdd.stat().st_size if cdd.exists() else 0,
        "cdd_build_type": build_type,
        "cdd_version_banner": first_line([str(cdd), "--help"]),
        "git": git_info(),
        "compilers": [{"label": s["label"], "cmd": shell_join(s["cmd"]),
                       "version": first_line(s["cmd"] + ["--version"])} for s in specs],
    }


def generate_corpus(sizes: list[int]) -> None:
    gen = BENCH_DIR / "corpus" / "gen.py"
    subprocess.run(
        [sys.executable, str(gen), "--out-dir", str(WORK / "corpus"),
         "--sizes", ",".join(str(s) for s in sizes)],
        check=True,
    )


def runtime_impls(cdd: str, specs: list[dict], name: str, opt: int,
                  tag: str = "") -> list[tuple[str, list[str], Path]]:
    suffix = f"_{tag}" if tag else ""
    src_cdd = BENCH_DIR / "cdd" / f"{name}.cdd"
    src_cpp = BENCH_DIR / "cpp" / f"{name}.cpp"
    impls = [("cdd", cdd_exe_cmd(cdd, src_cdd, WORK / "exe" / f"{name}{suffix}_cdd", opt),
              src_cdd)]
    for spec in specs:
        impls.append((spec["label"], cpp_exe_cmd(spec, src_cpp, WORK / "exe" / f"{name}{suffix}_{spec['label']}", opt),
                      src_cpp))
    return impls


def run_opt_matrix(cdd: str, specs: list[dict], names: list[str], levels: list[int],
                   reps: int, warmup: int, cpu: int | None) -> dict:
    matrix = {"levels": levels, "runtime_warmup": warmup, "runtime_reps": reps, "benchmarks": {}}
    expected = 1 + len(specs)
    for name in names:
        log(f"  matrix {name}")
        matrix["benchmarks"][name] = {}
        for level in levels:
            entry = {"impls": {}}
            built: list[tuple[str, list[str]]] = []
            reference_output = None
            for impl, cmd, _ in runtime_impls(cdd, specs, name, level, tag=f"O{level}"):
                try:
                    proc = run_capture(cmd)
                except (OSError, subprocess.TimeoutExpired) as exc:
                    entry["impls"][impl] = {"error": str(exc)}
                    continue
                if proc.returncode != 0:
                    entry["impls"][impl] = {"error": proc.stderr.strip() or "build failed"}
                    continue
                exe = Path(cmd[cmd.index("-o") + 1])
                try:
                    run = subprocess.run(
                        with_taskset([str(exe)], cpu),
                        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                        text=True, timeout=300,
                    )
                except (OSError, subprocess.TimeoutExpired) as exc:
                    entry["impls"][impl] = {"error": str(exc)}
                    continue
                if run.returncode != 0:
                    entry["impls"][impl] = {"error": f"run failed: {run.stderr.strip()}"}
                    continue
                output = run.stdout.strip()
                if reference_output is None:
                    reference_output = output
                entry["impls"][impl] = {
                    "checksum": output,
                    "checksum_ok": output == reference_output,
                    "exe_bytes": exe.stat().st_size,
                }
                built.append((impl, [str(exe)]))
            if len(built) == expected:
                try:
                    samples = interleaved_measure(
                        [(impl, with_taskset(cmd, cpu)) for impl, cmd in built], reps, warmup)
                except RuntimeError as exc:
                    entry["error"] = str(exc)
                    samples = None
                if samples:
                    for impl, _ in built:
                        entry["impls"][impl].update(summarize(samples[impl]))
            entry["checksum_ok"] = (
                len(entry["impls"]) == expected
                and all(v.get("checksum_ok", False) for v in entry["impls"].values())
            )
            matrix["benchmarks"][name][str(level)] = entry
    return matrix


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--quick", action="store_true", help="fewer repetitions, 300-function corpus only")
    ap.add_argument("--full", action="store_true", help="full repetition counts, all corpus sizes, matrix, memory and IR (default)")
    ap.add_argument("--reps", type=int, default=None)
    ap.add_argument("--warmup", type=int, default=None)
    ap.add_argument("--compile-reps", type=int, default=None)
    ap.add_argument("--opt", type=int, default=2)
    ap.add_argument("--cpu", default="auto",
                    help="CPU to pin to: 'auto' picks the least busy core (default), "
                         "'none' disables pinning, or a CPU number")
    ap.add_argument("--cdd", default=str(DEFAULT_CDD))
    ap.add_argument("--cpp-compiler", action="append", default=[], metavar="CMD",
                    help="C++ reference command; repeatable. Defaults to clang++ and g++.")
    ap.add_argument("--only", action="append", default=[], help="run only these benchmark names")
    ap.add_argument("--corpus-sizes", default=None)
    ap.add_argument("--skip-corpus", action="store_true")
    ap.add_argument("--skip-compile", action="store_true")
    ap.add_argument("--skip-size", action="store_true")
    ap.add_argument("--skip-memory", action="store_true")
    ap.add_argument("--skip-ir", action="store_true")
    ap.add_argument("--no-perf", action="store_true")
    ap.add_argument("--matrix", action="store_true",
                    help="also run the -O0/-O1/-O2/-O3 matrix in --quick mode")
    ap.add_argument("--skip-matrix", action="store_true")
    ap.add_argument("--matrix-levels", default="0,1,2,3")
    ap.add_argument("--matrix-reps", type=int, default=3)
    ap.add_argument("--label", default=None)
    ap.add_argument("--out-dir", default=str(BENCH_DIR / "results"))
    ap.add_argument("--baseline-md", action="store_true",
                    help="also write the generated report to benchmarks/BASELINE.md")
    args = ap.parse_args()

    quick = args.quick and not args.full
    reps = args.reps if args.reps is not None else (3 if quick else 9)
    warmup = args.warmup if args.warmup is not None else (1 if quick else 5)
    compile_reps = args.compile_reps if args.compile_reps is not None else (3 if quick else 5)
    skip_memory = args.skip_memory or quick
    skip_ir = args.skip_ir or quick
    matrix_enabled = not args.skip_matrix and (not quick or args.matrix)
    matrix_levels = [int(x) for x in args.matrix_levels.split(",") if x]
    cpu, cpu_busy = choose_cpu(args.cpu)

    cdd = str(Path(args.cdd).resolve())
    if not Path(cdd).exists():
        log(f"error: CDD compiler not found: {cdd}")
        return 1

    corpus_sizes = [int(x) for x in (args.corpus_sizes or ("300" if quick else "100,300,600,1200")).split(",") if x]
    specs = cpp_compiler_specs(args)
    if not specs:
        log("error: no C++ reference compiler found (use --cpp-compiler)")
        return 1

    selected = BENCHMARKS
    if args.only:
        wanted = set(args.only)
        selected = [b for b in BENCHMARKS if b[0] in wanted]
        unknown = wanted - {b[0] for b in BENCHMARKS}
        if unknown:
            log(f"error: unknown benchmarks: {sorted(unknown)}")
            return 1

    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / "exe").mkdir(exist_ok=True)
    (WORK / "obj").mkdir(exist_ok=True)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    label = args.label or f"{datetime.now().strftime('%Y-%m-%d')}-baseline"

    env = build_environment(cpu, Path(cdd), specs, cpu_busy)
    log(f"CDD        : {cdd}")
    log(f"references : {', '.join(s['label'] for s in specs)}")
    log(f"cpu        : {env['cpu_model']} | pinned={cpu} | governor={env['cpu_governor']}")
    log(f"reps       : runtime {warmup}+{reps}, compile {compile_reps}, matrix {args.matrix_reps if matrix_enabled else 0}")
    log(f"extras     : memory={'off' if skip_memory else 'on'} ir={'off' if skip_ir else 'on'} "
        f"matrix={'on' if matrix_enabled else 'off'}")

    report = {
        "schema": 1,
        "label": label,
        "generated_at": datetime.now().isoformat(timespec="seconds"),
        "command": shell_join([sys.executable] + sys.argv),
        "config": {
            "opt": args.opt,
            "runtime_warmup": warmup,
            "runtime_reps": reps,
            "compile_reps": compile_reps,
            "cpu": cpu,
            "skip_size": args.skip_size,
            "skip_memory": skip_memory,
            "skip_ir": skip_ir,
            "matrix": matrix_enabled,
            "matrix_levels": matrix_levels,
            "matrix_reps": args.matrix_reps,
            "corpus_sizes": [] if args.skip_corpus else corpus_sizes,
        },
        "environment": env,
        "benchmarks": {},
        "opt_matrix": {},
    }

    # ---- build + verify + runtime -----------------------------------------
    for name, tier, desc in selected:
        log(f"\n[{tier}] {name}: {desc}")
        entry = {"tier": tier, "description": desc, "impls": {}}
        report["benchmarks"][name] = entry
        built: list[tuple[str, list[str], Path]] = []
        reference_output = None
        for impl, cmd, src in runtime_impls(cdd, specs, name, args.opt):
            log(f"  build {impl}: {shell_join(cmd)}")
            try:
                proc = run_capture(cmd)
            except (OSError, subprocess.TimeoutExpired) as exc:
                entry["impls"][impl] = {"error": str(exc)}
                continue
            if proc.returncode != 0:
                entry["impls"][impl] = {"error": proc.stderr.strip() or "build failed"}
                log(f"  build failed: {proc.stderr.strip()[:400]}")
                continue
            exe = Path(cmd[cmd.index("-o") + 1])
            try:
                run = subprocess.run(
                    with_taskset([str(exe)], cpu),
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    timeout=300,
                )
            except (OSError, subprocess.TimeoutExpired) as exc:
                entry["impls"][impl] = {"error": str(exc)}
                continue
            output = run.stdout.strip()
            if run.returncode != 0:
                entry["impls"][impl] = {"error": f"run failed: {run.stderr.strip()}"}
                continue
            if reference_output is None:
                reference_output = output
            entry["impls"][impl] = {
                "checksum": output,
                "checksum_ok": output == reference_output,
                "exe": str(exe),
                "exe_bytes": exe.stat().st_size,
            }
            built.append((impl, [str(exe)], src))

        if len(built) != 1 + len(specs):
            log("  skipping timing: not every implementation built/ran")
            entry["skipped"] = True
            continue

        commands = [(impl, with_taskset(cmd, cpu)) for impl, cmd, _ in built]
        log(f"  timing: {len(commands)} impls x {warmup + reps} rounds")
        try:
            samples = interleaved_measure(commands, reps, warmup)
        except RuntimeError as exc:
            entry["error"] = str(exc)
            log(f"  timing failed: {exc}")
            continue
        for impl, cmd, _ in built:
            entry["impls"][impl].update(summarize(samples[impl]))
            if not args.no_perf:
                counters = perf_counters(cmd, cpu)
                if counters:
                    entry["impls"][impl]["perf"] = counters
            if not skip_memory:
                rss = max_rss_kb(cmd, cpu)
                if rss is not None:
                    entry["impls"][impl]["max_rss_kb"] = rss
        entry["checksum_ok"] = all(v.get("checksum_ok", False) for v in entry["impls"].values())
        if not args.skip_size:
            for impl, _, _ in built:
                exe = Path(entry["impls"][impl]["exe"])
                entry["impls"][impl]["exe_sections"] = section_sizes(exe)
                stripped = stripped_bytes(exe)
                if stripped is not None:
                    entry["impls"][impl]["stripped_bytes"] = stripped

    # ---- optimization-level matrix ----------------------------------------
    if matrix_enabled:
        matrix_names = [n for n in MATRIX_BENCHMARKS if n in report["benchmarks"]]
        if matrix_names:
            log("\noptimization-level matrix")
            report["opt_matrix"] = run_opt_matrix(
                cdd, specs, matrix_names, matrix_levels, args.matrix_reps, 2, cpu)
        else:
            log("\noptimization-level matrix: no selected matrix benchmarks")

    # ---- compile time + object size ---------------------------------------
    compile_results = {}
    if not args.skip_compile:
        log("\ncompile-time measurements")
        sources = [(name, BENCH_DIR / "cdd" / f"{name}.cdd", BENCH_DIR / "cpp" / f"{name}.cpp")
                   for name, _, _ in selected]
        if not args.skip_corpus:
            generate_corpus(corpus_sizes)
            for size in corpus_sizes:
                sources.append((f"corpus_{size}",
                                WORK / "corpus" / f"corpus_{size}.cdd",
                                WORK / "corpus" / f"corpus_{size}.cpp"))
        for name, src_cdd, src_cpp in sources:
            log(f"  {name}")
            item = {"cdd": {}, "refs": {}}
            variants = []
            obj_path = WORK / "obj" / f"{name}_cdd.o"
            variants.append(("cdd_obj", cdd_obj_cmd(cdd, src_cdd, obj_path, args.opt), "obj"))
            variants.append(("cdd_syntax", cdd_syntax_cmd(cdd, src_cdd), "syntax"))
            for spec in specs:
                o = WORK / "obj" / f"{name}_{spec['label']}.o"
                variants.append((f"{spec['label']}_obj", cpp_obj_cmd(spec, src_cpp, o, args.opt), "obj"))
                variants.append((f"{spec['label']}_syntax", cpp_syntax_cmd(spec, src_cpp), "syntax"))
            commands = [(label, with_taskset(cmd, cpu)) for label, cmd, _ in variants]
            try:
                samples = interleaved_measure(commands, compile_reps, 1)
            except RuntimeError as exc:
                item["error"] = str(exc)
                log(f"    failed: {exc}")
                compile_results[name] = item
                continue
            for variant_label, _, kind in variants:
                stats = summarize(samples[variant_label])
                if variant_label == "cdd_obj":
                    item["cdd"]["object_ms"] = stats["median_ms"]
                    if not args.skip_size:
                        item["cdd"]["object_sections"] = section_sizes(obj_path)
                elif variant_label == "cdd_syntax":
                    item["cdd"]["syntax_ms"] = stats["median_ms"]
                else:
                    impl = variant_label.rsplit("_", 1)[0]
                    key = "object_ms" if kind == "obj" else "syntax_ms"
                    ref = item["refs"].setdefault(impl, {})
                    ref[key] = stats["median_ms"]
                    if kind == "obj" and not args.skip_size:
                        ref["object_sections"] = section_sizes(
                            WORK / "obj" / f"{name}_{impl}.o")
            variant_cmds = {v[0]: v[1] for v in variants}
            if not skip_memory:
                rss = max_rss_kb(variant_cmds["cdd_obj"], cpu)
                if rss is not None:
                    item["cdd"]["max_rss_kb"] = rss
                for spec in specs:
                    rss = max_rss_kb(variant_cmds[f"{spec['label']}_obj"], cpu)
                    if rss is not None:
                        item["refs"].setdefault(spec["label"], {})["max_rss_kb"] = rss
            if not skip_ir:
                ir_dir = WORK / "ir"
                ir_dir.mkdir(parents=True, exist_ok=True)
                cdd_ir_path = ir_dir / f"{name}_cdd.ll"
                ir = measure_ir(
                    with_taskset(cdd_ir_cmd(cdd, src_cdd, args.opt), cpu),
                    cdd_ir_path,
                    capture_stdout=True,
                )
                if ir:
                    item["cdd"]["ir"] = ir
                for spec in specs:
                    if not Path(spec["cmd"][0]).name.startswith("clang"):
                        continue
                    ref_ir_path = ir_dir / f"{name}_{spec['label']}.ll"
                    ir = measure_ir(
                        with_taskset(cpp_ir_cmd(spec, src_cpp, ref_ir_path, args.opt), cpu),
                        ref_ir_path,
                    )
                    if ir:
                        item["refs"].setdefault(spec["label"], {})["ir"] = ir
            compile_results[name] = item
    report["compile"] = compile_results
    report["environment"]["load_average_end"] = (
        [round(x, 2) for x in os.getloadavg()] if hasattr(os, "getloadavg") else [])

    # ---- write -------------------------------------------------------------
    out_dir.mkdir(parents=True, exist_ok=True)
    json_path = out_dir / f"{label}.json"
    json_path.write_text(json.dumps(report, indent=2) + "\n")
    md = render_markdown(report, specs)
    md_path = out_dir / f"{label}.md"
    md_path.write_text(md + "\n")
    if args.baseline_md:
        (BENCH_DIR / "BASELINE.md").write_text(md + "\n")
    log(f"\nwrote {json_path}")
    log(f"wrote {md_path}")
    if args.baseline_md:
        log(f"wrote {BENCH_DIR / 'BASELINE.md'}")

    checksum_failures = [
        name for name, b in report["benchmarks"].items()
        if b.get("checksum_ok") is False
    ]
    if checksum_failures:
        log(f"warning: checksum mismatches: {', '.join(checksum_failures)}")
        return 2
    return 0


# --------------------------- Markdown rendering ---------------------------

def fmt_ms(value: float | None) -> str:
    return f"{value:.1f}" if isinstance(value, (int, float)) else "-"


def fmt_ratio(value: float | None) -> str:
    return f"{value:.2f}x" if isinstance(value, (int, float)) else "-"


def ratio(a: float | None, b: float | None) -> float | None:
    if isinstance(a, (int, float)) and isinstance(b, (int, float)) and b > 0:
        return a / b
    return None


def geomean(values: list[float]) -> float | None:
    values = [v for v in values if v and v > 0]
    if not values:
        return None
    return math.exp(sum(math.log(v) for v in values) / len(values))


def fmt_kib(value: int | None) -> str:
    if not isinstance(value, int):
        return "-"
    return f"{value / 1024:.1f}"


def render_markdown(report: dict, specs: list[dict]) -> str:
    env = report["environment"]
    labels = [s["label"] for s in specs]
    benches = report["benchmarks"]
    lines: list[str] = []
    add = lines.append

    add(f"# CDD Performance Baseline — {report['label']}")
    add("")
    add(f"Generated: `{report['generated_at']}`")
    add(f"Command: `{report['command']}`")
    add("")
    add("## Environment")
    add("")
    add("| Item | Value |")
    add("|---|---|")
    add(f"| CPU | {env['cpu_model']} |")
    add(f"| CPU count / pinned | {env['cpu_count']} / {env['cpu_pinned']} |")
    add(f"| Governor | {env['cpu_governor']} |")
    add(f"| Load average (1/5/15 min) | {' / '.join(str(x) for x in env.get('load_average', [])) or '-'} |")
    add(f"| Load average at end | {' / '.join(str(x) for x in env.get('load_average_end', [])) or '-'} |")
    busy = env.get("cpu_busy_at_start", {}).get(str(env.get("cpu_pinned")))
    add(f"| Selected core busy at start | {busy if busy is not None else '-'}% |")
    add(f"| Kernel | {env['kernel']} ({env['machine']}) |")
    add(f"| CDD | `{env['cdd_path']}` ({env['cdd_binary_bytes']} bytes, build type `{env['cdd_build_type'] or '?'}`) |")
    add(f"| CDD commit | `{env['git']['short_commit']}` {env['git']['branch']}"
        f"{' (dirty worktree)' if env['git']['dirty'] else ''} |")
    add(f"| LLVM (llvm-config) | {env['llvm_config_version']} |")
    for comp in env["compilers"]:
        add(f"| {comp['label']} | {comp['version']} |")
    add("")
    cfg = report.get("config", {})
    add(f"Method: `-O{cfg.get('opt', 2)}` for every compiler, "
        f"{cfg.get('runtime_warmup', 2)} warmup + {cfg.get('runtime_reps', 7)} measured runs "
        f"(median) for runtime, 1 warmup + {cfg.get('compile_reps', 5)} runs for compile time, "
        "one implementation round at a time (interleaved), process pinned with `taskset`. "
        "Each row runs the same algorithm, data types and workload size in CDD and C++. "
        "C++ references use `-g` because CDD emits debug information by default. "
        "The report also records peak RSS, LLVM IR size, object-section breakdown, "
        "stripped executable size, and an optional `-O0..-O3` matrix.")
    add("")
    add("## Headline")
    add("")
    for tier in TIER_ORDER:
        names = [n for n, m in benches.items() if m.get("tier") == tier and not m.get("skipped")]
        cols = []
        for label in labels:
            ratios = []
            for name in names:
                impls = benches[name].get("impls", {})
                r = ratio(impls.get("cdd", {}).get("median_ms"),
                          impls.get(label, {}).get("median_ms"))
                if r:
                    ratios.append(r)
            cols.append(f"**{fmt_ratio(geomean(ratios))}** vs {label}")
        add(f"- `{tier}` geometric mean: " + ", ".join(cols))
    core_pairs = []
    for name, meta in benches.items():
        if meta.get("tier") != "core":
            continue
        impls = meta.get("impls", {})
        r = ratio(impls.get("cdd", {}).get("median_ms"),
                  impls.get(labels[0], {}).get("median_ms")) if labels else None
        if r:
            core_pairs.append((name, r))
    if core_pairs:
        worst = max(core_pairs, key=lambda x: x[1])
        best = min(core_pairs, key=lambda x: x[1])
        add(f"- Largest core gap vs {labels[0]}: `{worst[0]}` at **{fmt_ratio(worst[1])}**; "
            f"largest core win: `{best[0]}` at **{fmt_ratio(best[1])}**.")
    add("")

    add("## Runtime (median, lower is better; ratio = CDD / reference)")
    add("")
    header = "| Benchmark | Tier | CDD (ms) | " + " | ".join(f"{l} (ms)" for l in labels)
    header += " | " + " | ".join(f"CDD/{l}" for l in labels) + " | checksum |"
    add(header)
    add("|---" * (4 + 2 * len(labels)) + "|")
    for tier in TIER_ORDER:
        for name, meta in benches.items():
            if meta.get("tier") != tier:
                continue
            impls = meta.get("impls", {})
            cdd_impl = impls.get("cdd", {})
            row = [name, tier, fmt_ms(cdd_impl.get("median_ms"))]
            for label in labels:
                row.append(fmt_ms(impls.get(label, {}).get("median_ms")))
            for label in labels:
                row.append(fmt_ratio(ratio(cdd_impl.get("median_ms"),
                                           impls.get(label, {}).get("median_ms"))))
            checks = cdd_impl.get("checksum", "-")
            row.append(str(checks))
            add("| " + " | ".join(row) + " |")
    add("")

    add("### Geometric mean by tier")
    add("")
    add("| Tier | " + " | ".join(f"CDD/{l}" for l in labels) + " |")
    add("|---" * (1 + len(labels)) + "|")
    for tier in TIER_ORDER:
        names = [n for n, m in benches.items() if m.get("tier") == tier and not m.get("skipped")]
        cols = []
        for label in labels:
            ratios = []
            for name in names:
                impls = benches[name].get("impls", {})
                r = ratio(impls.get("cdd", {}).get("median_ms"),
                          impls.get(label, {}).get("median_ms"))
                if r:
                    ratios.append(r)
            cols.append(fmt_ratio(geomean(ratios)))
        add(f"| {tier} ({len(names)}) | " + " | ".join(cols) + " |")
    add("")

    add("### Run-to-run stability (CV %, lower is better)")
    add("")
    add("| Benchmark | CDD CV | " + " | ".join(f"{l} CV" for l in labels) + " |")
    add("|---" * (2 + len(labels)) + "|")
    for name, meta in benches.items():
        if meta.get("skipped"):
            continue
        impls = meta.get("impls", {})
        cdd_cv = impls.get("cdd", {}).get("cv_pct")
        if cdd_cv is None:
            continue
        row = [name, f"{cdd_cv:.1f}%"]
        for label in labels:
            cv = impls.get(label, {}).get("cv_pct")
            row.append(f"{cv:.1f}%" if isinstance(cv, (int, float)) else "-")
        add("| " + " | ".join(row) + " |")
    add("")

    matrix = report.get("opt_matrix") or {}
    if matrix.get("benchmarks"):
        add("## Optimization-level matrix (runtime median ms)")
        add("")
        add(f"{matrix.get('runtime_warmup', 1)} warmup + {matrix.get('runtime_reps', 3)} runs per cell. "
            "Rows are the same workload compiled with a different `-O` level.")
        add("")
        add("| Benchmark | O | CDD | " + " | ".join(f"{l}" for l in labels) + " | " +
            " | ".join(f"CDD/{l}" for l in labels) + " |")
        add("|---" * (3 + 2 * len(labels)) + "|")
        for name, levels in matrix.get("benchmarks", {}).items():
            for level in matrix.get("levels", []):
                cell = levels.get(str(level), {})
                impls = cell.get("impls", {})
                cdd_ms = impls.get("cdd", {}).get("median_ms")
                row = [name, f"O{level}", fmt_ms(cdd_ms)]
                for label in labels:
                    row.append(fmt_ms(impls.get(label, {}).get("median_ms")))
                for label in labels:
                    row.append(fmt_ratio(ratio(cdd_ms, impls.get(label, {}).get("median_ms"))))
                add("| " + " | ".join(row) + " |")
        add("")
        failed = []
        for name, levels in matrix.get("benchmarks", {}).items():
            for level in matrix.get("levels", []):
                cell = levels.get(str(level), {})
                for impl, data in cell.get("impls", {}).items():
                    if data.get("error"):
                        failed.append(f"{name} O{level} {impl}")
        if failed:
            add("Cells with build/run errors: " + ", ".join(f"`{x}`" for x in failed) + ".")
            add("")
        add("This matrix is diagnostic: it shows whether a gap is already present at "
            "`-O0` (front-end/codegen shape) or only appears after optimization.")
        add("")

    add("## Compile time (median ms, lower is better)")
    add("")
    add("| Source | CDD `-c` | CDD syntax | " +
        " | ".join(f"{l} `-c`" for l in labels) + " | " +
        " | ".join(f"{l} syntax" for l in labels) + " |")
    add("|---" * (3 + 2 * len(labels)) + "|")
    for name, item in report.get("compile", {}).items():
        if item.get("error"):
            add(f"| {name} | error | - | " + " | ".join("-" for _ in range(2 * len(labels))) + " |")
            continue
        cdd_part = item.get("cdd", {})
        row = [name, fmt_ms(cdd_part.get("object_ms")), fmt_ms(cdd_part.get("syntax_ms"))]
        for label in labels:
            row.append(fmt_ms(item.get("refs", {}).get(label, {}).get("object_ms")))
        for label in labels:
            row.append(fmt_ms(item.get("refs", {}).get(label, {}).get("syntax_ms")))
        add("| " + " | ".join(row) + " |")
    add("")
    add("`syntax` is CDD's `-fsyntax-only` (lexer/parser/imports, no semantic "
        "analysis) versus the reference's `-fsyntax-only` (which does include "
        "semantic analysis), so the syntax columns are diagnostic, not a fair "
        "cross-language ratio.")
    add("")

    corpus_rows = [(name, item) for name, item in report.get("compile", {}).items()
                   if name.startswith("corpus_")]
    if corpus_rows:
        add("### Compile scaling (synthetic corpus)")
        add("")
        add("| Functions | CDD `-c` | " + " | ".join(f"{l} `-c`" for l in labels) +
            " | CDD ms/fn | " + " | ".join(f"{l} ms/fn" for l in labels) + " |")
        add("|---" * (2 + 2 * len(labels)) + "|")
        for name, item in corpus_rows:
            try:
                nfuncs = int(name.split("_", 1)[1])
            except (IndexError, ValueError):
                continue
            cdd_obj = item.get("cdd", {}).get("object_ms")
            row = [str(nfuncs), fmt_ms(cdd_obj)]
            for label in labels:
                row.append(fmt_ms(item.get("refs", {}).get(label, {}).get("object_ms")))
            row.append(f"{cdd_obj / nfuncs:.2f}" if isinstance(cdd_obj, (int, float)) else "-")
            for label in labels:
                ref_obj = item.get("refs", {}).get(label, {}).get("object_ms")
                row.append(f"{ref_obj / nfuncs:.2f}" if isinstance(ref_obj, (int, float)) else "-")
            add("| " + " | ".join(row) + " |")
        add("")
        add("`ms/fn` normalizes the fixed compiler startup cost and shows the "
            "marginal cost of one generated function.")
        add("")

    if not report.get("config", {}).get("skip_size"):
        add("## Code size: object sections (KiB)")
        add("")
        add("| Benchmark | CDD `.text` | " + " | ".join(f"{l} `.text`" for l in labels) +
            " | CDD `.rodata` | CDD `.data` | CDD `.bss` | CDD debug |")
        add("|---" * (6 + len(labels)) + "|")
        for name, meta in benches.items():
            if meta.get("skipped"):
                continue
            compile_item = report.get("compile", {}).get(name, {})
            cdd_sections = compile_item.get("cdd", {}).get("object_sections", {})
            row = [name, fmt_kib(cdd_sections.get("_text"))]
            for label in labels:
                ref_sections = compile_item.get("refs", {}).get(label, {}).get("object_sections", {})
                row.append(fmt_kib(ref_sections.get("_text")))
            row.extend([
                fmt_kib(cdd_sections.get("_rodata")),
                fmt_kib(cdd_sections.get("_data")),
                fmt_kib(cdd_sections.get("_bss")),
                fmt_kib(cdd_sections.get("_debug")),
            ])
            add("| " + " | ".join(row) + " |")
        add("")
        add("Object `.text` is generated code. `.rodata`/`.data`/`.bss` show static "
            "data; `.debug` shows debug-info payload. Debug sections are not part "
            "of the loaded code size.")
        add("")

        add("## Code size: executables (KiB)")
        add("")
        header = "| Benchmark | CDD | CDD stripped | " + " | ".join(
            f"{l} | {l} stripped" for l in labels) + " | CDD/clang |"
        add(header)
        add("|---" * (4 + 2 * len(labels)) + "|")
        for name, meta in benches.items():
            if meta.get("skipped"):
                continue
            impls = meta.get("impls", {})
            row = [name, fmt_kib(impls.get("cdd", {}).get("exe_bytes")),
                   fmt_kib(impls.get("cdd", {}).get("stripped_bytes"))]
            for label in labels:
                row.append(fmt_kib(impls.get(label, {}).get("exe_bytes")))
                row.append(fmt_kib(impls.get(label, {}).get("stripped_bytes")))
            clang_exe = impls.get(labels[0], {}).get("exe_bytes") if labels else None
            row.append(fmt_ratio(ratio(impls.get("cdd", {}).get("exe_bytes"), clang_exe)))
            add("| " + " | ".join(row) + " |")
        add("")
        add("CDD links mimalloc statically, so its executables are much larger "
            "than the C++ references even when generated code is comparable. "
            "`stripped` removes symbols/debug information from a copy.")
        add("")

    ir_rows = []
    for name, item in report.get("compile", {}).items():
        cdd_ir = item.get("cdd", {}).get("ir")
        if not cdd_ir:
            continue
        ref_label = next((l for l in labels if item.get("refs", {}).get(l, {}).get("ir")), None)
        ref_ir = item.get("refs", {}).get(ref_label, {}).get("ir") if ref_label else None
        ir_rows.append((name, cdd_ir, ref_label, ref_ir))
    if ir_rows:
        add("## LLVM IR size (diagnostic)")
        add("")
        add("| Source | CDD bytes | CDD lines | clang bytes | clang lines | CDD/clang bytes |")
        add("|---|---|---|---|---|---|")
        for name, cdd_ir, ref_label, ref_ir in ir_rows:
            cdd_b = cdd_ir.get("bytes", 0)
            clang_b = ref_ir.get("bytes") if ref_ir else None
            add(f"| {name} | {cdd_b} | {cdd_ir.get('lines', 0)} | "
                f"{clang_b if clang_b is not None else '-'} | "
                f"{ref_ir.get('lines') if ref_ir else '-'} | "
                f"{fmt_ratio(ratio(cdd_b, clang_b))} |")
        add("")
        add("Both are LLVM IR, but front ends attach different metadata and "
            "debug information; use this as a diagnostic for CDD's own IR growth, "
            "not as a strict cross-language size ratio.")
        add("")

    rss_rows = []
    for name, meta in benches.items():
        impls = meta.get("impls", {})
        if any(impls.get(l, {}).get("max_rss_kb") for l in ["cdd"] + labels):
            rss_rows.append((name, impls))
    if rss_rows:
        add("## Peak memory: runtime (MiB)")
        add("")
        add("| Benchmark | CDD | " + " | ".join(labels) + " | CDD/clang |")
        add("|---" * (3 + len(labels)) + "|")
        for name, impls in rss_rows:
            cdd_rss = impls.get("cdd", {}).get("max_rss_kb")
            row = [name, f"{cdd_rss / 1024:.1f}" if isinstance(cdd_rss, int) else "-"]
            for label in labels:
                rss = impls.get(label, {}).get("max_rss_kb")
                row.append(f"{rss / 1024:.1f}" if isinstance(rss, int) else "-")
            ref_rss = impls.get(labels[0], {}).get("max_rss_kb") if labels else None
            row.append(fmt_ratio(ratio(cdd_rss, ref_rss)))
            add("| " + " | ".join(row) + " |")
        add("")
        add("Peak RSS is measured once per implementation and includes the static "
            "mimalloc arena for CDD; allocator caching can make it sensitive to "
            "run order.")
        add("")

        compile_rss = [(name, item) for name, item in report.get("compile", {}).items()
                       if item.get("cdd", {}).get("max_rss_kb")]
        if compile_rss:
            add("### Peak memory: compile (MiB)")
            add("")
            add("| Source | CDD | " + " | ".join(labels) + " |")
            add("|---" * (2 + len(labels)) + "|")
            for name, item in compile_rss:
                cdd_rss = item.get("cdd", {}).get("max_rss_kb")
                row = [name, f"{cdd_rss / 1024:.1f}" if isinstance(cdd_rss, int) else "-"]
                for label in labels:
                    rss = item.get("refs", {}).get(label, {}).get("max_rss_kb")
                    row.append(f"{rss / 1024:.1f}" if isinstance(rss, int) else "-")
                add("| " + " | ".join(row) + " |")
            add("")

    perf_rows = []
    for name, meta in benches.items():
        for impl, data in meta.get("impls", {}).items():
            counters = data.get("perf")
            if counters:
                perf_rows.append((name, impl, counters))
    if perf_rows:
        add("## Hardware counters (one representative run)")
        add("")
        add("| Benchmark | Impl | cycles (M) | instructions (M) | IPC | GHz | cache-miss % | branch-miss % |")
        add("|---|---|---|---|---|---|---|---|")
        for name, impl, c in perf_rows:
            cycles = c.get("cycles", 0)
            instr = c.get("instructions", 0)
            misses = c.get("cache-misses", 0)
            branches = c.get("branches", 0)
            bmiss = c.get("branch-misses", 0)
            task_clock = c.get("task-clock", 0)
            ipc = instr / cycles if cycles else 0
            ghz = cycles / (task_clock * 1e6) if task_clock else 0
            miss_pct = 100.0 * misses / instr if instr else 0
            bmiss_pct = 100.0 * bmiss / branches if branches else 0
            add(f"| {name} | {impl} | {cycles / 1e6:.1f} | {instr / 1e6:.1f} | "
                f"{ipc:.2f} | {ghz:.2f} | {miss_pct:.2f} | {bmiss_pct:.2f} |")
        add("")

    add("## Analysis and optimization targets")
    add("")
    core_rows = []
    for name, meta in benches.items():
        if meta.get("tier") != "core":
            continue
        impls = meta.get("impls", {})
        r = ratio(impls.get("cdd", {}).get("median_ms"),
                  impls.get(labels[0], {}).get("median_ms")) if labels else None
        if r:
            core_rows.append((name, r))
    if core_rows:
        ordered = sorted(core_rows, key=lambda x: x[1], reverse=True)
        gaps = ", ".join(f"`{n}` {r:.2f}x" for n, r in ordered[:3])
        wins = ", ".join(f"`{n}` {r:.2f}x" for n, r in ordered[-3:])
        add(f"- Largest core gaps vs {labels[0]}: {gaps}.")
        add(f"- Largest core wins vs {labels[0]}: {wins}.")
    add("- With the auto x86-64-v3 target, matmul, class, and hashmap are now the "
        "largest core wins. The remaining core gaps (fib, sort, binary_search) are "
        "close to clang at the code-generation level; inspect allocator/layout and "
        "GCC's recursive-inlining shape before adding more IR metadata.")
    add("- `bench_fib` is close to clang but well behind gcc. GCC emits a different "
        "recursive shape/inlining decision; treat this as a codegen-shape question, "
        "not a front-end question.")
    add("- `bench_nqueens`, `bench_binary_search`, `bench_mandelbrot`, and "
        "`bench_class` extend the core coverage to backtracking, branchy memory "
        "access, floating-point control flow, and by-value method calls. They make "
        "it harder to improve the core geomean by tuning one hot loop.")
    add("- `stdlib` gaps reflect both generated code and the CDD implementation "
        "(`string`, `Vec`, `HashMap`, `Deque`). Keep them separate from core when "
        "judging the compiler backend.")
    add("- `alloc` is dominated by mimalloc versus the platform allocator; `startup` "
        "is process overhead. Neither should drive backend optimization decisions.")
    add("- Highest-value follow-ups: binary_search memory layout/allocator behavior, "
        "sort branch layout, and the `std::Deque` growth path. fib is dominated by "
        "GCC's recursive inlining, not missing CDD IR metadata. Use `compare.py` to "
        "quantify each change against this baseline.")
    add("")

    add("## Caveats")
    add("")
    add("- The CDD compiler uses LLVM " + env["llvm_config_version"] +
        "; `clang++` may use a different LLVM release. The `g++` column is an "
        "independent GCC reference.")
    add("- The main tables use `-O2`; the optimization-level matrix covers "
        "`-O0..-O3`. No fast-math. The CDD default target is `auto`: "
        "`x86-64-v3` on AVX2 hosts, `generic` otherwise; pass "
        "`-march=generic` for the portable SSE2 baseline.")
    add("- The CPU governor is read live; unless it is `performance`, boost "
        "behaviour adds run-to-run noise. Medians over the measured runs and CPU "
        "pinning keep this small but not zero; the CV table makes the spread explicit.")
    add("- This baseline may be taken on a shared desktop (`load_average` start "
        "and end are recorded above). The runner auto-selects the least busy core "
        "and interleaves implementations to keep the comparison fair, but an idle "
        "machine will produce lower absolute times. Ratios are the intended "
        "comparison metric.")
    add("- Runtime includes process start and exit (typically 1-3 ms); the "
        "`startup` tier measures that overhead directly.")
    add("- The `alloc` tier compares CDD's static mimalloc against the C++ "
        "reference's system allocator; read it as a product-level number, not an "
        "allocator-to-allocator contest. It is excluded from the core geomean.")
    add("- `stdlib` results depend on both code generation and the standard "
        "library implementation. `std::HashMap`/`std::unordered_map` and "
        "`std::Deque`/`std::deque` have different growth and bucket strategies; "
        "these rows are product-level comparisons.")
    add("- Compile time on the small benchmark sources is dominated by process "
        "startup and the automatic std prelude; use the `corpus_*` rows and their "
        "`ms/fn` columns for throughput comparisons.")
    add("- LLVM IR size is a diagnostic for CDD's own optimizer growth. The two "
        "front ends attach different metadata/debug information, so the IR ratio "
        "is not a strict cross-language code-size metric.")
    add("- Peak RSS is sampled once per implementation; allocator caching and the "
        "static mimalloc arena make it sensitive to run order.")
    add("- `startup` hardware counters are dominated by process setup and are only "
        "informative for the wall-clock column.")
    add("")
    return "\n".join(lines)


if __name__ == "__main__":
    raise SystemExit(main())
