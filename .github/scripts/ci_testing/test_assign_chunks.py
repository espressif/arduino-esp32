#!/usr/bin/env python3

from __future__ import annotations

import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPT_DIR))

from assign_chunks import (  # noqa: E402
    assign_lpt,
    resolve_cost,
    fallback_tables,
    family_medians,
    measured_target_count,
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


def test_measured_cost_wins_without_family_table() -> None:
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
    assert "cost sources: family=0  measured=0  scaled=0  last=0  prefix=1  default=1" in text
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


def test_family_medians_ignore_first_build_outliers() -> None:
    # One sketch per family pays for the family's static libraries; the rest
    # are marginal. The median must not carry that one-time cost for all of them.
    measured = {
        "sketches": {
            "libraries/Matter/first/first.ino": {
                "by_target": {"esp32": {"cold_s": 80.0}, "esp32s3": {"cold_s": 90.0}}
            },
            "libraries/Matter/later/later.ino": {
                "by_target": {"esp32": {"cold_s": 20.0}, "esp32s3": {"cold_s": 22.0}}
            },
            "libraries/Matter/other/other.ino": {
                "by_target": {"esp32": {"cold_s": 18.0}, "esp32s3": {"cold_s": 19.0}}
            },
            "libraries/ESP32/gpio/gpio.ino": {"by_target": {"esp32": {"cold_s": 3.5}}},
        }
    }
    family = family_medians(measured, "cold", FALLBACK)
    assert family["Matter"] == 21.0
    assert family["ESP32"] == 3.5
    # 21s/target * 2 targets for every Matter sketch, the outlier included
    for sketch in (
        "libraries/Matter/first/first.ino",
        "libraries/Matter/later/later.ino",
    ):
        cost = sketch_cost(sketch, "cold", measured, DEFAULT, PREFIXES, FALLBACK, family)
        assert cost == 42.0


def test_family_medians_follow_the_requested_mode() -> None:
    measured = {
        "sketches": {
            "libraries/Matter/a/a.ino": {
                "by_target": {"esp32": {"warm_s": 12.0, "cold_s": 99.0}}
            }
        }
    }
    assert family_medians(measured, "warm", FALLBACK)["Matter"] == 12.0
    assert family_medians(measured, "cold", FALLBACK)["Matter"] == 99.0


def test_family_cost_falls_back_to_measured_value() -> None:
    measured = {
        "sketches": {
            "libraries/Matter/a/a.ino": {
                "by_target": {"esp32": {"cold_s": 60.0}, "esp32s3": {"cold_s": 62.0}}
            }
        }
    }
    # No family table (e.g. all entries were filtered out): use the sketch value.
    cost = sketch_cost(
        "libraries/Matter/a/a.ino", "cold", measured, DEFAULT, PREFIXES, FALLBACK, {}
    )
    assert cost == 122.0


def test_assignment_stats_show_family_costs() -> None:
    measured = {
        "sketches": {
            "libraries/Matter/a/a.ino": {"by_target": {"esp32": {"cold_s": 20.0}}},
            "libraries/Matter/b/b.ino": {"by_target": {"esp32": {"cold_s": 22.0}}},
        }
    }
    family = family_medians(measured, "cold", FALLBACK)
    costs = {"libraries/Matter/a/a.ino": 21.0, "libraries/Matter/b/b.ino": 21.0}
    bins = assign_lpt(list(costs), costs, max_chunks=2)
    text = format_assignment_stats(
        bins,
        costs,
        {s: "family" for s in costs},
        {s: "Matter" for s in costs},
        "cold",
        2,
        measured,
        None,
        None,
        family,
    )
    assert "family cost: Matter=21.0s" in text
    assert "family=2" in text


def test_target_count_comes_from_the_database() -> None:
    measured = {
        "sketches": {
            "libraries/A/a.ino": {"by_target": {"esp32": {"cold_s": 1.0}, "esp32s3": {"cold_s": 1.0}}},
            "libraries/B/b.ino": {"by_target": {"esp32": {"cold_s": 1.0}}},
            "libraries/C/c.ino": {"by_target": {"esp32": {"cold_s": 1.0}}},
            "libraries/D/d.ino": {"by_target": {"esp32": {"cold_s": 1.0}}},
        }
    }
    # Median of 2,1,1,1 -> 1.
    assert measured_target_count(measured) == 1


def test_target_count_ignores_entries_without_by_target() -> None:
    measured = {
        "sketches": {
            "libraries/A/a.ino": {"cold_s": 10.0},
            "libraries/B/b.ino": {"by_target": {"esp32": {"cold_s": 1.0}}},
            "libraries/C/c.ino": {"by_target": {"esp32": {"cold_s": 1.0}}},
        }
    }
    assert measured_target_count(measured) == 1
    assert measured_target_count({}) == 1


def test_legacy_entry_uses_database_target_count() -> None:
    # No by_target on the entry that needs a cost: scale the family median by
    # the typical number of targets instead of a hardcoded constant.
    measured = {
        "sketches": {
            "libraries/Matter/known/known.ino": {
                "by_target": {"esp32": {"cold_s": 20.0}, "esp32s3": {"cold_s": 22.0}}
            },
            "libraries/Matter/other/other.ino": {
                "by_target": {"esp32": {"cold_s": 18.0}, "esp32s3": {"cold_s": 19.0}}
            },
            "libraries/Matter/legacy/legacy.ino": {"cold_s": 900.0},
        }
    }
    family = family_medians(measured, "cold", FALLBACK)
    cost = sketch_cost(
        "libraries/Matter/legacy/legacy.ino", "cold", measured, DEFAULT, PREFIXES, FALLBACK, family
    )
    # 2 targets (the database median) * 19.5 s family median.
    assert cost == 39.0


def test_last_sample_is_used_when_no_pure_sample_exists() -> None:
    # Sketch only ever appeared in a mixed run: use the recorded sample instead
    # of the in-repo fallback.
    measured = {
        "sketches": {
            "libraries/NewThing/x/x.ino": {
                "last_s": 240.0,
                "by_target": {"esp32": {"last_s": 120.0}, "esp32s3": {"last_s": 120.0}},
            }
        }
    }
    cost, source, detail = resolve_cost(
        "libraries/NewThing/x/x.ino", "warm", measured, DEFAULT, PREFIXES, FALLBACK
    )
    assert (cost, source, detail) == (240.0, "last", "last_s")


def test_family_median_still_wins_over_last_sample() -> None:
    measured = {
        "sketches": {
            "libraries/Matter/a/a.ino": {"by_target": {"esp32": {"cold_s": 20.0}}},
            "libraries/Matter/b/b.ino": {"by_target": {"esp32": {"cold_s": 22.0}}},
            "libraries/Matter/new/new.ino": {
                "last_s": 999.0,
                "by_target": {"esp32": {"last_s": 999.0}},
            },
        }
    }
    family = family_medians(measured, "cold", FALLBACK)
    cost, source, _ = resolve_cost(
        "libraries/Matter/new/new.ino", "cold", measured, DEFAULT, PREFIXES, FALLBACK, family
    )
    # Family median (21.0) * 1 target, not the mixed 999.
    assert (cost, source) == (21.0, "family")
