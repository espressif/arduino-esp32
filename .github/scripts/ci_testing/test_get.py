#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import importlib.util
import io
import os
import sys
import tarfile
import threading
import time
from pathlib import Path

import pytest

GET_PY = Path(__file__).resolve().parents[3] / "tools" / "get.py"
SPEC = importlib.util.spec_from_file_location("esp32_get", GET_PY)
get = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(get)


def _write_checksum(tool_dir: Path, checksum: str) -> None:
    tool_dir.mkdir(parents=True, exist_ok=True)
    (tool_dir / ".package_checksum").write_text(checksum)


def _cache_hit(tmp_path: Path, checksum: str = "abc123") -> str:
    _write_checksum(tmp_path / "xtensa-esp-elf", checksum)
    return str(tmp_path)


def test_checksum_match_still_verifies_without_skip_flag(tmp_path: Path) -> None:
    dest = _cache_hit(tmp_path)
    assert get.should_skip_install_verification(dest, "abc123", False, False) is True
    skip_verify = False
    assert (skip_verify and get.should_skip_install_verification(dest, "abc123", False, False)) is False


def test_new_package_not_skipped_even_with_skip_flag(tmp_path: Path) -> None:
    skip_verify = True
    assert get.should_skip_install_verification(str(tmp_path), "abc123", False, False) is False
    assert not (
        skip_verify and get.should_skip_install_verification(str(tmp_path), "abc123", False, False)
    )


def test_skip_flag_allows_cache_hit_fast_path(tmp_path: Path) -> None:
    dest = _cache_hit(tmp_path)
    skip_verify = True
    assert skip_verify and get.should_skip_install_verification(dest, "abc123", False, False)
    assert get.installed_tool_dir(dest, "abc123") == "xtensa-esp-elf"


def test_skip_without_dist_archive(tmp_path: Path) -> None:
    dest = _cache_hit(tmp_path)
    missing = tmp_path / "dist" / "tool.tar.gz"
    assert not missing.exists()
    assert get.should_skip_install_verification(dest, "abc123", False, False) is True


def test_no_skip_on_checksum_mismatch(tmp_path: Path) -> None:
    dest = _cache_hit(tmp_path, "old")
    assert get.should_skip_install_verification(dest, "new", False, False) is False


def test_no_skip_when_forced(tmp_path: Path) -> None:
    dest = _cache_hit(tmp_path)
    assert get.should_skip_install_verification(dest, "abc123", True, False) is False
    assert get.should_skip_install_verification(dest, "abc123", False, True) is False


def test_installed_tool_dir_ignores_other_files(tmp_path: Path) -> None:
    (tmp_path / "dist").mkdir()
    (tmp_path / "get.py").write_text("x")
    assert get.installed_tool_dir(str(tmp_path), "abc123") is None


def test_resolve_job_count_caps_at_tool_count() -> None:
    assert get.resolve_job_count(0) == 1
    assert get.resolve_job_count(3, 8) == 3
    assert get.resolve_job_count(9, 4) == 4
    assert get.resolve_job_count(9, None) >= 1


def test_install_tools_test_mode_lists_archives(capsys) -> None:
    tools = [{"archiveFileName": "a.tar.gz"}, {"archiveFileName": "b.zip"}]
    failed = get.install_tools(tools, False, False, False, 4, is_test=True)
    assert failed == []
    out = capsys.readouterr().out
    assert "Would install: a.tar.gz" in out
    assert "Would install: b.zip" in out


def _make_tool_tarball(home: Path, name: str, payload: bytes) -> dict:
    root = f"{name}-9.0.0"
    src = home / "build" / root
    src.mkdir(parents=True)
    (src / "data.txt").write_bytes(payload)
    dist = home / "dist"
    dist.mkdir(exist_ok=True)
    archive_name = f"{root}.tar.gz"
    archive = dist / archive_name
    with tarfile.open(archive, "w:gz") as tar:
        tar.add(src, arcname=root)
    checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
    return {
        "archiveFileName": archive_name,
        "checksum": "SHA-256:" + checksum,
        "url": "http://127.0.0.1:1/" + archive_name,
    }


