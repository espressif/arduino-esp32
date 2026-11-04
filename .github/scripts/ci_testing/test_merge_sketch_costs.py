#!/usr/bin/env python3

from __future__ import annotations

import json
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPT_DIR))

from merge_sketch_costs import (  # noqa: E402
    aggregate_times,
    classify_run,
    merge_costs,
    parse_ccache_stats,
)


CCACHE_V4 = """\
Cacheable calls:      1472
  Hits:               1472
    Direct:           1472
    Preprocessed:        0
  Misses:                0
"""

CCACHE_CLASSIC = """\
cache hit (direct)                    10
cache hit (preprocessed)               2
cache miss                             1
"""


def test_parse_ccache_v4_and_classic() -> None:
    assert parse_ccache_stats(CCACHE_V4) == (1472, 0)
    assert parse_ccache_stats(CCACHE_CLASSIC) == (12, 1)
    ratio = "Hits:             1472 / 1472 (100.00 %)\nMisses:              0 / 1472 (  0.00 %)\n"
    assert parse_ccache_stats(ratio) == (1472, 0)


def test_classify_warm_cold_mixed() -> None:
    assert classify_run([CCACHE_V4]) == "warm"
    cold = "Hits: 0\nMisses: 80\n"
    assert classify_run([cold]) == "cold"
    mixed = "cache hit (direct) 50\ncache miss 50\n"
    assert classify_run([mixed]) == "mixed"
    assert classify_run([]) == "mixed"


def test_merge_warm_keeps_cold(tmp_path: Path) -> None:
    prev = {
        "sketches": {
            "libraries/Matter/x/x.ino": {
                "warm_s": 10.0,
                "cold_s": 80.0,
                "by_target": {"esp32s3": {"warm_s": 10.0, "cold_s": 80.0}},
            }
        }
    }
    rows = [
        {"sketch": "libraries/Matter/x/x.ino", "target": "esp32s3", "wall_s": 12.5},
        {"sketch": "libraries/Matter/x/x.ino", "target": "esp32c6", "wall_s": 11.0},
    ]
    aggregated = aggregate_times(rows)
    merged = merge_costs(prev, aggregated, "warm", sha="abc")
    entry = merged["sketches"]["libraries/Matter/x/x.ino"]
    assert entry["warm_s"] == 23.5
    assert entry["cold_s"] == 80.0
    assert entry["by_target"]["esp32s3"]["warm_s"] == 12.5
    assert entry["by_target"]["esp32s3"]["cold_s"] == 80.0
    assert entry["by_target"]["esp32c6"]["warm_s"] == 11.0
    assert merged["last_updated"] == "warm"
    assert "mode" not in merged
    assert merged["sha"] == "abc"


def test_mixed_mode_does_not_clobber() -> None:
    prev = {"sketches": {"libraries/A/a.ino": {"warm_s": 4.0, "cold_s": 12.0}}}
    aggregated = aggregate_times(
        [{"sketch": "libraries/A/a.ino", "target": "esp32", "wall_s": 99.0}]
    )
    merged = merge_costs(prev, aggregated, "mixed")
    assert merged["sketches"]["libraries/A/a.ino"]["warm_s"] == 4.0
    assert merged["sketches"]["libraries/A/a.ino"]["cold_s"] == 12.0


def test_cli_roundtrip(tmp_path: Path) -> None:
    prev = tmp_path / "prev.json"
    prev.write_text(
        json.dumps({"sketches": {"libraries/A/a.ino": {"cold_s": 12.0}}}),
        encoding="utf-8",
    )
    times = tmp_path / "t.jsonl"
    times.write_text(
        json.dumps({"sketch": "libraries/A/a.ino", "target": "esp32", "wall_s": 4.2})
        + "\n",
        encoding="utf-8",
    )
    stats = tmp_path / "stats.txt"
    stats.write_text(CCACHE_V4, encoding="utf-8")
    out = tmp_path / "out.json"
    from merge_sketch_costs import main

    assert (
        main(
            [
                "--prev",
                str(prev),
                "--times",
                str(times),
                "--stats",
                str(stats),
                "--out",
                str(out),
                "--sha",
                "def",
            ]
        )
        == 0
    )
    data = json.loads(out.read_text(encoding="utf-8"))
    assert data["last_updated"] == "warm"
    assert "mode" not in data
    assert data["sketches"]["libraries/A/a.ino"]["warm_s"] == 4.2
    assert data["sketches"]["libraries/A/a.ino"]["cold_s"] == 12.0
