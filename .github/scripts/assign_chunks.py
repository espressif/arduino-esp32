#!/usr/bin/env python3
"""Assign affected sketches to Compilation Tests chunks by compile cost.

Longest-processing-time (LPT) packing: sort sketches by warm or cold cost
descending and place each on the current lightest bin. The workflow passes
``--mode warm`` when a Linux ccache key exists to restore, otherwise
``--mode cold``. Measured costs from gh-pages win; otherwise the longest
matching fallback prefix; otherwise the fallback default. Does not split by
library family.
"""

from __future__ import annotations

import argparse
import json
import os
import statistics
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_FALLBACK = SCRIPT_DIR / "ci_chunking" / "fallback_costs.json"


def normalize_sketch(path: str) -> str:
    p = path.replace("\\", "/").strip()
    while p.startswith("./"):
        p = p[2:]
    idx = p.find("libraries/")
    if idx != -1:
        return p[idx:]
    return p.lstrip("/")


def load_json(path: Path | None) -> dict:
    if path is None or not path.is_file():
        return {}
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    return data if isinstance(data, dict) else {}


def load_affected(path: Path) -> list[str]:
    sketches: list[str] = []
    seen: set[str] = set()
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        key = normalize_sketch(line)
        if key in seen:
            continue
        seen.add(key)
        sketches.append(key)
    return sketches


def _mode_key(mode: str) -> str:
    return "warm_s" if mode == "warm" else "cold_s"


def fallback_tables(fallback: dict) -> tuple[float, list[tuple[str, float]]]:
    default_block = fallback.get("default") or {}
    prefixes = fallback.get("prefixes") or []
    defaults = [
        float(p.get("warm_s", 0) or 0)
        for p in prefixes
        if isinstance(p, dict) and p.get("warm_s")
    ]
    default = float(default_block.get("warm_s") or 0)
    if default <= 0:
        default = statistics.median(defaults) if defaults else 20.0
    rows: list[tuple[str, float]] = []
    for item in prefixes:
        if not isinstance(item, dict):
            continue
        prefix = str(item.get("prefix") or "")
        if not prefix:
            continue
        rows.append((prefix.replace("\\", "/"), float(item.get("warm_s") or default)))
    rows.sort(key=lambda r: len(r[0]), reverse=True)
    return default, rows


def _positive(val) -> float | None:
    if isinstance(val, (int, float)) and val > 0:
        return float(val)
    return None


def _sum_by_target(entry: dict, key: str) -> float | None:
    by_target = entry.get("by_target")
    if not isinstance(by_target, dict):
        return None
    total = 0.0
    found = False
    for target_entry in by_target.values():
        if not isinstance(target_entry, dict):
            continue
        val = _positive(target_entry.get(key))
        if val is not None:
            total += val
            found = True
    return total if found else None


def resolve_cost(
    sketch: str,
    mode: str,
    measured: dict,
    default: float,
    prefixes: list[tuple[str, float]],
    fallback: dict,
) -> tuple[float, str, str]:
    """Return (seconds, source, detail). source is measured|scaled|prefix|default."""
    cold_default = float((fallback.get("default") or {}).get("cold_s") or default)
    other_scale = (cold_default / default) if default > 0 else 1.0
    key = _mode_key(mode)
    other = "cold_s" if mode == "warm" else "warm_s"
    entry = (measured.get("sketches") or {}).get(sketch)
    if isinstance(entry, dict):
        got = _positive(entry.get(key))
        if got is None:
            got = _sum_by_target(entry, key)
        if got is not None:
            return got, "measured", key
        got = _positive(entry.get(other))
        if got is None:
            got = _sum_by_target(entry, other)
        if got is not None:
            if other_scale > 0:
                got = got / other_scale if mode == "warm" else got * other_scale
            return got, "scaled", f"{other} x{other_scale:.2f}"
    for prefix, warm in prefixes:
        if sketch.startswith(prefix):
            if mode == "warm":
                return warm, "prefix", prefix
            for item in fallback.get("prefixes") or []:
                if isinstance(item, dict) and str(item.get("prefix") or "").replace(
                    "\\", "/"
                ) == prefix:
                    return float(item.get("cold_s") or cold_default), "prefix", prefix
            return cold_default, "prefix", prefix
    if mode == "cold":
        return cold_default, "default", "cold_s"
    return default, "default", "warm_s"