@pytest.fixture
def tools_home(tmp_path, monkeypatch):
    dist = tmp_path / "dist"
    dist.mkdir()
    monkeypatch.setattr(get, "dist_dir", str(dist) + "/")
    monkeypatch.setattr(get, "verbose", False)
    monkeypatch.chdir(tmp_path)
    return tmp_path


def _extracted_payloads(home: Path) -> dict[str, bytes]:
    found = {}
    for child in home.iterdir():
        data = child / "data.txt"
        if child.is_dir() and data.is_file():
            found[child.name] = data.read_bytes()
    return found


def test_parallel_extracts_distinct_tools(tools_home: Path) -> None:
    tools = [_make_tool_tarball(tools_home, f"tool{c}", f"body-{c}".encode()) for c in "abcdef"]
    failed = get.install_tools(tools, False, False, False, 4)
    assert failed == []
    payloads = _extracted_payloads(tools_home)
    assert payloads == {f"tool{c}": f"body-{c}".encode() for c in "abcdef"}
    for name, tool in zip("abcdef", tools):
        checksum = (tools_home / f"tool{name}" / ".package_checksum").read_text()
        assert checksum == tool["checksum"][8:]


def test_parallel_status_lines_are_tagged_with_archive_name(tools_home: Path, capsys) -> None:
    tools = [_make_tool_tarball(tools_home, f"tool{c}", f"body-{c}".encode()) for c in "abc"]
    assert get.install_tools(tools, False, False, False, 3) == []
    out = capsys.readouterr().out
    for tool in tools:
        name = tool["archiveFileName"]
        assert "[{0}] Already downloaded".format(name) in out
        assert "[{0}] Verify archive".format(name) in out
        assert "[{0}] Extracting archive".format(name) in out
        assert "[{0}] Files extracted successfully".format(name) in out
    for line in out.splitlines():
        if "Verify archive" in line or "Extracting archive" in line or "Files extracted successfully" in line:
            assert line.startswith("[")


def test_log_prefixes_when_tool_is_set(capsys) -> None:
    get._tls.tool = "foo.tar.gz"
    try:
        get.log("Verify archive")
    finally:
        get._tls.tool = None
    assert capsys.readouterr().out == "[foo.tar.gz] Verify archive\n"


def test_parallel_and_serial_extract_the_same_files(tmp_path, monkeypatch) -> None:
    def run(dest: Path, jobs: int) -> dict[str, bytes]:
        dest.mkdir()
        (dest / "dist").mkdir()
        monkeypatch.setattr(get, "dist_dir", str(dest / "dist") + "/")
        monkeypatch.setattr(get, "verbose", False)
        monkeypatch.chdir(dest)
        tools = [_make_tool_tarball(dest, f"tool{c}", f"body-{c}".encode()) for c in "abcd"]
        assert get.install_tools(tools, False, False, False, jobs) == []
        return _extracted_payloads(dest)

    parallel = run(tmp_path / "par", 4)
    serial = run(tmp_path / "ser", 1)
    assert parallel == serial
    assert set(parallel) == {"toola", "toolb", "toolc", "toold"}


def test_parallel_workers_overlap(tools_home: Path, monkeypatch) -> None:
    tools = [_make_tool_tarball(tools_home, f"tool{c}", f"body-{c}".encode()) for c in "abcdefgh"]
    lock = threading.Lock()
    active = 0
    max_active = 0
    orig = get.get_tool

    def counting_get_tool(*args, **kwargs):
        nonlocal active, max_active
        with lock:
            active += 1
            max_active = max(max_active, active)
        try:
            time.sleep(0.05)
            return orig(*args, **kwargs)
        finally:
            with lock:
                active -= 1

    monkeypatch.setattr(get, "get_tool", counting_get_tool)
    assert get.install_tools(tools, False, False, False, 4) == []
    assert max_active > 1
    assert max_active <= 4
    assert _extracted_payloads(tools_home) == {f"tool{c}": f"body-{c}".encode() for c in "abcdefgh"}


def test_parallel_restores_stdout(tools_home: Path) -> None:
    tools = [_make_tool_tarball(tools_home, "toolx", b"x")]
    before = sys.stdout
    assert get.install_tools(tools, False, False, False, 2) == []
    assert sys.stdout is before


