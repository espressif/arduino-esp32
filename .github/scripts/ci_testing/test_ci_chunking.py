#!/usr/bin/env python3

import json
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
PROJECT_ROOT = SCRIPT_DIR.parent.parent
sys.path.insert(0, str(SCRIPT_DIR))

from ci_chunking.affinity import (
    affinity_key,
    find_sketches_under,
    libraries_from_graph,
    library_of_path,
)
from ci_chunking.costs import (
    CACHED_COLD_FRACTION,
    CostTable,
    DEFAULT_COLD,
    EMA_ALPHA,
    SEC_PER_SOURCE,
    SKETCH_REMAINDER,
    bin_cost,
    cold_cost,
    library_fingerprint,
    prior_cold,
    source_count,
    static_archive_bytes,
    warm_cost,
)
from ci_chunking.pack import assign, write_assignment


def _matter_sketches(count: int, header: str = "Matter.h") -> list[str]:
    return [
        f"libraries/Matter/examples/Sketch{index}/Sketch{index}.ino"
        for index in range(count)
    ]


def test_library_of_path_uses_libraries_folder() -> None:
    assert library_of_path("libraries/Matter/examples/OnOff/OnOff.ino") == "Matter"
    assert library_of_path("Matter/examples/OnOff/OnOff.ino") == "Matter"
    assert library_of_path("cores/esp32/Arduino.h") == "core"
    assert library_of_path("tests/validation/ble/server/server.ino") == "other"


def test_path_fallback_groups_by_parent_library() -> None:
    sketches = [
        "libraries/WiFi/examples/A/A.ino",
        "libraries/WiFi/examples/B/B.ino",
        "libraries/HTTPClient/examples/C/C.ino",
    ]
    assignment = assign(sketches, max_chunks=15)
    assert assignment.groups["WiFi"] == sketches[:2]
    wifi_bins = [
        index
        for index, bin_sketches in enumerate(assignment.bins)
        if any("WiFi" in sketch for sketch in bin_sketches)
    ]
    assert len(wifi_bins) == 1


def test_packing_is_deterministic() -> None:
    sketches = [
        "libraries/Matter/examples/One/One.ino",
        "libraries/WiFi/examples/Two/Two.ino",
        "libraries/BLE/examples/Three/Three.ino",
        "libraries/SD/examples/Four/Four.ino",
        "libraries/HTTPClient/examples/Five/Five.ino",
    ]
    first = assign(sketches, max_chunks=3)
    second = assign(list(reversed(sketches)), max_chunks=3)
    assert first.bins == second.bins
    assert first.costs == second.costs


def test_matter_sketches_stay_colocated() -> None:
    sketches = _matter_sketches(4) + [
        "libraries/WiFi/examples/Scan/Scan.ino",
        "libraries/EEPROM/examples/eeprom_extra/eeprom_extra.ino",
    ]
    assignment = assign(sketches, max_chunks=15)
    matter_bins = [
        index
        for index, bin_sketches in enumerate(assignment.bins)
        if any(sketch.startswith("libraries/Matter/") for sketch in bin_sketches)
    ]
    assert len(matter_bins) == 1
    assert assignment.connectivity.get("Matter") == 1


def test_include_graph_moves_esp32_example_to_matter() -> None:
    sketches = [
        "libraries/ESP32/examples/MatterBridge/MatterBridge.ino",
        "libraries/Matter/examples/OnOff/OnOff.ino",
        "libraries/ESP32/examples/HelloWorld/HelloWorld.ino",
    ]
    dependencies = {
        sketches[0]: ["libraries/Matter/src/Matter.h", "cores/esp32/Arduino.h"],
        sketches[1]: ["libraries/Matter/src/Matter.h", "cores/esp32/Arduino.h"],
        sketches[2]: ["libraries/ESP32/src/ESP32.h", "cores/esp32/Arduino.h"],
        "libraries/Matter/src/Matter.h": [],
        "libraries/ESP32/src/ESP32.h": [],
        "cores/esp32/Arduino.h": [],
    }
    assignment = assign(sketches, max_chunks=15, dependencies=dependencies)
    matter_group = assignment.groups["Matter"]
    assert sketches[0] in matter_group
    assert sketches[1] in matter_group
    assert sketches[2] not in matter_group
    libs = libraries_from_graph(sketches[0], dependencies)
    assert "Matter" in libs
    assert affinity_key(sketches[0], libs) == "Matter"


def test_different_matter_headers_share_one_group() -> None:
    sketches = [
        "libraries/Matter/examples/OnOff/OnOff.ino",
        "libraries/Matter/examples/Light/Light.ino",
    ]
    dependencies = {
        sketches[0]: ["libraries/Matter/src/MatterOnOff.h"],
        sketches[1]: ["libraries/Matter/src/MatterLight.h"],
        "libraries/Matter/src/MatterOnOff.h": [],
        "libraries/Matter/src/MatterLight.h": [],
    }
    assignment = assign(sketches, max_chunks=15, dependencies=dependencies)
    assert assignment.groups["Matter"] == sorted(sketches)
    assert assignment.chunk_count == 1


