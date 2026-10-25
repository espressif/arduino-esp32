#!/usr/bin/env python3

import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPT_DIR))

from ccache_writer_chunk import chunk_size_for, writer_chunk  # noqa: E402


def test_matter_lands_in_sorted_slice() -> None:
    sketches = [
        "libraries/ESP32/examples/Hash/Hash.ino",
        "libraries/Matter/examples/MatterOnOffLight/MatterOnOffLight.ino",
        "libraries/WiFi/examples/WiFiClient/WiFiClient.ino",
        "libraries/Zigbee/examples/Zigbee_OnOff/Zigbee_OnOff.ino",
    ]
    # 4 sketches, 2 chunks → size 2. Sorted: ESP32, Matter, WiFi, Zigbee.
    # Matter is index 1 → chunk 0.
    assert writer_chunk(sketches, 2) == 0


def test_matter_second_half() -> None:
    sketches = [
        f"libraries/AAA/examples/s{i}/s{i}.ino" for i in range(10)
    ] + ["libraries/Matter/examples/X/X.ino"]
    # 11 sketches, 2 chunks → size 6. Matter is last (index 10) → chunk 1.
    assert writer_chunk(sketches, 2) == 1


def test_no_matter_falls_back_to_zero() -> None:
    sketches = ["libraries/Hash/examples/Hash/Hash.ino"]
    assert writer_chunk(sketches, 15) == 0


def test_empty_list() -> None:
    assert writer_chunk([], 15) == 0


def test_one_sketch_per_chunk() -> None:
    sketches = [
        "libraries/A/examples/a/a.ino",
        "libraries/Matter/examples/m/m.ino",
        "libraries/Z/examples/z/z.ino",
    ]
    assert writer_chunk(sketches, 3) == 1


def test_chunk_size_matches_sketch_utils() -> None:
    assert chunk_size_for(362, 15) == 25
    assert chunk_size_for(15, 15) == 1
    assert chunk_size_for(0, 15) == 0
