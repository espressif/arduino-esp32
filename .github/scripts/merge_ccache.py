#!/usr/bin/env python3
"""Union content-addressed ccache directories into one destination.

Only hex bucket trees (``0``-``9``, ``a``-``f``) are copied. Top-level names
such as ``tmp/``, ``stats``, and ``CACHEDIR.TAG`` are skipped so a merge does
not mix per-job counters or in-flight compiles. Existing dest files are left
alone (ccache result paths are content hashes).
"""

from __future__ import annotations

import argparse
import os
import shutil
from collections.abc import Iterable
from pathlib import Path

BUCKETS = frozenset("0123456789abcdef")


def is_ccache_dir(path: Path) -> bool:
    if not path.is_dir():
        return False
    return any(child.is_dir() and child.name in BUCKETS for child in path.iterdir())


def expand_sources(sources: Iterable[Path]) -> list[Path]:
    """Accept a ccache dir, a parent of ``.ccache``, or an artifact folder."""
    found: list[Path] = []
    seen: set[Path] = set()

    def add(path: Path) -> None:
        resolved = path.resolve()
        if resolved in seen or not is_ccache_dir(resolved):
            return
        seen.add(resolved)
        found.append(resolved)

    for raw in sources:
        if not raw.exists():
            continue
        add(raw)
        if is_ccache_dir(raw):
            continue
        add(raw / ".ccache")
        if raw.is_dir():
            for child in raw.iterdir():
                add(child)
                if not is_ccache_dir(child):
                    add(child / ".ccache")
    return found


def _copy_file(src: Path, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    try:
        os.link(src, dest)
    except OSError:
        shutil.copy2(src, dest)


def merge_ccache_dirs(dest: Path, sources: Iterable[Path]) -> tuple[int, int, int]:
    """Copy hex buckets from each source into dest.

    Returns ``(sources_used, added, skipped)``.
    """
    dest = dest.resolve()
    dest.mkdir(parents=True, exist_ok=True)
    used = 0
    added = 0
    skipped = 0
    for src in expand_sources(sources):
        if src == dest:
            continue
        used += 1
        for bucket in src.iterdir():
            if not bucket.is_dir() or bucket.name not in BUCKETS:
                continue
            for dirpath, dirnames, filenames in os.walk(bucket, followlinks=False):
                dirnames[:] = [name for name in dirnames if name not in (".", "..")]
                rel_dir = Path(dirpath).relative_to(src)
                for name in filenames:
                    from_path = Path(dirpath) / name
                    if from_path.is_symlink() or not from_path.is_file():
                        continue
                    to_path = dest / rel_dir / name
                    if to_path.exists():
                        skipped += 1
                        continue
                    _copy_file(from_path, to_path)
                    added += 1
    return used, added, skipped


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dest", type=Path)
    parser.add_argument("sources", nargs="+", type=Path)
    args = parser.parse_args(argv)
    used, added, skipped = merge_ccache_dirs(args.dest, args.sources)
    print(f"merged {used} caches: {added} files added, {skipped} already present")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
