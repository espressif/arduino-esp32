#!/usr/bin/env python3

from __future__ import annotations

import sys
from datetime import datetime, timedelta, timezone
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPT_DIR))

from prune_caches import (  # noqa: E402
    format_summary,
    plan_deletions,
)

NOW = datetime(2026, 11, 13, 12, 0, tzinfo=timezone.utc)
PREFIX = "ccache-linux-" + "a" * 64
SHA_A = "1" * 40
SHA_B = "2" * 40
SHA_C = "3" * 40


def cache(
    cache_id: int,
    key: str,
    hours_old: float,
    ref: str = "refs/heads/master",
    mb: int = 1000,
    accessed_hours_ago: float | None = None,
) -> dict:
    created = NOW - timedelta(hours=hours_old)
    accessed = NOW - timedelta(
        hours=hours_old if accessed_hours_ago is None else accessed_hours_ago
    )
    return {
        "id": cache_id,
        "key": key,
        "ref": ref,
        "created_at": created.strftime("%Y-%m-%dT%H:%M:%SZ"),
        "last_accessed_at": accessed.strftime("%Y-%m-%dT%H:%M:%SZ"),
        "size_in_bytes": mb * 1024 * 1024,
    }


def test_keeps_newest_ccache_generation() -> None:
    caches = [
        cache(1, f"{PREFIX}-{SHA_A}", 100),
        cache(2, f"{PREFIX}-{SHA_B}", 10),
    ]
    assert plan_deletions(caches, now=NOW) == {
        1: f"older {PREFIX} generation on refs/heads/master"
    }


def test_deletes_superseded_generation_immediately() -> None:
    # Called right after a save, so the previous generation is minutes old.
    caches = [
        cache(1, f"{PREFIX}-{SHA_A}", 0.05),
        cache(2, f"{PREFIX}-{SHA_B}", 0.01),
    ]
    assert plan_deletions(caches, now=NOW, min_age_hours=0) == {
        1: f"older {PREFIX} generation on refs/heads/master"
    }


def test_generations_are_scoped_per_ref() -> None:
    caches = [
        cache(1, f"{PREFIX}-{SHA_A}", 100, ref="refs/heads/master"),
        cache(2, f"{PREFIX}-{SHA_B}", 100, ref="refs/heads/release/v4.0.x"),
    ]
    # Different refs cannot restore each other's caches, so both are newest.
    assert plan_deletions(caches, now=NOW) == {}


def test_different_prefixes_are_independent() -> None:
    other = "ccache-windows-" + "b" * 64
    caches = [
        cache(1, f"{PREFIX}-{SHA_A}", 100),
        cache(2, f"{PREFIX}-{SHA_B}", 10),
        cache(3, f"{other}-{SHA_C}", 100),
    ]
    assert plan_deletions(caches, now=NOW) == {
        1: f"older {PREFIX} generation on refs/heads/master"
    }


def test_min_age_protects_recent_generations() -> None:
    caches = [
        cache(1, f"{PREFIX}-{SHA_A}", 1),
        cache(2, f"{PREFIX}-{SHA_B}", 0.5),
    ]
    assert plan_deletions(caches, now=NOW, min_age_hours=2) == {}


def test_single_generation_is_never_deleted() -> None:
    caches = [cache(1, f"{PREFIX}-{SHA_A}", 24 * 30)]
    assert plan_deletions(caches, now=NOW) == {}


def test_unrelated_keys_are_ignored() -> None:
    caches = [
        cache(1, "libs-Linux-X64-" + "e" * 64, 24 * 30),
        cache(2, "cache-apt-pkgs_" + "f" * 32, 24 * 30),
        cache(3, "setup-python-Linux-x64-26.04-Ubuntu-python-3.15.2-pip-" + "a" * 64, 24 * 30),
        # Legacy per-PR binary caches are left to the 7-day inactivity cleanup.
        cache(4, "test-13112-" + "c" * 40 + "-esp32s3-validation-bin", 24 * 30),
    ]
    assert plan_deletions(caches, now=NOW) == {}


def test_plan_is_idempotent() -> None:
    caches = [
        cache(1, f"{PREFIX}-{SHA_A}", 100),
        cache(2, f"{PREFIX}-{SHA_B}", 10),
    ]
    first = plan_deletions(caches, now=NOW)
    remaining = [c for c in caches if int(c["id"]) not in first]
    assert plan_deletions(remaining, now=NOW) == {}


def test_summary_reports_freed_space() -> None:
    caches = [cache(1, f"{PREFIX}-{SHA_A}", 100, mb=1300), cache(2, f"{PREFIX}-{SHA_B}", 10)]
    reasons = plan_deletions(caches, now=NOW)
    text = format_summary(caches, reasons)
    assert "to delete: 1" in text
    assert "freeing: 1300 MB" in text
