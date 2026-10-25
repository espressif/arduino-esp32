#!/usr/bin/env python3
"""Pick the Compilation Tests Linux chunk that should save ccache.

Path-sorted slices keep ``libraries/Matter`` in one chunk. That chunk is Linux
Cmax, so it is the only writer. If Matter is not in the sketch list, chunk 0
saves. Chunk math matches ``sketch_utils.sh`` / ``get_affected.py``.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

MATTER_MARKER = "libraries/Matter"


def chunk_size_for(sketch_count: int, chunk_count: int) -> int:
    if chunk_count <= 0:
        return 0
    size = sketch_count // chunk_count
    if chunk_count * size < sketch_count:
        size += 1
    return max(size, 1) if sketch_count else 0


def writer_chunk(
    sketches: list[str],
    chunk_count: int,
    marker: str = MATTER_MARKER,
) -> int:
    if chunk_count <= 0:
        return 0
    ordered = sorted(s.strip().replace("\\", "/") for s in sketches if s.strip())
    n = len(ordered)
    if n == 0:
        return 0
    size = chunk_size_for(n, chunk_count)
    for i, path in enumerate(ordered):
        if marker in path:
            return min(i // size, chunk_count - 1)
    return 0


def load_sketches(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8")
    return [line.strip() for line in text.splitlines() if line.strip()]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sketches_file", type=Path)
    parser.add_argument("chunk_count", type=int)
    args = parser.parse_args(argv)
    if not args.sketches_file.is_file():
        print(f"error: sketches file not found: {args.sketches_file}", file=sys.stderr)
        return 2
    print(writer_chunk(load_sketches(args.sketches_file), args.chunk_count))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