def test_parallel_restores_stdout_after_worker_exception(monkeypatch) -> None:
    def boom(tool, *args, **kwargs):
        raise RuntimeError("worker boom")

    monkeypatch.setattr(get, "get_tool", boom)
    before = sys.stdout
    tools = [{"archiveFileName": "a.tar.gz", "checksum": "SHA-256:" + "a" * 64, "url": "http://127.0.0.1:1/a"}]
    with pytest.raises(RuntimeError, match="worker boom"):
        get.install_tools(tools, False, False, False, 2)
    assert sys.stdout is before


def test_parallel_mixed_failure_does_not_block_successes(monkeypatch) -> None:
    def fake_get_tool(tool, force_download, force_extract, skip_verify=False):
        time.sleep(0.02)
        return not tool["archiveFileName"].startswith("bad")

    monkeypatch.setattr(get, "get_tool", fake_get_tool)
    tools = [
        {"archiveFileName": "good-a.tar.gz"},
        {"archiveFileName": "bad-b.tar.gz"},
        {"archiveFileName": "good-c.tar.gz"},
        {"archiveFileName": "bad-d.tar.gz"},
    ]
    failed = get.install_tools(tools, False, False, False, 4)
    assert sorted(failed) == ["bad-b.tar.gz", "bad-d.tar.gz"]


def test_parallel_retry_succeeds_on_force_download(monkeypatch) -> None:
    attempts = {}
    lock = threading.Lock()

    def flaky_get_tool(tool, force_download, force_extract, skip_verify=False):
        name = tool["archiveFileName"]
        with lock:
            attempts[name] = attempts.get(name, 0) + 1
        time.sleep(0.02)
        if not force_download:
            return False
        return True

    monkeypatch.setattr(get, "get_tool", flaky_get_tool)
    tools = [{"archiveFileName": f"tool{i}.tar.gz"} for i in range(6)]
    assert get.install_tools(tools, False, False, False, 3) == []
    assert all(count == 2 for count in attempts.values())


def test_parallel_skip_verify_only_for_already_installed(tools_home: Path) -> None:
    installed = _make_tool_tarball(tools_home, "toolold", b"old")
    (tools_home / "toolold").mkdir()
    (tools_home / "toolold" / ".package_checksum").write_text(installed["checksum"][8:])
    (tools_home / "toolold" / "data.txt").write_bytes(b"cached-old")
    fresh = _make_tool_tarball(tools_home, "toolnew", b"new")
    failed = get.install_tools([installed, fresh], False, False, True, 2)
    assert failed == []
    assert (tools_home / "toolold" / "data.txt").read_bytes() == b"cached-old"
    assert (tools_home / "toolnew" / "data.txt").read_bytes() == b"new"


def test_skip_verify_without_dist_does_not_redownload(tools_home: Path) -> None:
    installed = _make_tool_tarball(tools_home, "toolold", b"old")
    (tools_home / "toolold").mkdir()
    (tools_home / "toolold" / ".package_checksum").write_text(installed["checksum"][8:])
    (tools_home / "toolold" / "data.txt").write_bytes(b"cached-old")
    (tools_home / "dist" / installed["archiveFileName"]).unlink()
    fresh = _make_tool_tarball(tools_home, "toolnew", b"new")
    failed = get.install_tools([installed, fresh], False, False, True, 2)
    assert failed == []
    assert (tools_home / "toolold" / "data.txt").read_bytes() == b"cached-old"
    assert not (tools_home / "dist" / installed["archiveFileName"]).exists()
    assert (tools_home / "toolnew" / "data.txt").read_bytes() == b"new"


def test_locked_stdio_does_not_interleave_lines() -> None:
    buf = io.StringIO()
    locked = get._LockedStdio(buf)

    def writer(token: str) -> None:
        line = token * 32 + "\n"
        for _ in range(200):
            locked.write(line)

    threads = [threading.Thread(target=writer, args=(ch,)) for ch in "ABC"]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()
    for line in buf.getvalue().splitlines():
        assert len(set(line)) == 1
        assert len(line) == 32


def test_locked_stdio_isatty_is_false() -> None:
    locked = get._LockedStdio(io.StringIO())
    assert locked.isatty() is False

