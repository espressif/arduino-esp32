#!/usr/bin/env python3
"""Assign affected sketches to Compilation Tests chunks by compile cost.

Longest-processing-time (LPT) packing: sort sketches by warm or cold cost
descending and place each on the current lightest bin. The workflow passes
``--mode warm`` when a Linux ccache key exists to restore, otherwise
``--mode cold``. Measured costs from gh-pages win; otherwise the longest
matching fallback prefix; otherwise the fallback default.

Measured per-sketch values are only used to derive a per-library-family cost
(median over the family's sketches and targets). A single sketch's measured
time is dominated by whether it happened to be the first one built for its
target, because that build pays for the family's static libraries while every
later sketch reuses them; using the raw per-sketch value made the packer trade
one "heavy" sketch for many light ones and produced chunks that looked balanced
(0.4% predicted spread) but finished 2x apart.
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


def family_of(sketch: str) -> str:
    """libraries/<Family>/... -> <Family>; anything else -> ?."""
    parts = sketch.replace("\\", "/").split("/")
    if len(parts) >= 2 and parts[0] == "libraries":
        return parts[1]
    return "?"


def measured_target_count(measured: dict) -> int:
    """Typical number of by_target samples in the cost database.

    Only used for entries that predate by_target data, where the number of
    targets a sketch was built for is unknown. Take it from the data instead of
    a constant: the count differs per branch and per sketch (a sketch builds
    only for the SoCs that support it), and CORE_SOCS in socs_config.sh lists
    the library build targets, which is a different set.
    """
    counts = [
        len(entry.get("by_target") or {})
        for entry in (measured.get("sketches") or {}).values()
        if isinstance(entry, dict)
    ]
    counts = [count for count in counts if count > 0]
    return int(statistics.median(counts)) if counts else 1


def family_medians(measured: dict, mode: str, fallback: dict) -> dict[str, float]:
    """Median per-target cost per library family, for the requested mode."""
    key = _mode_key(mode)
    other = "cold_s" if mode == "warm" else "warm_s"
    default, _prefixes = fallback_tables(fallback)
    cold_default = float((fallback.get("default") or {}).get("cold_s") or default)
    other_scale = (cold_default / default) if default > 0 else 1.0
    samples: dict[str, list[float]] = {}
    for sketch, entry in (measured.get("sketches") or {}).items():
        if not isinstance(entry, dict):
            continue
        for target_entry in (entry.get("by_target") or {}).values():
            if not isinstance(target_entry, dict):
                continue
            value = _positive(target_entry.get(key))
            if value is None:
                value = _positive(target_entry.get(other))
                if value is not None and other_scale > 0:
                    value = value / other_scale if mode == "warm" else value * other_scale
            if value is not None:
                samples.setdefault(family_of(sketch), []).append(value)
    return {family: statistics.median(vals) for family, vals in samples.items() if vals}


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
    family: dict[str, float] | None = None,
    target_count: int | None = None,
) -> tuple[float, str, str]:
    """Return (seconds, source, detail).

    source is family|measured|scaled|last|prefix|default. When ``family`` medians are
    available they win: the packer only controls how many sketches of a family
    land in a chunk, not which one pays the family's one-time build.
    """
    cold_default = float((fallback.get("default") or {}).get("cold_s") or default)
    other_scale = (cold_default / default) if default > 0 else 1.0
    key = _mode_key(mode)
    other = "cold_s" if mode == "warm" else "warm_s"
    entry = (measured.get("sketches") or {}).get(sketch)
    if isinstance(entry, dict):
        med = (family or {}).get(family_of(sketch))
        if med:
            targets = (
                len(entry.get("by_target") or {})
                or target_count
                or measured_target_count(measured)
            )
            return med * targets, "family", family_of(sketch)
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
        # No sample from a pure warm/cold run: fall back to the raw sample of
        # the last run (which may have been mixed) before the in-repo defaults.
        got = _positive(entry.get("last_s"))
        if got is None:
            got = _sum_by_target(entry, "last_s")
        if got is not None:
            return got, "last", "last_s"
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
    family: dict[str, float] | None = None,
    target_count: int | None = None,
) -> float:
    return resolve_cost(
        sketch, mode, measured, default, prefixes, fallback, family, target_count
    )[0]


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
    family: dict[str, float] | None = None,
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
        ratio = measured.get("hit_ratio")
        ratio_text = "" if ratio is None else f"  hit_ratio={ratio}"
        lines.append(
            f"costs: {costs_path}  sha={sha or 'n/a'}  last_updated={last}{ratio_text}  db={db_n}"
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
        + "  ".join(
            f"{k}={counts.get(k, 0)}"
            for k in ("family", "measured", "scaled", "last", "prefix", "default")
        )
    )
    if family:
        heaviest = sorted(family.items(), key=lambda kv: (-kv[1], kv[0]))[:8]
        lines.append("family cost: " + "  ".join(f"{f}={v:.1f}s" for f, v in heaviest))

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
    family = family_medians(measured, args.mode, fallback)
    target_count = measured_target_count(measured)
    costs: dict[str, float] = {}
    sources: dict[str, str] = {}
    details: dict[str, str] = {}
    for s in sketches:
        cost, source, detail = resolve_cost(
            s, args.mode, measured, default, prefixes, fallback, family, target_count
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
            family,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
