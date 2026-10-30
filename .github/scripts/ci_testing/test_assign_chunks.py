#!/usr/bin/env python3

from __future__ import annotations

import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPT_DIR))

from assign_chunks import (  # noqa: E402
    assign_lpt,
    fallback_tables,
    format_assignment_stats,
    load_affected,
    load_json,
    normalize_sketch,
    sketch_cost,
    write_chunk_files,
    write_github_output,
)


FALLBACK = load_json(SCRIPT_DIR / "ci_chunking" / "fallback_costs.json")
DEFAULT, PREFIXES = fallback_tables(FALLBACK)


def test_normalize_strips_to_libraries() -> None:
    assert (
        normalize_sketch("/home/runner/work/arduino-esp32/libraries/Matter/x/x.ino")
        == "libraries/Matter/x/x.ino"
    )
    assert normalize_sketch("./libraries/Hash/Hash.ino") == "libraries/Hash/Hash.ino"


def test_one_sketch_per_chunk_when_n_at_most_max() -> None:
    sketches = ["libraries/A/a.ino", "libraries/B/b.ino"]
    costs = {s: 1.0 for s in sketches}
    bins = assign_lpt(sketches, costs, max_chunks=15)
    assert bins == [["libraries/A/a.ino"], ["libraries/B/b.ino"]]


def test_lpt_spreads_heavy_sketches() -> None:
    sketches = [f"s{i}.ino" for i in range(6)]
    costs = {
        "s0.ino": 100.0,
        "s1.ino": 90.0,
        "s2.ino": 5.0,
        "s3.ino": 5.0,
        "s4.ino": 5.0,
        "s5.ino": 5.0,
    }
    bins = assign_lpt(sketches, costs, max_chunks=2)
    assert len(bins) == 2
    loads = [sum(costs[s] for s in b) for b in bins]
    assert abs(loads[0] - loads[1]) <= 10.0
    homes = {s: i for i, b in enumerate(bins) for s in b}
    assert homes["s0.ino"] != homes["s1.ino"]


def test_prefix_fallback_beats_default() -> None:
    measured = {"sketches": {}}
    matter = sketch_cost(
        "libraries/Matter/examples/Control/MatterOnOffPlugin/MatterOnOffPlugin.ino",
        "warm",
        measured,
        DEFAULT,
        PREFIXES,
        FALLBACK,
    )
    gpio = sketch_cost(
        "libraries/ESP32/examples/GPIO/GPIO.ino",
        "warm",
        measured,
        DEFAULT,
        PREFIXES,
        FALLBACK,
    )
    assert matter == 73.0
    assert gpio == 17.0


def test_measured_cost_wins_over_prefix() -> None:
    sketch = "libraries/Matter/examples/Control/MatterOnOffPlugin/MatterOnOffPlugin.ino"
    measured = {"sketches": {sketch: {"warm_s": 48.2, "cold_s": 120.0}}}
    assert (
        sketch_cost(sketch, "warm", measured, DEFAULT, PREFIXES, FALLBACK) == 48.2
    )
    assert (
        sketch_cost(sketch, "cold", measured, DEFAULT, PREFIXES, FALLBACK) == 120.0
    )


def test_missing_costs_json_uses_fallback(tmp_path: Path) -> None:
    missing = tmp_path / "nope.json"
    measured = load_json(missing)
    assert measured == {}
    cost = sketch_cost(
        "libraries/Matter/x/x.ino", "cold", measured, DEFAULT, PREFIXES, FALLBACK
    )
    assert cost == 124.0


def test_measured_other_side_is_scaled() -> None:
    sketch = "libraries/ESP32/examples/GPIO/GPIO.ino"
    measured = {"sketches": {sketch: {"cold_s": 33.0}}}
    assert sketch_cost(sketch, "warm", measured, DEFAULT, PREFIXES, FALLBACK) == 17.0
    measured = {"sketches": {sketch: {"warm_s": 17.0}}}
    assert sketch_cost(sketch, "cold", measured, DEFAULT, PREFIXES, FALLBACK) == 33.0


def test_longest_prefix_wins_for_ethernet_matter() -> None:
    measured = {"sketches": {}}
    eth = sketch_cost(
        "libraries/Matter/examples/Commissioning/MatterOnNetworkEthernet/MatterOnNetworkEthernet.ino",
        "cold",
        measured,
        DEFAULT,
        PREFIXES,
        FALLBACK,
    )
    other = sketch_cost(
        "libraries/Matter/examples/Sensors/MatterContactSensor/MatterContactSensor.ino",
        "cold",
        measured,
        DEFAULT,
        PREFIXES,
        FALLBACK,
    )
    assert eth == 648.0
    assert other == 124.0


def test_empty_affected_list(tmp_path: Path) -> None:
    src = tmp_path / "affected_sketches.txt"
    src.write_text("\n# comment\n", encoding="utf-8")
    assert load_affected(src) == []
    bins = assign_lpt([], {}, max_chunks=15)
    assert bins == []
    write_chunk_files(bins, tmp_path)
    assert list(tmp_path.glob("chunk_*.txt")) == []


def test_assignment_stats_show_loads_and_sources() -> None:
    sketches = ["libraries/Matter/x/x.ino", "libraries/ESP32/GPIO/GPIO.ino"]
    costs = {sketches[0]: 73.0, sketches[1]: 17.0}
    sources = {sketches[0]: "prefix", sketches[1]: "default"}
    details = {sketches[0]: "libraries/Matter/", sketches[1]: "warm_s"}
    bins = assign_lpt(sketches, costs, max_chunks=15)
    text = format_assignment_stats(
        bins,
        costs,
        sources,
        details,
        "warm",
        15,
        {},
        None,
        None,
    )
    assert "mode=warm  sketches=2  chunks=2" in text
    assert "cost sources: measured=0  scaled=0  prefix=1  default=1" in text
    assert "heaviest sketches:" in text
    assert "libraries/Matter/x/x.ino" in text


def test_write_github_output(tmp_path: Path, monkeypatch) -> None:
    out = tmp_path / "github_output"
    monkeypatch.setenv("GITHUB_OUTPUT", str(out))
    write_github_output([["a.ino"], ["b.ino"]])
    text = out.read_text(encoding="utf-8")
    assert "chunk_count=2\n" in text
    assert 'chunks=["0","1"]\n' in text


def test_main_lpt_when_more_sketches_than_chunks(tmp_path: Path, monkeypatch) -> None:
    from assign_chunks import main

    affected = tmp_path / "affected_sketches.txt"
    sketches = [f"libraries/Lib{i}/e/e.ino" for i in range(20)]
    sketches[0] = "libraries/Matter/examples/x/x.ino"
    affected.write_text("\n".join(sketches) + "\n", encoding="utf-8")
    out = tmp_path / "github_output"
    monkeypatch.setenv("GITHUB_OUTPUT", str(out))
    assert (
        main(
            [
                "--input",
                str(affected),
                "--out",
                str(tmp_path),
                "--max-chunks",
                "5",
                "--mode",
                "warm",
            ]
        )
        == 0
    )
    files = sorted(tmp_path.glob("chunk_*.txt"))
    assert [p.name for p in files] == [f"chunk_{i}.txt" for i in range(5)]
    assigned = [line for p in files for line in p.read_text().splitlines() if line]
    assert len(assigned) == 20
    assert "libraries/Matter/examples/x/x.ino" in assigned
    assert "chunk_count=5\n" in out.read_text()