def sketch_cost(
    sketch: str,
    mode: str,
    measured: dict,
    default: float,
    prefixes: list[tuple[str, float]],
    fallback: dict,
) -> float:
    return resolve_cost(sketch, mode, measured, default, prefixes, fallback)[0]


def assign_lpt(sketches: list[str], costs: dict[str, float], max_chunks: int) -> list[list[str]]:
    if max_chunks < 1:
        raise ValueError("max_chunks must be >= 1")
    if not sketches:
        return []
    if len(sketches) <= max_chunks:
        return [[s] for s in sketches]
    loads = [0.0] * max_chunks
    bins: list[list[str]] = [[] for _ in range(max_chunks)]
    ordered = sorted(sketches, key=lambda s: (-costs[s], s))
    for sketch in ordered:
        i = min(range(max_chunks), key=lambda j: (loads[j], j))
        bins[i].append(sketch)
        loads[i] += costs[sketch]
    packed = []
    for bin_sketches in bins:
        if not bin_sketches:
            continue
        packed.append(sorted(bin_sketches))
    return packed


def write_chunk_files(bins: list[list[str]], out_dir: Path) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    for i, sketches in enumerate(bins):
        path = out_dir / f"chunk_{i}.txt"
        path.write_text("".join(f"{s}\n" for s in sketches), encoding="utf-8")


def _library(sketch: str) -> str:
    parts = sketch.replace("\\", "/").split("/")
    if len(parts) >= 2 and parts[0] == "libraries":
        return parts[1]
    return "?"


def _short_sketch(sketch: str) -> str:
    parts = sketch.replace("\\", "/").split("/")
    if len(parts) >= 2:
        return "/".join(parts[-2:])
    return sketch


