#!/usr/bin/env python3
"""Compare two baseline JSON files produced by benchmarks/run.py."""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path


def load(path: str) -> dict:
    return json.loads(Path(path).read_text())


def fmt_ms(value) -> str:
    return f"{value:.1f}" if isinstance(value, (int, float)) else "-"


def fmt_pct(value) -> str:
    if not isinstance(value, (int, float)):
        return "-"
    sign = "+" if value > 0 else ""
    return f"{sign}{value:.1f}%"


def delta_pct(old, new) -> float | None:
    if isinstance(old, (int, float)) and isinstance(new, (int, float)) and old > 0:
        return 100.0 * (new - old) / old
    return None


def ratio(a, b):
    if isinstance(a, (int, float)) and isinstance(b, (int, float)) and b > 0:
        return a / b
    return None


def fmt_ratio(value) -> str:
    return f"{value:.2f}x" if isinstance(value, (int, float)) else "-"


def ref_label(report: dict) -> str | None:
    for comp in report.get("environment", {}).get("compilers", []):
        return comp.get("label")
    return None


def geomean(value: list[float]) -> float | None:
    value = [v for v in value if isinstance(v, (int, float)) and v > 0]
    if not value:
        return None
    return math.exp(sum(math.log(v) for v in value) / len(value))


