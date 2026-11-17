#!/usr/bin/env python3
"""Generate the synthetic compile-time corpus.

The generated files are deliberately boring: many small functions with a loop
and a call from main.  They isolate front-end/optimizer throughput from any
interesting program behaviour.
"""

from __future__ import annotations

import argparse
from pathlib import Path


def cdd_source(nfuncs: int) -> str:
    out = [f"module corpus_{nfuncs};", ""]
    for i in range(nfuncs):
        out.append(f"int32 fn{i}(int32 x) {{")
        out.append("    int32 s = x;")
        out.append("    for (int32 i = 0; i < 12; i = i + 1) {")
        out.append("        s = s * 3 + i;")
        out.append("    }")
        out.append("    return s;")
        out.append("}")
        out.append("")
    out.append("int32 main() {")
    out.append("    int32 s = 0;")
    for i in range(nfuncs):
        out.append(f"    s = s + fn{i}(s);")
    out.append("    return s & 0;")
    out.append("}")
    out.append("")
    return "\n".join(out)


def cpp_source(nfuncs: int) -> str:
    out = []
    for i in range(nfuncs):
        out.append(f"int fn{i}(int x) {{")
        out.append("    int s = x;")
        out.append("    for (int j = 0; j < 12; ++j) {")
        out.append("        s = s * 3 + j;")
        out.append("    }")
        out.append("    return s;")
        out.append("}")
        out.append("")
    out.append("int main() {")
    out.append("    int s = 0;")
    for i in range(nfuncs):
        out.append(f"    s = s + fn{i}(s);")
    out.append("    return s & 0;")
    out.append("}")
    out.append("")
    return "\n".join(out)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--sizes", default="300,1200")
    args = ap.parse_args()
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    for size in (int(x) for x in args.sizes.split(",") if x):
        (out_dir / f"corpus_{size}.cdd").write_text(cdd_source(size))
        (out_dir / f"corpus_{size}.cpp").write_text(cpp_source(size))
        print(f"wrote corpus_{size}.cdd / corpus_{size}.cpp ({size} functions)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
