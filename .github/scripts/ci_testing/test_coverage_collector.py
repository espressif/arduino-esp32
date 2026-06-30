#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""
Tests for coverage_collector.py

Usage:
    python3 -m pytest .github/scripts/ci_testing/test_coverage_collector.py -v
"""

import base64
import json
import os
import subprocess
import sys
from pathlib import Path
from unittest import mock

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from coverage_collector import (
    _demangle_names,
    _match_build_dir,
    extract_gcov_files,
    find_gcno,
    generate_uncovered_report,
    parse_lcov_summary,
    parse_tracefile_functions,
    place_gcda_files,
    run_lcov,
    strip_ansi,
)

FIXTURE_DIR = Path(__file__).resolve().parent.parent.parent.parent / "tests" / "validation" / "unity" / "esp32"
COVERAGE_LOG = FIXTURE_DIR / "unity_coverage.log"


# ---------------------------------------------------------------------------
# strip_ansi
# ---------------------------------------------------------------------------

class TestStripAnsi:
    def test_removes_color_codes(self):
        assert strip_ansi("\x1b[32mPASS\x1b[0m") == "PASS"

    def test_removes_multiple_codes(self):
        assert strip_ansi("\x1b[1;31mERROR\x1b[0m: \x1b[33mwarning\x1b[0m") == "ERROR: warning"

    def test_passthrough_plain_text(self):
        assert strip_ansi("hello world") == "hello world"

    def test_empty_string(self):
        assert strip_ansi("") == ""


# ---------------------------------------------------------------------------
# extract_gcov_files
# ---------------------------------------------------------------------------

class TestExtractGcovFiles:
    def test_single_file_roundtrip(self):
        payload = b"\x01\x02\x03\x04\x05"
        b64 = base64.b64encode(payload).decode()
        log = (
            "<<<GCOV_FILE:/tmp/test.gcda>>>\n"
            f"{b64}\n"
            "<<<GCOV_END>>>\n"
        )
        files = extract_gcov_files(log)
        assert len(files) == 1
        assert files["/tmp/test.gcda"] == payload

    def test_multiple_files(self):
        p1, p2 = b"data1", b"data2"
        log = (
            "<<<GCOV_FILE:/a.gcda>>>\n"
            f"{base64.b64encode(p1).decode()}\n"
            "<<<GCOV_END>>>\n"
            "<<<GCOV_FILE:/b.gcda>>>\n"
            f"{base64.b64encode(p2).decode()}\n"
            "<<<GCOV_END>>>\n"
        )
        files = extract_gcov_files(log)
        assert len(files) == 2
        assert files["/a.gcda"] == p1
        assert files["/b.gcda"] == p2

    def test_strips_ansi_in_markers(self):
        payload = b"\xff"
        b64 = base64.b64encode(payload).decode()
        log = (
            "\x1b[32m<<<GCOV_FILE:/ansi.gcda>>>\x1b[0m\n"
            f"{b64}\n"
            "\x1b[32m<<<GCOV_END>>>\x1b[0m\n"
        )
        files = extract_gcov_files(log)
        assert "/ansi.gcda" in files
        assert files["/ansi.gcda"] == payload

    def test_multiline_independently_encoded(self):
        """Each serial line is encoded independently by base64_encode_chars."""
        part1 = b"first chunk data!!"
        part2 = b"second chunk data!"
        log = (
            "<<<GCOV_FILE:/multi.gcda>>>\n"
            f"{base64.b64encode(part1).decode()}\n"
            f"{base64.b64encode(part2).decode()}\n"
            "<<<GCOV_END>>>\n"
        )
        files = extract_gcov_files(log)
        assert files["/multi.gcda"] == part1 + part2

    def test_chunked_base64_with_intermediate_padding(self):
        """libb64 may emit '=' padding at the end of each chunk."""
        chunk1 = b"\x01\x02\x03\x04\x05"  # 5 bytes -> padded b64
        chunk2 = b"\x06\x07\x08\x09\x0a\x0b"  # 6 bytes -> no padding
        chunk3 = b"\x0c\x0d"  # 2 bytes -> padded b64
        b1 = base64.b64encode(chunk1).decode()  # "AQIDBAU="
        b2 = base64.b64encode(chunk2).decode()  # "BgcICQoL"
        b3 = base64.b64encode(chunk3).decode()  # "DA0="
        assert "=" in b1 and "=" in b3
        log = (
            "<<<GCOV_FILE:/padded.gcda>>>\n"
            f"{b1}\n{b2}\n{b3}\n"
            "<<<GCOV_END>>>\n"
        )
        files = extract_gcov_files(log)
        assert files["/padded.gcda"] == chunk1 + chunk2 + chunk3

    def test_noise_lines_skipped_valid_lines_kept(self):
        """Noise lines that fail b64 decode are skipped; valid lines are kept."""
        payload = b"clean data here"
        b64 = base64.b64encode(payload).decode()
        log = (
            "<<<GCOV_FILE:/noisy.gcda>>>\n"
            "X\n"  # 1 char -> invalid b64 length -> skipped
            f"{b64}\n"
            "<<<GCOV_END>>>\n"
        )
        files = extract_gcov_files(log)
        assert files["/noisy.gcda"] == payload

    def test_empty_base64_skipped(self):
        log = (
            "<<<GCOV_FILE:/empty.gcda>>>\n"
            "<<<GCOV_END>>>\n"
        )
        files = extract_gcov_files(log)
        assert len(files) == 0

    def test_no_markers_returns_empty(self):
        files = extract_gcov_files("just some random test output\nnothing here\n")
        assert files == {}

    def test_unclosed_file_ignored(self):
        log = (
            "<<<GCOV_FILE:/unclosed.gcda>>>\n"
            f"{base64.b64encode(b'data').decode()}\n"
        )
        files = extract_gcov_files(log)
        assert len(files) == 0

    def test_real_log_fixture(self):
        if not COVERAGE_LOG.exists():
            pytest.skip("unity_coverage.log fixture not found")
        log_text = COVERAGE_LOG.read_text(errors="replace")
        files = extract_gcov_files(log_text)
        assert len(files) > 0
        for path, data in files.items():
            assert path.endswith(".gcda"), f"Expected .gcda path, got {path}"
            assert len(data) > 128, f"Suspiciously small data for {path}: {len(data)} bytes"
            assert data[:4] == b"adcg", f"Bad gcda magic for {path}"

    def test_all_lines_invalid_base64_skips_file(self):
        """When every b64 line fails to decode, the file is not emitted."""
        log = (
            "<<<GCOV_FILE:/bad.gcda>>>\n"
            "X\n"  # 1 char after cleaning -> 1 mod 4 -> invalid
            "<<<GCOV_END>>>\n"
        )
        files = extract_gcov_files(log)
        assert "/bad.gcda" not in files

    def test_duplicate_paths_concatenated(self):
        """Multiple dump sessions with the same .gcda path concatenate bytes."""
        boot1 = b"boot1_data_record"
        boot2 = b"boot2_data_record"
        log = (
            "<<<GCOV_FILE:/build/sketch.cpp.gcda>>>\n"
            f"{base64.b64encode(boot1).decode()}\n"
            "<<<GCOV_END>>>\n"
            "<<<GCOV_FILE:/build/sketch.cpp.gcda>>>\n"
            f"{base64.b64encode(boot2).decode()}\n"
            "<<<GCOV_END>>>\n"
        )
        files = extract_gcov_files(log)
        assert len(files) == 1
        assert files["/build/sketch.cpp.gcda"] == boot1 + boot2

    def test_duplicate_and_unique_paths_mixed(self):
        """A mix of duplicate and unique paths: duplicates merge, unique stays."""
        d1 = b"dup_part1"
        d2 = b"dup_part2"
        u = b"unique_data"
        log = (
            "<<<GCOV_FILE:/dup.gcda>>>\n"
            f"{base64.b64encode(d1).decode()}\n"
            "<<<GCOV_END>>>\n"
            "<<<GCOV_FILE:/uniq.gcda>>>\n"
            f"{base64.b64encode(u).decode()}\n"
            "<<<GCOV_END>>>\n"
            "<<<GCOV_FILE:/dup.gcda>>>\n"
            f"{base64.b64encode(d2).decode()}\n"
            "<<<GCOV_END>>>\n"
        )
        files = extract_gcov_files(log)
        assert len(files) == 2
        assert files["/dup.gcda"] == d1 + d2
        assert files["/uniq.gcda"] == u


# ---------------------------------------------------------------------------
# find_gcno
# ---------------------------------------------------------------------------

class TestFindGcno:
    def test_finds_matching_gcno(self, tmp_path):
        gcno = tmp_path / "build" / "gcov" / "Esp.cpp.gcno"
        gcno.parent.mkdir(parents=True)
        gcno.write_bytes(b"\x00")
        result = find_gcno("Esp.cpp.gcda", [str(tmp_path / "build")])
        assert result == str(gcno)

    def test_returns_none_when_missing(self, tmp_path):
        result = find_gcno("Missing.cpp.gcda", [str(tmp_path)])
        assert result is None

    def test_searches_multiple_dirs(self, tmp_path):
        dir1 = tmp_path / "dir1"
        dir2 = tmp_path / "dir2" / "sub"
        dir2.mkdir(parents=True)
        dir1.mkdir()
        gcno = dir2 / "Stream.cpp.gcno"
        gcno.write_bytes(b"\x00")
        result = find_gcno("Stream.cpp.gcda", [str(dir1), str(tmp_path / "dir2")])
        assert result == str(gcno)

    def test_empty_build_dirs(self):
        assert find_gcno("anything.gcda", []) is None


# ---------------------------------------------------------------------------
# _match_build_dir
# ---------------------------------------------------------------------------

class TestMatchBuildDir:
    def test_matches_path_containing_build_dir(self, tmp_path):
        bd = tmp_path / "build0.tmp"
        bd.mkdir()
        gcda_path = str(bd / "objects" / "sketch.cpp.gcda")
        assert _match_build_dir(gcda_path, [str(bd)]) == str(bd)

    def test_returns_none_for_external_path(self, tmp_path):
        bd = tmp_path / "build0.tmp"
        bd.mkdir()
        assert _match_build_dir("/tmp/acmake_cache/core/Esp.cpp.gcda", [str(bd)]) is None

    def test_selects_correct_dir_among_multiple(self, tmp_path):
        bd0 = tmp_path / "build0.tmp"
        bd1 = tmp_path / "build1.tmp"
        bd0.mkdir()
        bd1.mkdir()
        gcda0 = str(bd0 / "objects" / "sketch.cpp.gcda")
        gcda1 = str(bd1 / "objects" / "sketch.cpp.gcda")
        assert _match_build_dir(gcda0, [str(bd0), str(bd1)]) == str(bd0)
        assert _match_build_dir(gcda1, [str(bd0), str(bd1)]) == str(bd1)


# ---------------------------------------------------------------------------
# place_gcda_files
# ---------------------------------------------------------------------------

class TestPlaceGcdaFiles:
    def test_places_gcda_next_to_gcno(self, tmp_path):
        gcno = tmp_path / "gcov" / "Esp.cpp.gcno"
        gcno.parent.mkdir(parents=True)
        gcno.write_bytes(b"gcno content")
        gcov_files = {"/some/path/Esp.cpp.gcda": b"gcda content"}
        written = place_gcda_files(gcov_files, [str(tmp_path)])
        assert len(written) == 1
        gcda = tmp_path / "gcov" / "Esp.cpp.gcda"
        assert gcda.exists()
        assert gcda.read_bytes() == b"gcda content"

    def test_skips_when_no_gcno_match(self, tmp_path, capsys):
        gcov_files = {"/missing/NoMatch.cpp.gcda": b"data"}
        written = place_gcda_files(gcov_files, [str(tmp_path)])
        assert len(written) == 0
        captured = capsys.readouterr()
        assert "WARNING" in captured.err

    def test_multiple_files(self, tmp_path):
        for name in ("A.cpp.gcno", "B.cpp.gcno"):
            f = tmp_path / name
            f.write_bytes(b"\x00")
        gcov_files = {
            "/x/A.cpp.gcda": b"A",
            "/y/B.cpp.gcda": b"B",
        }
        written = place_gcda_files(gcov_files, [str(tmp_path)])
        assert len(written) == 2
        assert (tmp_path / "A.cpp.gcda").read_bytes() == b"A"
        assert (tmp_path / "B.cpp.gcda").read_bytes() == b"B"

    def test_multi_config_same_basename_no_overwrite(self, tmp_path):
        """Two configs with same .gcno basenames get separate .gcda files."""
        bd0 = tmp_path / "build0.tmp"
        bd1 = tmp_path / "build1.tmp"
        for bd in (bd0, bd1):
            gcno = bd / "gcov" / "core" / "main.cpp.gcno"
            gcno.parent.mkdir(parents=True)
            gcno.write_bytes(b"\x00")
            sketch_gcno = bd / "gcov" / "sketch" / "sketch.cpp.gcno"
            sketch_gcno.parent.mkdir(parents=True)
            sketch_gcno.write_bytes(b"\x00")

        gcov_files = {
            str(bd0 / "objects/sketch.cpp.gcda"): b"sketch0",
            str(bd1 / "objects/sketch.cpp.gcda"): b"sketch1",
            "/cache/hash0/core/main.cpp.gcda": b"core0",
            "/cache/hash1/core/main.cpp.gcda": b"core1",
        }
        written = place_gcda_files(gcov_files, [str(bd0), str(bd1)])
        assert len(written) == 4

        s0 = bd0 / "gcov" / "sketch" / "sketch.cpp.gcda"
        s1 = bd1 / "gcov" / "sketch" / "sketch.cpp.gcda"
        assert s0.read_bytes() == b"sketch0"
        assert s1.read_bytes() == b"sketch1"

        c0 = bd0 / "gcov" / "core" / "main.cpp.gcda"
        c1 = bd1 / "gcov" / "core" / "main.cpp.gcda"
        assert c0.exists() and c1.exists()
        assert {c0.read_bytes(), c1.read_bytes()} == {b"core0", b"core1"}


# ---------------------------------------------------------------------------
# run_lcov
# ---------------------------------------------------------------------------

class TestRunLcov:
    def test_single_dir_success(self, tmp_path):
        bd = tmp_path / "build"
        bd.mkdir()
        tracefile = str(tmp_path / "out.info")
        part0 = f"{tracefile}.part0"

        def fake_run(cmd, **kwargs):
            Path(part0).write_text("TN:\nSF:/src/test.c\nend_of_record\n")
            return mock.Mock(returncode=0, stderr="")

        with mock.patch("coverage_collector.subprocess.run", side_effect=fake_run):
            ok = run_lcov([str(bd)], "xtensa-esp-elf-gcov", tracefile)
        assert ok
        assert Path(tracefile).exists()

    def test_capture_failure_returns_false(self, tmp_path):
        bd = tmp_path / "build"
        bd.mkdir()
        tracefile = str(tmp_path / "out.info")

        def fake_run(cmd, **kwargs):
            return mock.Mock(returncode=1, stderr="lcov error")

        with mock.patch("coverage_collector.subprocess.run", side_effect=fake_run):
            ok = run_lcov([str(bd)], None, tracefile)
        assert not ok

    def test_multiple_dirs_merge(self, tmp_path):
        bd1 = tmp_path / "b1"
        bd2 = tmp_path / "b2"
        bd1.mkdir()
        bd2.mkdir()
        tracefile = str(tmp_path / "merged.info")

        call_count = [0]

        def fake_run(cmd, **kwargs):
            nonlocal call_count
            if "--capture" in cmd:
                idx = call_count[0]
                call_count[0] += 1
                part = f"{tracefile}.part{idx}"
                Path(part).write_text(f"SF:test{idx}.c\nend_of_record\n")
                return mock.Mock(returncode=0, stderr="")
            elif "--add-tracefile" in cmd:
                Path(tracefile).write_text("merged\n")
                return mock.Mock(returncode=0)
            return mock.Mock(returncode=1, stderr="unexpected")

        with mock.patch("coverage_collector.subprocess.run", side_effect=fake_run):
            ok = run_lcov([str(bd1), str(bd2)], None, tracefile)
        assert ok

    def test_gcov_tool_passed_to_cmd(self, tmp_path):
        bd = tmp_path / "build"
        bd.mkdir()
        tracefile = str(tmp_path / "out.info")
        captured_cmds = []

        def fake_run(cmd, **kwargs):
            captured_cmds.append(cmd)
            Path(f"{tracefile}.part0").write_text("data\n")
            return mock.Mock(returncode=0, stderr="")

        with mock.patch("coverage_collector.subprocess.run", side_effect=fake_run):
            run_lcov([str(bd)], "my-gcov-tool", tracefile)

        assert any("--gcov-tool" in cmd and "my-gcov-tool" in cmd for cmd in captured_cmds)

    def test_no_gcov_tool_omitted(self, tmp_path):
        bd = tmp_path / "build"
        bd.mkdir()
        tracefile = str(tmp_path / "out.info")
        captured_cmds = []

        def fake_run(cmd, **kwargs):
            captured_cmds.append(cmd)
            Path(f"{tracefile}.part0").write_text("data\n")
            return mock.Mock(returncode=0, stderr="")

        with mock.patch("coverage_collector.subprocess.run", side_effect=fake_run):
            run_lcov([str(bd)], None, tracefile)

        assert all("--gcov-tool" not in cmd for cmd in captured_cmds)


# ---------------------------------------------------------------------------
# parse_lcov_summary
# ---------------------------------------------------------------------------

class TestParseLcovSummary:
    def test_parses_typical_output(self, tmp_path):
        tracefile = str(tmp_path / "cov.info")
        fake_output = (
            "Reading tracefile cov.info\n"
            "Summary coverage rate:\n"
            "  lines......: 45.2% (1234 of 2731 lines)\n"
            "  functions..: 30.0% (50 of 167 functions)\n"
        )

        def fake_run(cmd, **kwargs):
            return mock.Mock(returncode=0, stdout=fake_output, stderr="")

        with mock.patch("coverage_collector.subprocess.run", side_effect=fake_run):
            covered, total = parse_lcov_summary(tracefile)
        assert covered == 1234
        assert total == 2731

    def test_output_on_stderr(self, tmp_path):
        tracefile = str(tmp_path / "cov.info")
        fake_stderr = "  lines......: 80.0% (400 of 500 lines)\n"

        def fake_run(cmd, **kwargs):
            return mock.Mock(returncode=0, stdout="", stderr=fake_stderr)

        with mock.patch("coverage_collector.subprocess.run", side_effect=fake_run):
            covered, total = parse_lcov_summary(tracefile)
        assert covered == 400
        assert total == 500

    def test_no_match_returns_zeros(self, tmp_path):
        tracefile = str(tmp_path / "cov.info")

        def fake_run(cmd, **kwargs):
            return mock.Mock(returncode=0, stdout="no coverage data\n", stderr="")

        with mock.patch("coverage_collector.subprocess.run", side_effect=fake_run):
            covered, total = parse_lcov_summary(tracefile)
        assert covered == 0
        assert total == 0


# ---------------------------------------------------------------------------
# main() integration (mocking subprocess)
# ---------------------------------------------------------------------------

class TestMainIntegration:
    def test_no_gcov_data_produces_zero_json(self, tmp_path):
        log_file = tmp_path / "empty.log"
        log_file.write_text("no coverage data here\n")
        output = tmp_path / "coverage.json"

        sys.argv = [
            "coverage_collector.py",
            "--log", str(log_file),
            "--build-dir", str(tmp_path),
            "--output", str(output),
        ]

        from coverage_collector import main
        main()

        data = json.loads(output.read_text())
        assert data["lines_total"] == 0
        assert data["lines_covered"] == 0
        assert data["coverage_pct"] == 0.0

    def test_log_directory_scan(self, tmp_path):
        log_dir = tmp_path / "logs"
        log_dir.mkdir()

        payload = b"testdata"
        b64 = base64.b64encode(payload).decode()
        (log_dir / "test1_coverage.log").write_text(
            f"<<<GCOV_FILE:/x/A.cpp.gcda>>>\n{b64}\n<<<GCOV_END>>>\n"
        )
        (log_dir / "test2_coverage.log").write_text(
            f"<<<GCOV_FILE:/x/B.cpp.gcda>>>\n{b64}\n<<<GCOV_END>>>\n"
        )

        build = tmp_path / "build"
        build.mkdir()
        (build / "A.cpp.gcno").write_bytes(b"\x00")
        (build / "B.cpp.gcno").write_bytes(b"\x00")

        output = tmp_path / "coverage.json"
        tracefile = tmp_path / "coverage.info"

        fake_summary = "  lines......: 50.0% (10 of 20 lines)\n"

        def fake_run(cmd, **kwargs):
            if "--capture" in cmd:
                for i, c in enumerate(cmd):
                    if c == "--output-file":
                        Path(cmd[i + 1]).write_text("TN:\nend_of_record\n")
                        break
                return mock.Mock(returncode=0, stderr="")
            if "--summary" in cmd:
                return mock.Mock(returncode=0, stdout=fake_summary, stderr="")
            return mock.Mock(returncode=0, stdout="", stderr="")

        sys.argv = [
            "coverage_collector.py",
            "--log", str(log_dir),
            "--build-dir", str(build),
            "--output", str(output),
            "--tracefile", str(tracefile),
        ]

        with mock.patch("coverage_collector.subprocess.run", side_effect=fake_run):
            from coverage_collector import main
            main()

        data = json.loads(output.read_text())
        assert data["lines_covered"] == 10
        assert data["lines_total"] == 20
        assert data["coverage_pct"] == 50.0


# ---------------------------------------------------------------------------
# parse_tracefile_functions
# ---------------------------------------------------------------------------

SAMPLE_TRACEFILE = (
    "TN:\n"
    "SF:/src/Esp.cpp\n"
    "FNL:0,10,20\n"
    "FNA:0,0,_ZN8EspClass9deepSleepEy\n"
    "FNL:1,30,40\n"
    "FNA:1,5,_ZN8EspClass7restartEv\n"
    "end_of_record\n"
    "TN:\n"
    "SF:/src/main.cpp\n"
    "FNL:0,1,5\n"
    "FNA:0,3,main\n"
    "FNL:1,10,15\n"
    "FNA:1,0,unusedHelper\n"
    "end_of_record\n"
)


class TestParseTracefileFunctions:
    def test_parses_functions(self, tmp_path):
        tf = tmp_path / "cov.info"
        tf.write_text(SAMPLE_TRACEFILE)
        result = parse_tracefile_functions(str(tf))
        assert "/src/Esp.cpp" in result
        assert "/src/main.cpp" in result
        assert len(result["/src/Esp.cpp"]) == 2
        assert len(result["/src/main.cpp"]) == 2

    def test_function_attributes(self, tmp_path):
        tf = tmp_path / "cov.info"
        tf.write_text(SAMPLE_TRACEFILE)
        result = parse_tracefile_functions(str(tf))
        funcs = {f["name"]: f for f in result["/src/Esp.cpp"]}
        assert funcs["_ZN8EspClass9deepSleepEy"]["hits"] == 0
        assert funcs["_ZN8EspClass9deepSleepEy"]["line"] == 10
        assert funcs["_ZN8EspClass7restartEv"]["hits"] == 5
        assert funcs["_ZN8EspClass7restartEv"]["line"] == 30

    def test_empty_tracefile(self, tmp_path):
        tf = tmp_path / "empty.info"
        tf.write_text("")
        assert parse_tracefile_functions(str(tf)) == {}


# ---------------------------------------------------------------------------
# _demangle_names
# ---------------------------------------------------------------------------

class TestDemangleNames:
    def test_empty_list(self):
        assert _demangle_names([]) == {}

    def test_fallback_when_tool_missing(self):
        result = _demangle_names(
            ["_ZN8EspClass7restartEv"],
            gcov_tool="/nonexistent/path/to/gcov",
        )
        assert "_ZN8EspClass7restartEv" in result

    def test_uses_system_cxxfilt(self):
        names = ["_ZN8EspClass7restartEv", "main"]
        result = _demangle_names(names)
        assert "main" in result
        assert result["main"] == "main"

    def test_derives_cxxfilt_from_gcov_tool(self):
        captured_cmds = []
        original_run = subprocess.run

        def spy_run(cmd, **kwargs):
            captured_cmds.append(cmd[0])
            return original_run(cmd, **kwargs)

        with mock.patch("coverage_collector.subprocess.run", side_effect=spy_run):
            _demangle_names(["main"], gcov_tool="/usr/bin/some-gcov")
        if captured_cmds:
            assert "c++filt" in captured_cmds[0]


# ---------------------------------------------------------------------------
# generate_uncovered_report
# ---------------------------------------------------------------------------

class TestGenerateUncoveredReport:
    def test_returns_counts(self, tmp_path):
        tf = tmp_path / "cov.info"
        tf.write_text(SAMPLE_TRACEFILE)
        total, untested = generate_uncovered_report(str(tf))
        assert total == 4
        assert untested == 2

    def test_writes_markdown(self, tmp_path):
        tf = tmp_path / "cov.info"
        tf.write_text(SAMPLE_TRACEFILE)
        out = tmp_path / "report.md"
        generate_uncovered_report(str(tf), output_path=str(out))
        content = out.read_text()
        assert "# Uncovered Functions Report" in content
        assert "2 of 4 functions untested" in content
        assert "/src/Esp.cpp" in content
        assert "unusedHelper" in content

    def test_tested_functions_excluded(self, tmp_path):
        tf = tmp_path / "cov.info"
        tf.write_text(SAMPLE_TRACEFILE)
        out = tmp_path / "report.md"
        generate_uncovered_report(str(tf), output_path=str(out))
        content = out.read_text()
        assert "restartEv" not in content
        assert "main" not in content or "main.cpp" in content

    def test_core_root_relativizes_paths(self, tmp_path):
        tf = tmp_path / "cov.info"
        tf.write_text(SAMPLE_TRACEFILE)
        out = tmp_path / "report.md"
        generate_uncovered_report(str(tf), output_path=str(out), core_root="/src")
        content = out.read_text()
        assert "## Esp.cpp" in content or "## ./Esp.cpp" in content
        assert "/src/Esp.cpp" not in content

    def test_empty_tracefile(self, tmp_path):
        tf = tmp_path / "empty.info"
        tf.write_text("")
        out = tmp_path / "report.md"
        total, untested = generate_uncovered_report(str(tf), output_path=str(out))
        assert total == 0
        assert untested == 0
        assert "No function data found" in out.read_text()

    def test_all_functions_tested(self, tmp_path):
        tracefile_text = (
            "TN:\nSF:/src/a.cpp\n"
            "FNL:0,1,5\nFNA:0,3,funcA\n"
            "FNL:1,10,15\nFNA:1,1,funcB\n"
            "end_of_record\n"
        )
        tf = tmp_path / "cov.info"
        tf.write_text(tracefile_text)
        out = tmp_path / "report.md"
        total, untested = generate_uncovered_report(str(tf), output_path=str(out))
        assert total == 2
        assert untested == 0
        content = out.read_text()
        assert "0 of 2 functions untested" in content
        assert "| Line |" not in content

    def test_markdown_table_format(self, tmp_path):
        tf = tmp_path / "cov.info"
        tf.write_text(SAMPLE_TRACEFILE)
        out = tmp_path / "report.md"
        generate_uncovered_report(str(tf), output_path=str(out))
        content = out.read_text()
        assert "| Line | Function |" in content
        assert "|------|----------|" in content
