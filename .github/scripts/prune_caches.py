#!/usr/bin/env python3
"""Delete superseded GitHub Actions ccache generations.

Every push to master or a release branch saves a new
``ccache-<family>-<prefix>-<sha>`` entry (~1.3-1.6 GB). Restore keys always
resolve to the newest generation of a prefix, so older generations are dead
weight. Left alone they accumulate until the repository exceeds its 10 GB
cache quota, and GitHub evicts by last access: a branch that has been quiet
for a few hours loses its newest generation to the churn (measured on
espressif/arduino-esp32: master's cache was saved at 23:18 and evicted by
02:36 while four toolchain caches were being written).

This keeps exactly one generation per ``(ref, key prefix)``. The newest
generation is never deleted, nor is anything younger than
``--min-age-hours``. Abandoned branches need no handling here: GitHub deletes
caches that have not been accessed for 7 days, and anything else outside the
``ccache-*`` family is left untouched.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from datetime import datetime, timezone
from urllib.error import HTTPError
from urllib.request import Request, urlopen

# ccache generations are saved as <prefix>-<40 hex sha>; the prefix contains the
# hash of the files that make a cache reusable across commits.
CCACHE_KEY_RE = re.compile(r"^(?P<prefix>ccache-.+)-(?P<sha>[0-9a-f]{40})$")


def parse_time(value: str) -> datetime:
    text = str(value or "").replace("Z", "+00:00")
    return datetime.fromisoformat(text)


def age_hours(value: str, now: datetime) -> float:
    try:
        then = parse_time(value)
    except ValueError:
        return 0.0
    if then.tzinfo is None:
        then = then.replace(tzinfo=timezone.utc)
    return max(0.0, (now - then).total_seconds() / 3600.0)


def plan_deletions(
    caches: list[dict],
    now: datetime | None = None,
    min_age_hours: float = 0.0,
) -> dict[int, str]:
    """Return {cache_id: reason} for the generations that may be deleted."""
    now = now or datetime.now(timezone.utc)
    reasons: dict[int, str] = {}

    generations: dict[tuple[str, str], list[dict]] = {}
    for cache in caches:
        match = CCACHE_KEY_RE.match(str(cache.get("key") or ""))
        if match:
            generations.setdefault(
                (str(cache.get("ref") or ""), match.group("prefix")), []
            ).append(cache)

    for (ref, prefix), entries in generations.items():
        entries = sorted(entries, key=lambda c: str(c.get("created_at") or ""), reverse=True)
        # entries[0] is the newest generation and is always kept.
        for cache in entries[1:]:
            if age_hours(cache.get("last_accessed_at") or cache.get("created_at"), now) >= min_age_hours:
                reasons[int(cache["id"])] = f"older {prefix} generation on {ref}"

    return reasons


def _request(url: str, token: str, method: str = "GET"):
    request = Request(
        url,
        method=method,
        headers={
            "Authorization": f"Bearer {token}",
            "Accept": "application/vnd.github+json",
            "X-GitHub-Api-Version": "2022-11-28",
            "User-Agent": "arduino-esp32-cache-prune",
        },
    )
    return urlopen(request, timeout=30)


def list_caches(repo: str, token: str, base_url: str = "https://api.github.com") -> list[dict]:
    caches: list[dict] = []
    page = 1
    while True:
        with _request(
            f"{base_url}/repos/{repo}/actions/caches?per_page=100&page={page}", token
        ) as response:
            payload = json.loads(response.read().decode("utf-8"))
        batch = payload.get("actions_caches") or []
        caches.extend(batch)
        if len(batch) < 100:
            break
        page += 1
    return caches


def delete_cache(repo: str, token: str, cache_id: int, base_url: str = "https://api.github.com") -> None:
    with _request(f"{base_url}/repos/{repo}/actions/caches/{cache_id}", token, method="DELETE"):
        pass


def format_summary(caches: list[dict], reasons: dict[int, str]) -> str:
    by_id = {int(c["id"]): c for c in caches}
    freed = sum(int(by_id.get(cid, {}).get("size_in_bytes") or 0) for cid in reasons)
    lines = [
        f"caches: {len(caches)}  to delete: {len(reasons)}  freeing: {freed / 1048576:.0f} MB"
    ]
    for cid, reason in sorted(reasons.items()):
        cache = by_id.get(cid, {})
        size = int(cache.get("size_in_bytes") or 0) / 1048576
        lines.append(f"  {size:7.0f} MB  {cache.get('key', '?')[:88]}  ({reason})")
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", default=os.environ.get("GITHUB_REPOSITORY", ""), help="owner/name")
    parser.add_argument("--token", default=os.environ.get("GITHUB_TOKEN", ""))
    parser.add_argument("--base-url", default=os.environ.get("GITHUB_API_URL", "https://api.github.com"))
    parser.add_argument("--min-age-hours", type=float, default=0.0)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    if not args.repo or not args.token:
        parser.error("--repo and --token (or GITHUB_REPOSITORY/GITHUB_TOKEN) are required")

    caches = list_caches(args.repo, args.token, args.base_url)
    reasons = plan_deletions(caches, min_age_hours=args.min_age_hours)
    print(format_summary(caches, reasons))
    if args.dry_run or not reasons:
        return 0

    failures = 0
    for cache_id, reason in sorted(reasons.items()):
        try:
            delete_cache(args.repo, args.token, cache_id, args.base_url)
            print(f"deleted {cache_id} ({reason})")
        except HTTPError as error:
            failures += 1
            print(f"failed to delete {cache_id}: {error}", file=sys.stderr)

    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