def render(base: dict, cand: dict) -> str:
    out: list[str] = []
    add = out.append
    label = ref_label(base) or "reference"
    add(f"# Baseline comparison: {base.get('label')} → {cand.get('label')}")
    add("")
    add(f"- baseline: `{base.get('command', '')}`")
    add(f"- candidate: `{cand.get('command', '')}`")
    add(f"- CDD: `{base.get('environment', {}).get('cdd_path')}` "
        f"(commit `{base.get('environment', {}).get('git', {}).get('short_commit')}`) → "
        f"`{cand.get('environment', {}).get('cdd_path')}` "
        f"(commit `{cand.get('environment', {}).get('git', {}).get('short_commit')}`)")
    add("")
    add("Negative deltas are faster/smaller; ratio columns are CDD / reference.")
    add("")

    add("## Runtime")
    add("")
    add(f"| Benchmark | CDD baseline (ms) | CDD candidate (ms) | Δ CDD | "
        f"baseline CDD/{label} | candidate CDD/{label} | Δ ratio | checksum |")
    add("|---|---|---|---|---|---|---|---|")
    ratios_before, ratios_after = [], []
    for name, bmeta in base.get("benchmarks", {}).items():
        cmeta = cand.get("benchmarks", {}).get(name)
        if not cmeta:
            continue
        bimpl = bmeta.get("impls", {})
        cimpl = cmeta.get("impls", {})
        bms = bimpl.get("cdd", {}).get("median_ms")
        cms = cimpl.get("cdd", {}).get("median_ms")
        rb = ratio(bms, bimpl.get(label, {}).get("median_ms"))
        ra = ratio(cms, cimpl.get(label, {}).get("median_ms"))
        if rb:
            ratios_before.append(rb)
        if ra:
            ratios_after.append(ra)
        checksum = cimpl.get("cdd", {}).get("checksum", "-")
        if not cimpl.get("cdd", {}).get("checksum_ok", True):
            checksum = f"{checksum} (MISMATCH)"
        elif bimpl.get("cdd", {}).get("checksum", checksum) != checksum:
            checksum = f"{checksum} (CHANGED)"
        add(f"| {name} | {fmt_ms(bms)} | {fmt_ms(cms)} | {fmt_pct(delta_pct(bms, cms))} | "
            f"{fmt_ratio(rb)} | {fmt_ratio(ra)} | "
            f"{fmt_pct(delta_pct(rb, ra))} | {checksum} |")
    add("")
    add(f"- geometric mean CDD/{label} (all benchmarks): {fmt_ratio(geomean(ratios_before))} → "
        f"{fmt_ratio(geomean(ratios_after))} "
        f"({fmt_pct(delta_pct(geomean(ratios_before), geomean(ratios_after)))})")
    for tier in ("core", "stdlib", "alloc", "startup"):
        before, after = [], []
        for name, bmeta in base.get("benchmarks", {}).items():
            if bmeta.get("tier") != tier:
                continue
            cmeta = cand.get("benchmarks", {}).get(name)
            if not cmeta:
                continue
            rb = ratio(bmeta.get("impls", {}).get("cdd", {}).get("median_ms"),
                       bmeta.get("impls", {}).get(label, {}).get("median_ms"))
            ra = ratio(cmeta.get("impls", {}).get("cdd", {}).get("median_ms"),
                       cmeta.get("impls", {}).get(label, {}).get("median_ms"))
            if rb:
                before.append(rb)
            if ra:
                after.append(ra)
        add(f"- geometric mean `{tier}`: {fmt_ratio(geomean(before))} → "
            f"{fmt_ratio(geomean(after))} "
            f"({fmt_pct(delta_pct(geomean(before), geomean(after)))})")
    add("")

    add("## Compile time and code size (CDD only)")
    add("")
    add("| Source | object ms | Δ | syntax ms | Δ | object .text KiB | Δ |")
    add("|---|---|---|---|---|---|---|")
    for name, bitem in base.get("compile", {}).items():
        citem = cand.get("compile", {}).get(name)
        if not citem:
            continue
        bo = bitem.get("cdd", {}).get("object_ms")
        co = citem.get("cdd", {}).get("object_ms")
        bs = bitem.get("cdd", {}).get("syntax_ms")
        cs = citem.get("cdd", {}).get("syntax_ms")
        bt = bitem.get("cdd", {}).get("object_sections", {}).get("_text")
        ct = citem.get("cdd", {}).get("object_sections", {}).get("_text")
        bt_kib = bt / 1024 if isinstance(bt, int) else None
        ct_kib = ct / 1024 if isinstance(ct, int) else None
        add("| " + " | ".join([
            name,
            fmt_ms(bo),
            fmt_pct(delta_pct(bo, co)),
            fmt_ms(bs),
            fmt_pct(delta_pct(bs, cs)),
            f"{bt_kib:.1f}" if isinstance(bt_kib, float) else "-",
            fmt_pct(delta_pct(bt_kib, ct_kib)),
        ]) + " |")
    add("")

    add("## Executable size (CDD)")
    add("")
    add("| Benchmark | baseline KiB | candidate KiB | Δ |")
    add("|---|---|---|---|")
    for name, bmeta in base.get("benchmarks", {}).items():
        cmeta = cand.get("benchmarks", {}).get(name)
        if not cmeta:
            continue
        bb = bmeta.get("impls", {}).get("cdd", {}).get("exe_bytes")
        cb = cmeta.get("impls", {}).get("cdd", {}).get("exe_bytes")
        if not isinstance(bb, int) or not isinstance(cb, int):
            continue
        add(f"| {name} | {bb / 1024:.1f} | {cb / 1024:.1f} | "
            f"{fmt_pct(100.0 * (cb - bb) / bb if bb else None)} |")
    add("")

    add("## Peak memory (CDD)")
    add("")
    add("| Benchmark | baseline MiB | candidate MiB | Δ |")
    add("|---|---|---|---|")
    for name, bmeta in base.get("benchmarks", {}).items():
        cmeta = cand.get("benchmarks", {}).get(name)
        if not cmeta:
            continue
        bb = bmeta.get("impls", {}).get("cdd", {}).get("max_rss_kb")
        cb = cmeta.get("impls", {}).get("cdd", {}).get("max_rss_kb")
        if not isinstance(bb, int) or not isinstance(cb, int):
            continue
        add(f"| {name} | {bb / 1024:.1f} | {cb / 1024:.1f} | "
            f"{fmt_pct(100.0 * (cb - bb) / bb if bb else None)} |")
    add("")

    bmatrix = base.get("opt_matrix") or {}
    cmatrix = cand.get("opt_matrix") or {}
    if bmatrix.get("benchmarks") and cmatrix.get("benchmarks"):
        add("## Optimization-level matrix (CDD)")
        add("")
        add("| Benchmark | O | baseline ms | candidate ms | Δ | baseline CDD/ref | candidate CDD/ref |")
        add("|---|---|---|---|---|---|---|")
        for name, blevels in bmatrix.get("benchmarks", {}).items():
            clevels = cmatrix.get("benchmarks", {}).get(name, {})
            for level in bmatrix.get("levels", []):
                bcell = blevels.get(str(level), {}).get("impls", {})
                ccell = clevels.get(str(level), {}).get("impls", {})
                bms = bcell.get("cdd", {}).get("median_ms")
                cms = ccell.get("cdd", {}).get("median_ms")
                rb = ratio(bms, bcell.get(label, {}).get("median_ms"))
                ra = ratio(cms, ccell.get(label, {}).get("median_ms"))
                add(f"| {name} | O{level} | {fmt_ms(bms)} | {fmt_ms(cms)} | "
                    f"{fmt_pct(delta_pct(bms, cms))} | {fmt_ratio(rb)} | {fmt_ratio(ra)} |")
        add("")

    regressions = []
    for name, bmeta in base.get("benchmarks", {}).items():
        if bmeta.get("tier") != "core":
            continue
        cmeta = cand.get("benchmarks", {}).get(name)
        if not cmeta:
            continue
        rb = ratio(bmeta.get("impls", {}).get("cdd", {}).get("median_ms"),
                   bmeta.get("impls", {}).get(label, {}).get("median_ms"))
        ra = ratio(cmeta.get("impls", {}).get("cdd", {}).get("median_ms"),
                   cmeta.get("impls", {}).get(label, {}).get("median_ms"))
        d = delta_pct(rb, ra)
        if d is not None:
            regressions.append((name, d))
    return "\n".join(out), regressions


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("baseline")
    ap.add_argument("candidate")
    ap.add_argument("--out", help="write Markdown here instead of stdout")
    ap.add_argument("--fail-regression", type=float, default=None, metavar="PCT",
                    help="exit 2 if a core CDD/reference ratio worsens by more than PCT%%")
    args = ap.parse_args()
    text, regressions = render(load(args.baseline), load(args.candidate))
    if args.out:
        Path(args.out).write_text(text + "\n")
    else:
        print(text)
    if args.fail_regression is not None:
        bad = [(name, d) for name, d in regressions if d > args.fail_regression]
        if bad:
            for name, d in bad:
                print(f"regression: {name} CDD/reference worsened by {d:.1f}%", file=sys.stderr)
            return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
