#!/usr/bin/env python3
"""Merge per-sketch compile timings into size_test/<branch>/sketch_costs.json.

The file is a sketch cost database (warm_s / cold_s per sketch). Which side to
read is chosen at assign time from the Linux ccache lookup. A ~100% ccache-hit
run updates warm_s; a ~0% hit run updates cold_s. The other side is kept.
Mixed hit rates leave both sides unchanged. last_updated stamps which side
the last merge wrote; assignment does not read it.
"""

from __future__ import annotations

import argparse
import json
import re
from datetime import datetime, timezone
from pathlib import Path

DIRECT_HIT_RE = re.compile(r"cache hit \(direct\)\s+(\d+)", re.IGNORECASE)
PRE_HIT_RE = re.compile(r"cache hit \(preprocessed\)\s+(\d+)", re.IGNORECASE)


def normalize_sketch(path: str) -> str:
    p = path.replace("\\", "/").strip()
    while p.startswith("./"):
        p = p[2:]
    idx = p.find("libraries/")
    if idx != -1:
        return p[idx:]
    return p.lstrip("/")


def parse_ccache_stats(text: str) -> tuple[int, int]:
    """Return (hits, misses) from `ccache -s` / `ccache -sv` output."""
    direct = DIRECT_HIT_RE.findall(text)
    pre = PRE_HIT_RE.findall(text)
    if direct or pre:
        hits = sum(int(x) for x in direct) + sum(int(x) for x in pre)
        miss_m = re.search(r"cache miss(?:es)?\s+(\d+)", text, re.IGNORECASE)
        misses = int(miss_m.group(1)) if miss_m else 0
        return hits, misses

    # ccache 4: "Hits: N" or "Hits: N / M (100.00 %)"
    hits = 0
    misses = 0
    for m in re.finditer(r"^\s*Hits:\s+(\d+)", text, re.MULTILINE):
        hits = max(hits, int(m.group(1)))
    for m in re.finditer(r"^\s*Misses:\s+(\d+)", text, re.MULTILINE):
        misses = max(misses, int(m.group(1)))
    if hits or misses:
        return hits, misses
    return 0, 0


def run_hit_ratio(stats_texts: list[str]) -> float | None:
    hits = 0
    misses = 0
    for text in stats_texts:
        h, m = parse_ccache_stats(text)
        hits += h
        misses += m
    total = hits + misses
    return (hits / total) if total > 0 else None


def classify_run(stats_texts: list[str], hit_high: float = 0.9, hit_low: float = 0.1) -> str:
    ratio = run_hit_ratio(stats_texts)
    if ratio is None:
        return "mixed"
    if ratio >= hit_high:
        return "warm"
    if ratio <= hit_low:
        return "cold"
    return "mixed"


def load_jsonl(paths: list[Path]) -> list[dict]:
    rows: list[dict] = []
    for path in paths:
        if not path.is_file():
            continue
        for line in path.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if not line:
                continue
            try:
                row = json.loads(line)
            except json.JSONDecodeError:
                continue
            if isinstance(row, dict):
                rows.append(row)
    return rows


def aggregate_times(rows: list[dict]) -> dict[str, dict]:
    """sketch -> {total, by_target: {target: wall_s}}."""
    out: dict[str, dict] = {}
    for row in rows:
        sketch = normalize_sketch(str(row.get("sketch") or ""))
        target = str(row.get("target") or "")
        try:
            wall = float(row.get("wall_s"))
        except (TypeError, ValueError):
            continue
        if not sketch or wall < 0:
            continue
        entry = out.setdefault(sketch, {"total": 0.0, "by_target": {}})
        entry["total"] += wall
        if target:
            by_t = entry["by_target"]
            by_t[target] = by_t.get(target, 0.0) + wall
    return out


def merge_costs(
    previous: dict,
    aggregated: dict[str, dict],
    mode: str,
    sha: str = "",
    hit_ratio: float | None = None,
) -> dict:
    """Merge one run's samples into the cost database.

    ``warm_s`` and ``cold_s`` only ever receive samples from a run that is
    (almost) entirely warm or cold: a mixed sample is not representative of
    either regime, and writing it to one side would bias that side. Every run
    still stores its raw sample as ``last_s`` (plus the run's hit ratio), so a
    sketch that only ever appeared in mixed runs is measured instead of falling
    back to the in-repo defaults.
    """
    sketches = dict(previous.get("sketches") or {})
    pure_key = {"warm": "warm_s", "cold": "cold_s"}.get(mode)
    for sketch, sample in aggregated.items():
        entry = dict(sketches.get(sketch) or {})
        entry["last_s"] = round(sample["total"], 3)
        by_target = dict(entry.get("by_target") or {})
        for target, wall in sample["by_target"].items():
            t_entry = dict(by_target.get(target) or {})
            t_entry["last_s"] = round(wall, 3)
            if pure_key:
                t_entry[pure_key] = round(wall, 3)
            by_target[target] = t_entry
        if by_target:
            entry["by_target"] = by_target
        if pure_key:
            entry[pure_key] = round(sample["total"], 3)
        sketches[sketch] = entry
    merged = {
        "version": 1,
        "updated_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "sha": sha or previous.get("sha") or "",
        "last_updated": mode,
        "sketches": sketches,
    }
    if hit_ratio is not None:
        merged["hit_ratio"] = round(hit_ratio, 3)
    elif previous.get("hit_ratio") is not None:
        merged["hit_ratio"] = previous["hit_ratio"]
    return merged


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prev", type=Path, default=None, help="previous sketch_costs.json")
    parser.add_argument("--times", type=Path, nargs="*", default=[], help="sketch_times JSONL files")
    parser.add_argument("--stats", type=Path, nargs="*", default=[], help="ccache -s dumps")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--sha", default="")
    args = parser.parse_args(argv)

    previous: dict = {}
    if args.prev and args.prev.is_file():
        try:
            loaded = json.loads(args.prev.read_text(encoding="utf-8"))
            if isinstance(loaded, dict):
                previous = loaded
        except (OSError, json.JSONDecodeError):
            previous = {}

    stats_texts = []
    for path in args.stats:
        if path.is_file():
            stats_texts.append(path.read_text(encoding="utf-8", errors="replace"))
    mode = classify_run(stats_texts)
    ratio = run_hit_ratio(stats_texts)
    aggregated = aggregate_times(load_jsonl(list(args.times)))
    merged = merge_costs(previous, aggregated, mode, sha=args.sha, hit_ratio=ratio)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(merged, indent=2, sort_keys=False) + "\n", encoding="utf-8")
    ratio_text = "n/a" if ratio is None else f"{ratio:.1%}"
    print(
        f"Wrote {args.out} last_updated={mode} hit_ratio={ratio_text} "
        f"sketches={len(merged.get('sketches') or {})}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