def test_group_splits_only_when_it_exceeds_a_fair_bin() -> None:
    cheap = [f"libraries/EEPROM/examples/S{i}/S{i}.ino" for i in range(20)]
    assignment = assign(cheap, max_chunks=5)
    assert assignment.splits == []
    assert assignment.chunk_count <= 5

    mixed = assign(_matter_sketches(8) + cheap, max_chunks=5)
    matter_bins = [
        index
        for index, bin_sketches in enumerate(mixed.bins)
        if any(sketch.startswith("libraries/Matter/") for sketch in bin_sketches)
    ]
    assert 1 <= len(matter_bins) <= 2


def test_bin_cost_is_library_union_not_cpp_count() -> None:
    sketches = _matter_sketches(3)
    libraries_of = {sketch: frozenset({"Matter"}) for sketch in sketches}
    one = bin_cost(sketches[:1], libraries_of)
    three = bin_cost(sketches, libraries_of)
    assert three < one * 3
    assert three == one + 2 * warm_cost("Matter") + 2 * SKETCH_REMAINDER


def test_write_assignment_and_cli(tmp_path: Path) -> None:
    sketches = _matter_sketches(3) + ["libraries/WiFi/examples/Scan/Scan.ino"]
    listing = tmp_path / "affected_sketches.txt"
    listing.write_text("".join(f"{sketch}\n" for sketch in sketches), encoding="utf-8")
    out_dir = tmp_path / "chunks"
    result = subprocess.run(
        [
            sys.executable,
            str(SCRIPT_DIR / "chunk_assign.py"),
            "--list",
            str(listing),
            "--max",
            "15",
            "--out-dir",
            str(out_dir),
            "--print-summary",
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    assert result.returncode == 0, result.stderr
    payload = json.loads((out_dir / "chunks.json").read_text(encoding="utf-8"))
    assert payload["chunk_count"] == int(result.stdout.strip())
    assert payload["cmax"] > 0
    written = []
    for index in range(payload["chunk_count"]):
        chunk_file = out_dir / f"chunk_{index}.txt"
        assert chunk_file.is_file()
        written.extend(chunk_file.read_text(encoding="utf-8").splitlines())
    assert sorted(written) == sorted(sketches)
    assert "Cmax=" in result.stderr
    assert "libs=" in result.stderr


def test_from_tree_libraries_emits_project_relative_paths(tmp_path: Path) -> None:
    libraries = tmp_path / "libraries"
    for lib, name in (("Matter", "OnOff"), ("WiFi", "Scan"), ("BLE", "Server")):
        sketch = libraries / lib / "examples" / name
        sketch.mkdir(parents=True)
        (sketch / f"{name}.ino").write_text("void setup() {}\n", encoding="utf-8")
    out_dir = tmp_path / "chunks"
    result = subprocess.run(
        [
            sys.executable,
            str(SCRIPT_DIR / "chunk_assign.py"),
            "--from-tree",
            str(libraries),
            "--max",
            "15",
            "--out-dir",
            str(out_dir),
            "--root",
            str(tmp_path),
        ],
        check=False,
        capture_output=True,
        text=True,
        cwd=tmp_path,
    )
    assert result.returncode == 0, result.stderr
    payload = json.loads((out_dir / "chunks.json").read_text(encoding="utf-8"))
    written = []
    for index in range(payload["chunk_count"]):
        written.extend((out_dir / f"chunk_{index}.txt").read_text(encoding="utf-8").splitlines())
    assert all(path.startswith("libraries/") for path in written)
    assert payload["chunk_count"] >= 2
    assert "Matter" in result.stderr
    assert "libs=[]" not in result.stderr


def test_write_assignment_uses_sketch_paths(tmp_path: Path) -> None:
    sketches = [
        "libraries/Matter/examples/OnOff/OnOff.ino",
        "libraries/WiFi/examples/Scan/Scan.ino",
    ]
    assignment = assign(sketches, max_chunks=2)
    write_assignment(assignment, tmp_path)
    listed = []
    for index, _ in enumerate(assignment.bins):
        listed.extend((tmp_path / f"chunk_{index}.txt").read_text(encoding="utf-8").splitlines())
    assert sorted(listed) == sorted(sketches)
    assert cold_cost("Matter") > cold_cost("WiFi")


def _write_library_sources(root: Path, library: str, files: dict[str, str]) -> None:
    src = root / "libraries" / library / "src"
    src.mkdir(parents=True)
    (root / "libraries" / library / "library.properties").write_text(
        f"name={library}\n", encoding="utf-8"
    )
    for relative, contents in files.items():
        path = src / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents, encoding="utf-8")


def test_prior_cold_scales_with_source_count(tmp_path: Path) -> None:
    n_files = 50
    _write_library_sources(
        tmp_path,
        "HeavyLib",
        {f"file_{index}.cpp": f"int v{index};\n" for index in range(n_files)},
    )
    assert source_count("HeavyLib", tmp_path) == n_files
    assert prior_cold("HeavyLib", tmp_path) == int(round(SEC_PER_SOURCE * n_files))
    assert prior_cold("MissingLib", tmp_path) == DEFAULT_COLD


def test_fingerprint_change_blends_toward_new_prior(tmp_path: Path) -> None:
    _write_library_sources(tmp_path, "HeavyLib", {"a.cpp": "int a;\n"})
    old_fp = library_fingerprint("HeavyLib", tmp_path)
    stored = {"HeavyLib": {"cold": 200, "warm": 40, "fingerprint": old_fp, "samples": 4}}
    assert CostTable(tmp_path, stored).cold("HeavyLib") == 200
    (tmp_path / "libraries" / "HeavyLib" / "src" / "b.cpp").write_text("int b;\n", encoding="utf-8")
    blended = CostTable(tmp_path, stored).cold("HeavyLib")
    assert blended != 200
    assert blended == int(round(0.3 * 200 + 0.7 * prior_cold("HeavyLib", tmp_path)))


def test_update_from_timings_ema_and_cached_guard(tmp_path: Path) -> None:
    _write_library_sources(
        tmp_path,
        "Matter",
        {f"file_{index}.cpp": f"int v{index};\n" for index in range(20)},
    )
    table = CostTable(tmp_path)
    table.update_from_timings(
        [
            {"path": "libraries/Matter/examples/A/A.ino", "seconds": 100},
            {"path": "libraries/Matter/examples/B/B.ino", "seconds": 20},
        ]
    )
    assert table.entries["Matter"]["cold"] == 100
    assert table.entries["Matter"]["warm"] == 20
    fp = table.entries["Matter"]["fingerprint"]
    assert fp == library_fingerprint("Matter", tmp_path)

    table.update_from_timings(
        [
            {"path": "libraries/Matter/examples/A/A.ino", "seconds": 110},
            {"path": "libraries/Matter/examples/B/B.ino", "seconds": 22},
        ]
    )
    assert table.entries["Matter"]["cold"] == int(round((1.0 - EMA_ALPHA) * 100 + EMA_ALPHA * 110))
    assert table.entries["Matter"]["warm"] == int(round((1.0 - EMA_ALPHA) * 20 + EMA_ALPHA * 22))
    assert table.entries["Matter"]["samples"] == 2

    assert 2 < CACHED_COLD_FRACTION * prior_cold("Matter", tmp_path)
    table.update_from_timings(
        [{"path": "libraries/Matter/examples/A/A.ino", "seconds": 2}]
    )
    assert table.entries["Matter"]["cold"] == int(round((1.0 - EMA_ALPHA) * 100 + EMA_ALPHA * 110))


def test_cli_update_costs_roundtrip(tmp_path: Path) -> None:
    timings = tmp_path / "compile_timings_0.jsonl"
    timings.write_text(
        json.dumps(
            {"path": "libraries/Matter/examples/OnOff/OnOff.ino", "seconds": 95, "target": "esp32"}
        )
        + "\n"
        + json.dumps({"path": "libraries/Matter/examples/Light/Light.ino", "seconds": 18})
        + "\n",
        encoding="utf-8",
    )
    costs = tmp_path / "compile_costs.json"
    result = subprocess.run(
        [
            sys.executable,
            str(SCRIPT_DIR / "chunk_assign.py"),
            "--update-costs",
            str(timings),
            "--costs",
            str(costs),
            "--root",
            str(tmp_path),
        ],
        check=False,
        capture_output=True,
        text=True,
        cwd=tmp_path,
    )
    assert result.returncode == 0, result.stderr
    payload = json.loads(costs.read_text(encoding="utf-8"))
    assert payload["libraries"]["Matter"]["cold"] == 95
    assert payload["libraries"]["Matter"]["warm"] == 18
    assert int(result.stdout.strip()) == 1


def test_assign_uses_measured_costs(tmp_path: Path) -> None:
    sketches = _matter_sketches(2) + ["libraries/WiFi/examples/Scan/Scan.ino"]
    cheap = CostTable(
        tmp_path,
        {
            "Matter": {"cold": 8, "warm": 2, "fingerprint": "", "samples": 1},
            "WiFi": {"cold": 8, "warm": 2, "fingerprint": "", "samples": 1},
        },
    )
    measured = assign(sketches, max_chunks=15, cost_table=cheap)
    seeded = assign(sketches, max_chunks=15, cost_table=CostTable())
    assert measured.costs != seeded.costs
    assert measured.cost_table is cheap


LIVE_LIBS = ("Matter", "BLE", "WiFi", "EEPROM", "HTTPClient")


def _live_sketches() -> list[str]:
    sketches: list[str] = []
    for lib in LIVE_LIBS:
        lib_root = PROJECT_ROOT / "libraries" / lib
        if lib_root.is_dir():
            sketches.extend(find_sketches_under(lib_root, relative_to=PROJECT_ROOT))
    return sketches


def test_cli_update_costs_missing_path(tmp_path: Path) -> None:
    result = subprocess.run(
        [
            sys.executable,
            str(SCRIPT_DIR / "chunk_assign.py"),
            "--update-costs",
            str(tmp_path / "compile_timings"),
            "--costs",
            str(tmp_path / "compile_costs.json"),
            "--root",
            str(tmp_path),
        ],
        check=False,
        capture_output=True,
        text=True,
        cwd=tmp_path,
    )
    assert result.returncode != 0
    assert "timing path not found" in result.stderr


def test_live_subset_priors_rank_expensive_libraries() -> None:
    if not (PROJECT_ROOT / "libraries" / "Matter" / "src").is_dir():
        return
    table = CostTable(PROJECT_ROOT)
    assert source_count("Matter", PROJECT_ROOT) > source_count("EEPROM", PROJECT_ROOT)
    assert table.cold("Matter") > table.cold("EEPROM")
    assert table.cold("Matter") >= 100
    assert table.cold("Matter") > table.cold("BLE")
    assert table.cold("Matter") > table.cold("WiFi")
    assert table.cold("WiFi") > table.cold("EEPROM")
    assert table.cold("BLE") > table.cold("EEPROM")
    assert library_fingerprint("Matter", PROJECT_ROOT)
    assert library_fingerprint("BLE", PROJECT_ROOT) != library_fingerprint("WiFi", PROJECT_ROOT)
    archives = PROJECT_ROOT / "tools" / "esp32-arduino-libs" / "esp32" / "lib"
    if archives.is_dir():
        assert static_archive_bytes("Matter", PROJECT_ROOT) > 10_000_000
        assert static_archive_bytes("Matter", PROJECT_ROOT) > static_archive_bytes("EEPROM", PROJECT_ROOT)


def test_live_subset_packing_and_cost_roundtrip(tmp_path: Path) -> None:
    sketches = _live_sketches()
    if len(sketches) < 20:
        return
    prior = assign(sketches, max_chunks=4, project_root=PROJECT_ROOT)
    assert prior.chunk_count >= 2
    assert prior.chunk_count <= 4
    matter_bins = [
        index
        for index, bin_sketches in enumerate(prior.bins)
        if any(path.startswith("libraries/Matter/") for path in bin_sketches)
    ]
    assert matter_bins
    assert all(
        any(path.startswith("libraries/") for path in bin_sketches)
        for bin_sketches in prior.bins
    )

    timings = tmp_path / "compile_timings_0.jsonl"
    lines = []
    seen_lib: dict[str, int] = {}
    for sketch in sketches:
        lib = library_of_path(sketch)
        copies = seen_lib.get(lib, 0)
        seen_lib[lib] = copies + 1
        seconds = 90 if lib == "Matter" and copies == 0 else 12 if lib == "Matter" else 8
        if lib == "BLE" and copies == 0:
            seconds = 40
        elif lib == "BLE":
            seconds = 7
        lines.append(json.dumps({"path": sketch, "seconds": seconds, "target": "esp32"}))
    timings.write_text("".join(f"{line}\n" for line in lines), encoding="utf-8")
    costs = tmp_path / "compile_costs.json"
    result = subprocess.run(
        [
            sys.executable,
            str(SCRIPT_DIR / "chunk_assign.py"),
            "--update-costs",
            str(timings),
            "--costs",
            str(costs),
            "--root",
            str(PROJECT_ROOT),
        ],
        check=False,
        capture_output=True,
        text=True,
        cwd=tmp_path,
    )
    assert result.returncode == 0, result.stderr
    payload = json.loads(costs.read_text(encoding="utf-8"))
    assert payload["libraries"]["Matter"]["cold"] == 90
    assert payload["libraries"]["Matter"]["warm"] == 12
    assert payload["libraries"]["BLE"]["cold"] == 40
    measured = assign(
        sketches,
        max_chunks=4,
        project_root=PROJECT_ROOT,
        cost_table=CostTable.load(costs, PROJECT_ROOT),
    )
    assert measured.chunk_count >= 1
    assert measured.cmax != prior.cmax or measured.costs != prior.costs
