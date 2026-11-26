#!/usr/bin/env python3

from __future__ import annotations

import json
import sys
from pathlib import Path

import pytest

SCRIPT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPT_DIR))

from build_upload_matrix import build_matrix, load_core_socs, main, resolve_target_socs, split_shards  # noqa: E402


def test_load_core_socs_reads_socs_config() -> None:
    socs = load_core_socs()
    assert "esp32" in socs
    assert "esp32s3" in socs
    assert "esp32c2" not in socs


def test_split_shards_puts_smaller_chunks_first() -> None:
    socs = ["s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8"]
    shards = split_shards(socs, 4)
    assert [len(s) for s in shards] == [2, 2, 2, 3]
    assert sum(shards, []) == socs


def test_resolve_target_socs_scopes_boards_only_pr() -> None:
    core_socs = ["esp32s31", "esp32", "esp32c3", "esp32s3"]
    fqbns = json.dumps({"fqbn": ["espressif:esp32:esp32s3:FlashSize=4M", "espressif:esp32:esp32:PSRAM=enabled"]})
    target, reason = resolve_target_socs(
        event="pull_request",
        upload_changed=False,
        fqbns_raw=fqbns,
        input_socs="",
        core_socs=core_socs,
    )
    assert target == ["esp32", "esp32s3"]
    assert "boards.txt-only PR" in reason


def test_resolve_target_socs_skips_when_nothing_relevant_changed() -> None:
    target, _ = resolve_target_socs(
        event="pull_request",
        upload_changed=False,
        fqbns_raw="",
        input_socs="",
        core_socs=["esp32", "esp32s3"],
    )
    assert target == []


def test_resolve_target_socs_rejects_unknown_dispatch_soc() -> None:
    with pytest.raises(ValueError, match="Unknown SoC"):
        resolve_target_socs(
            event="workflow_dispatch",
            upload_changed=False,
            fqbns_raw="",
            input_socs="esp32,esp99",
            core_socs=["esp32", "esp32s3"],
        )


def test_build_matrix_all_tool_and_single_soc() -> None:
    all_jobs = build_matrix([f"soc{i}" for i in range(9)], "all")["include"]
    assert len(all_jobs) == 9
    assert sum(1 for j in all_jobs if j["os"] == "macos-26-intel") == 4

    single_jobs = build_matrix(["esp32s3"], "all")["include"]
    assert len(single_jobs) == 3
    assert [j["os"] for j in single_jobs] == ["ubuntu-latest", "windows-latest", "macos-26-intel"]
    assert all(j["socs"] == "esp32s3" for j in single_jobs)


def test_main_writes_github_output(tmp_path: Path) -> None:
    gh_out = tmp_path / "github_output.txt"
    rc = main(
        [
            "--event",
            "workflow_dispatch",
            "--socs",
            "esp32c3,esp32s3",
            "--tool",
            "cli",
            "--core-socs",
            "esp32,esp32c3,esp32s3",
            "--github-output",
            str(gh_out),
        ]
    )
    assert rc == 0
    lines = dict(line.split("=", 1) for line in gh_out.read_text(encoding="utf-8").splitlines())
    assert lines["should_run"] == "true"
    matrix = json.loads(lines["matrix"])
    assert len(matrix["include"]) == 5
