#!/usr/bin/env python3
"""Empty commits must not fail check_official_variants.sh."""

from __future__ import annotations

import os
import subprocess
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "check_official_variants.sh"


def _run(event: str, *changed: str, target: str = "BUILD_TEST_TARGETS") -> tuple[int, str, str]:
    env = os.environ.copy()
    env.pop("GITHUB_OUTPUT", None)
    cmd = ["bash", str(SCRIPT), event, target, *changed]
    proc = subprocess.run(cmd, capture_output=True, text=True, env=env, check=False)
    return proc.returncode, proc.stdout, proc.stderr


def test_push_empty_commit_runs() -> None:
    code, stdout, stderr = _run("push")
    assert code == 0, stderr
    assert stdout.strip().splitlines()[-1] == "true"


def test_schedule_empty_commit_runs() -> None:
    code, stdout, stderr = _run("schedule")
    assert code == 0, stderr
    assert stdout.strip().splitlines()[-1] == "true"


def test_pr_empty_commit_skips() -> None:
    code, stdout, stderr = _run("pull_request")
    assert code == 0, stderr
    assert stdout.strip().splitlines()[-1] == "false"


def test_pr_core_change_runs() -> None:
    code, stdout, stderr = _run("pull_request", "cores/esp32/esp32-hal-gpio.c")
    assert code == 0, stderr
    assert stdout.strip().splitlines()[-1] == "true"


def test_missing_event_fails() -> None:
    env = os.environ.copy()
    env.pop("GITHUB_OUTPUT", None)
    proc = subprocess.run(
        ["bash", str(SCRIPT)],
        capture_output=True,
        text=True,
        env=env,
        check=False,
    )
    assert proc.returncode != 0
    assert "event_name is required" in (proc.stdout + proc.stderr)