def format_assignment_stats(
    bins: list[list[str]],
    costs: dict[str, float],
    sources: dict[str, str],
    details: dict[str, str],
    mode: str,
    max_chunks: int,
    measured: dict,
    costs_path: Path | None,
    fallback_path: Path | None,
) -> str:
    lines: list[str] = []
    n = sum(len(b) for b in bins)
    lines.append("=== chunk assignment ===")
    lines.append(
        f"mode={mode}  sketches={n}  chunks={len(bins)}  max_chunks={max_chunks}"
    )
    if costs_path:
        sha = str(measured.get("sha") or "")[:12]
        last = measured.get("last_updated") or measured.get("mode") or "?"
        db_n = len(measured.get("sketches") or {})
        lines.append(
            f"costs: {costs_path}  sha={sha or 'n/a'}  last_updated={last}  db={db_n}"
        )
    else:
        lines.append("costs: (none; in-repo fallback only)")
    if fallback_path:
        lines.append(f"fallback: {fallback_path}")

    counts: dict[str, int] = {}
    for src in sources.values():
        counts[src] = counts.get(src, 0) + 1
    lines.append(
        "cost sources: "
        + "  ".join(f"{k}={counts.get(k, 0)}" for k in ("measured", "scaled", "prefix", "default"))
    )

    if not bins:
        lines.append("no sketches to assign")
        return "\n".join(lines) + "\n"

    loads = [sum(costs.get(s, 0.0) for s in b) for b in bins]
    max_load = max(loads) if loads else 0.0
    mean_load = statistics.mean(loads) if loads else 0.0
    min_load = min(loads) if loads else 0.0
    spread = ((max_load - min_load) / mean_load * 100.0) if mean_load else 0.0

    mix_libs = ("Matter", "Zigbee", "OpenThread", "BLE", "USB", "WebServer")
    lines.append("")
    header = (
        f"{'chunk':>5} {'n':>4} {'load_s':>8} {'load_min':>8} {'pct_max':>7}  "
        f"{' '.join(f'{lib[:6]:>6}' for lib in mix_libs)} {'other':>6}  heaviest"
    )
    lines.append(header)
    for i, (sketches, load) in enumerate(zip(bins, loads)):
        mix = {lib: 0 for lib in mix_libs}
        other = 0
        for s in sketches:
            lib = _library(s)
            if lib in mix:
                mix[lib] += 1
            else:
                other += 1
        heaviest = max(sketches, key=lambda s: (costs.get(s, 0.0), s)) if sketches else ""
        hcost = costs.get(heaviest, 0.0) if heaviest else 0.0
        hsrc = sources.get(heaviest, "?") if heaviest else ""
        pct = (load / max_load * 100.0) if max_load else 0.0
        mix_cols = " ".join(f"{mix[lib]:6d}" for lib in mix_libs)
        lines.append(
            f"{i:5d} {len(sketches):4d} {load:8.1f} {load / 60.0:8.2f} {pct:6.1f}%  "
            f"{mix_cols} {other:6d}  {_short_sketch(heaviest)} ({hcost:.1f}s {hsrc})"
        )
    lines.append(
        f"load min={min_load/60.0:.2f}m  max={max_load/60.0:.2f}m  "
        f"mean={mean_load/60.0:.2f}m  spread={spread:.1f}% of mean"
    )

    ranked = sorted(costs, key=lambda s: (-costs[s], s))
    lines.append("")
    lines.append("heaviest sketches:")
    home = {s: i for i, b in enumerate(bins) for s in b}
    for sketch in ranked[:12]:
        lines.append(
            f"  {costs[sketch]:7.1f}s  chunk {home.get(sketch, -1):2d}  "
            f"{sources.get(sketch, '?'):8} {details.get(sketch, ''):<28}  {sketch}"
        )
    return "\n".join(lines) + "\n"


def write_github_output(bins: list[list[str]]) -> None:
    chunk_count = len(bins)
    chunks = "[" + ",".join(f'"{i}"' for i in range(chunk_count)) + "]" if chunk_count else "[]"
    lines = [f"chunk_count={chunk_count}\n", f"chunks={chunks}\n"]
    out = os.environ.get("GITHUB_OUTPUT")
    if out:
        with open(out, "a", encoding="utf-8") as fh:
            fh.writelines(lines)
    else:
        sys.stderr.write("".join(lines))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True, help="affected sketches list")
    parser.add_argument("--out", type=Path, default=Path("."), help="directory for chunk_N.txt")
    parser.add_argument("--max-chunks", type=int, default=int(os.environ.get("MAX_CHUNKS", "15")))
    parser.add_argument("--mode", choices=("warm", "cold"), default="warm")
    parser.add_argument("--costs", type=Path, default=None, help="gh-pages sketch_costs.json")
    parser.add_argument(
        "--fallback",
        type=Path,
        default=DEFAULT_FALLBACK,
        help="in-repo prefix weights",
    )
    args = parser.parse_args(argv)

    sketches = load_affected(args.input)
    measured = load_json(args.costs)
    fallback = load_json(args.fallback)
    default, prefixes = fallback_tables(fallback)
    costs: dict[str, float] = {}
    sources: dict[str, str] = {}
    details: dict[str, str] = {}
    for s in sketches:
        cost, source, detail = resolve_cost(
            s, args.mode, measured, default, prefixes, fallback
        )
        costs[s] = cost
        sources[s] = source
        details[s] = detail
    bins = assign_lpt(sketches, costs, args.max_chunks)
    write_chunk_files(bins, args.out)
    write_github_output(bins)
    sys.stderr.write(
        format_assignment_stats(
            bins,
            costs,
            sources,
            details,
            args.mode,
            args.max_chunks,
            measured,
            args.costs,
            args.fallback,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
