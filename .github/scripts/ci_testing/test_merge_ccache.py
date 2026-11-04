#!/usr/bin/env python3

from __future__ import annotations

import os
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPT_DIR))

from merge_ccache import expand_sources, is_ccache_dir, merge_ccache_dirs  # noqa: E402


def _write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def test_is_ccache_dir_requires_hex_bucket(tmp_path: Path) -> None:
    empty = tmp_path / "empty"
    empty.mkdir()
    assert is_ccache_dir(empty) is False
    (empty / "tmp").mkdir()
    assert is_ccache_dir(empty) is False
    (empty / "a").mkdir()
    assert is_ccache_dir(empty) is True


def test_union_copies_unique_and_skips_overlap(tmp_path: Path) -> None:
    src0 = tmp_path / "c0"
    src1 = tmp_path / "c1"
    dest = tmp_path / "out"
    _write(src0 / "a" / "aa" / "core.o", b"core")
    _write(src0 / "a" / "aa" / "lib0.o", b"lib0")
    _write(src1 / "a" / "aa" / "core.o", b"core")
    _write(src1 / "b" / "bb" / "lib1.o", b"lib1")

    used, added, skipped = merge_ccache_dirs(dest, [src0, src1])
    assert used == 2
    assert added == 3
    assert skipped == 1
    assert (dest / "a" / "aa" / "core.o").read_bytes() == b"core"
    assert (dest / "a" / "aa" / "lib0.o").read_bytes() == b"lib0"
    assert (dest / "b" / "bb" / "lib1.o").read_bytes() == b"lib1"


def test_skips_tmp_stats_and_tag(tmp_path: Path) -> None:
    src = tmp_path / "c0"
    dest = tmp_path / "out"
    _write(src / "CACHEDIR.TAG", b"tag")
    _write(src / "stats", b"stats")
    _write(src / "tmp" / "junk", b"tmp")
    _write(src / "0" / "ff" / "hit.o", b"obj")

    used, added, skipped = merge_ccache_dirs(dest, [src])
    assert used == 1
    assert added == 1
    assert skipped == 0
    assert (dest / "0" / "ff" / "hit.o").read_bytes() == b"obj"
    assert not (dest / "CACHEDIR.TAG").exists()
    assert not (dest / "stats").exists()
    assert not (dest / "tmp").exists()


def test_ignores_missing_and_self(tmp_path: Path) -> None:
    dest = tmp_path / "out"
    _write(dest / "c" / "cc" / "keep.o", b"keep")
    used, added, skipped = merge_ccache_dirs(
        dest, [tmp_path / "missing", dest, tmp_path / "not-a-dir"]
    )
    assert used == 0
    assert added == 0
    assert skipped == 0
    assert (dest / "c" / "cc" / "keep.o").read_bytes() == b"keep"


def test_existing_dest_file_wins(tmp_path: Path) -> None:
    src = tmp_path / "c0"
    dest = tmp_path / "out"
    _write(dest / "a" / "aa" / "core.o", b"first")
    _write(src / "a" / "aa" / "core.o", b"second")
    used, added, skipped = merge_ccache_dirs(dest, [src])
    assert used == 1
    assert added == 0
    assert skipped == 1
    assert (dest / "a" / "aa" / "core.o").read_bytes() == b"first"


def test_expand_does_not_treat_buckets_as_caches(tmp_path: Path) -> None:
    src = tmp_path / "c0"
    _write(src / "0" / "f" / "x.o", b"x")
    found = expand_sources([src])
    assert found == [src.resolve()]


def test_expand_nested_dot_ccache_and_artifact_layout(tmp_path: Path) -> None:
    nested = tmp_path / "job" / ".ccache"
    artifact = tmp_path / "ccache_chunk_3"
    _write(nested / "a" / "aa" / "x.o", b"x")
    _write(artifact / "b" / "bb" / "y.o", b"y")
    found = expand_sources([tmp_path / "job", artifact, tmp_path / "missing"])
    names = {path.name for path in found}
    assert names == {".ccache", "ccache_chunk_3"}


def test_merge_from_artifact_parent(tmp_path: Path) -> None:
    chunks = tmp_path / "ccache_chunks"
    src = chunks / "ccache_chunk_0"
    dest = tmp_path / "out"
    _write(src / "e" / "ee" / "z.o", b"z")
    used, added, skipped = merge_ccache_dirs(dest, [chunks])
    assert used == 1
    assert added == 1
    assert skipped == 0
    assert (dest / "e" / "ee" / "z.o").read_bytes() == b"z"


def test_skips_symlinks(tmp_path: Path) -> None:
    src = tmp_path / "c0"
    dest = tmp_path / "out"
    target = tmp_path / "outside"
    target.write_bytes(b"secret")
    bucket = src / "d"
    bucket.mkdir(parents=True)
    os.symlink(target, bucket / "link.o")
    _write(bucket / "real.o", b"ok")
    used, added, skipped = merge_ccache_dirs(dest, [src])
    assert used == 1
    assert added == 1
    assert skipped == 0
    assert (dest / "d" / "real.o").read_bytes() == b"ok"
    assert not (dest / "d" / "link.o").exists()
