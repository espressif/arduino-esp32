#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""
Tests for the coverage_capture and save_perf_result fixtures in conftest.py.

These tests mock the ``dut`` and ``request`` objects to avoid requiring
real hardware or pytest-embedded.

Usage:
    python3 -m pytest .github/scripts/ci_testing/test_conftest_fixtures.py -v -p no:pytest_embedded
"""

import json
import os
from pathlib import Path
from types import SimpleNamespace
from unittest import mock
import importlib
import importlib.util

import pytest


# ---------------------------------------------------------------------------
# Import fixture functions, stripping the @pytest.fixture marker so they
# can be called as plain functions from our tests.
# ---------------------------------------------------------------------------

_conftest_path = Path(__file__).resolve().parent.parent.parent.parent / "tests" / "conftest.py"

_orig_fixture = pytest.fixture

def _passthrough_fixture(*args, **kwargs):
    """Replace @pytest.fixture with a no-op decorator during module load."""
    if args and callable(args[0]):
        return args[0]
    return lambda f: f

pytest.fixture = _passthrough_fixture
try:
    _spec = importlib.util.spec_from_file_location("tests_conftest", _conftest_path)
    _conftest = importlib.util.module_from_spec(_spec)
    _spec.loader.exec_module(_conftest)
finally:
    pytest.fixture = _orig_fixture

coverage_capture_fn = _conftest.coverage_capture
gcov_dump_capture_fn = _conftest.gcov_dump_capture
save_perf_result_fn = _conftest.save_perf_result
wait_for_gcov_dump_fn = _conftest.wait_for_gcov_dump
_get_coverage_log_path_fn = _conftest._get_coverage_log_path

GCOV_START = _conftest.GCOV_START
GCOV_END = _conftest.GCOV_END
GCOV_START_TIMEOUT = _conftest.GCOV_START_TIMEOUT
GCOV_DUMP_TIMEOUT = _conftest.GCOV_DUMP_TIMEOUT


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_request(xmlpath=None, fspath="/fake/tests/test_example.py"):
    """Create a mock request object."""
    req = mock.MagicMock()
    req.config.getoption.return_value = xmlpath
    req.fspath = fspath
    req.path = Path(fspath)
    req.node = SimpleNamespace()
    return req


def _run_coverage_capture(dut, request):
    """Execute the coverage_capture generator (yield-fixture pattern)."""
    gen = coverage_capture_fn(dut, request)
    next(gen)  # run up to yield
    try:
        next(gen)  # run teardown
    except StopIteration:
        pass


def _make_dut(capture_return=True):
    """Create a mock dut with capture_payload_to_file stubbed."""
    dut = mock.MagicMock()
    dut.capture_payload_to_file.return_value = capture_return
    return dut


# ---------------------------------------------------------------------------
# _get_coverage_log_path tests
# ---------------------------------------------------------------------------

class TestGetCoverageLogPath:
    def test_derives_from_xmlpath(self, tmp_path):
        req = _make_request(xmlpath=str(tmp_path / "results.xml"))
        assert _get_coverage_log_path_fn(req) == str(tmp_path / "results_coverage.log")

    def test_falls_back_to_fspath(self, tmp_path):
        req = _make_request(xmlpath=None, fspath=str(tmp_path / "test_foo.py"))
        assert _get_coverage_log_path_fn(req) == str(tmp_path / "coverage_serial.log")

    def test_indexed_xml_path(self, tmp_path):
        req = _make_request(xmlpath=str(tmp_path / "nvs0.xml"))
        assert _get_coverage_log_path_fn(req) == str(tmp_path / "nvs0_coverage.log")


# ---------------------------------------------------------------------------
# wait_for_gcov_dump tests
# ---------------------------------------------------------------------------

class TestWaitForGcovDump:
    def test_delegates_to_capture_payload_to_file(self, tmp_path):
        dut = _make_dut(capture_return=True)
        out = str(tmp_path / "cov.log")
        assert wait_for_gcov_dump_fn(dut, out) is True
        dut.capture_payload_to_file.assert_called_once_with(
            start=GCOV_START,
            end=GCOV_END,
            filepath=out,
            start_timeout=GCOV_START_TIMEOUT,
            timeout=GCOV_DUMP_TIMEOUT,
        )

    def test_returns_false_on_failure(self, tmp_path):
        dut = _make_dut(capture_return=False)
        out = str(tmp_path / "cov.log")
        assert wait_for_gcov_dump_fn(dut, out) is False

    def test_returns_true_on_success(self, tmp_path):
        dut = _make_dut(capture_return=True)
        out = str(tmp_path / "cov.log")
        assert wait_for_gcov_dump_fn(dut, out) is True


# ---------------------------------------------------------------------------
# coverage_capture fixture tests
# ---------------------------------------------------------------------------

class TestCoverageCapture:
    def test_no_capture_when_payload_missing(self, tmp_path):
        """When firmware has no coverage, capture_payload_to_file returns False."""
        dut = _make_dut(capture_return=False)
        req = _make_request(xmlpath=str(tmp_path / "results.xml"))

        _run_coverage_capture(dut, req)

        dut.capture_payload_to_file.assert_called_once()

    def test_captures_data(self, tmp_path):
        """Full flow: capture_payload_to_file is called with correct args."""
        dut = _make_dut(capture_return=True)
        req = _make_request(xmlpath=str(tmp_path / "results.xml"))

        _run_coverage_capture(dut, req)

        dut.capture_payload_to_file.assert_called_once()

    def test_skips_when_gcov_captured_flag_set(self, tmp_path):
        """Autouse fixture does nothing when test used gcov_dump_capture."""
        dut = _make_dut(capture_return=True)
        req = _make_request(xmlpath=str(tmp_path / "results.xml"))
        req.node._gcov_captured = True

        _run_coverage_capture(dut, req)

        dut.capture_payload_to_file.assert_not_called()


# ---------------------------------------------------------------------------
# gcov_dump_capture fixture tests
# ---------------------------------------------------------------------------

class TestGcovDumpCapture:
    def test_returns_callable(self, tmp_path):
        dut = _make_dut(capture_return=True)
        req = _make_request(xmlpath=str(tmp_path / "results.xml"))
        gen = gcov_dump_capture_fn(dut, req)
        capture = next(gen)
        assert callable(capture)

    def test_callable_sets_flag(self, tmp_path):
        dut = _make_dut(capture_return=True)
        req = _make_request(xmlpath=str(tmp_path / "results.xml"))
        gen = gcov_dump_capture_fn(dut, req)
        capture = next(gen)
        assert not getattr(req.node, "_gcov_captured", False)
        capture()
        assert req.node._gcov_captured is True

    def test_multiple_calls_invoke_capture(self, tmp_path):
        """Calling the capture callable multiple times invokes capture_payload_to_file each time."""
        dut = _make_dut(capture_return=True)
        req = _make_request(xmlpath=str(tmp_path / "results.xml"))
        gen = gcov_dump_capture_fn(dut, req)
        capture = next(gen)

        assert capture() is True
        assert capture() is True

        assert dut.capture_payload_to_file.call_count == 2


# ---------------------------------------------------------------------------
# save_perf_result tests
# ---------------------------------------------------------------------------

class TestSavePerfResult:
    def _make_save_fixture(self, tmp_path, target="esp32"):
        """Create the save callable from the fixture."""
        dut = mock.MagicMock()
        dut.app.target = target
        req = mock.MagicMock()
        req.path = tmp_path / "test_perf.py"
        return save_perf_result_fn(dut, req)

    def test_saves_json_file(self, tmp_path):
        save = self._make_save_fixture(tmp_path)
        results = {
            "test_name": "coremark",
            "runs": 3,
            "settings": "",
            "metrics": [{"name": "score", "value": 1000.0, "unit": "iterations/s"}],
        }
        save(results)

        out_file = tmp_path / "esp32" / "result_coremark0.json"
        assert out_file.exists()
        data = json.loads(out_file.read_text())
        assert data["test_name"] == "coremark"
        assert data["runs"] == 3

    def test_increments_index_on_collision(self, tmp_path):
        save = self._make_save_fixture(tmp_path)
        target_dir = tmp_path / "esp32"
        target_dir.mkdir(parents=True)
        (target_dir / "result_bench0.json").write_text("{}")

        results = {"test_name": "bench", "runs": 1, "settings": "", "metrics": []}
        save(results)

        assert (target_dir / "result_bench1.json").exists()

    def test_multiple_increments(self, tmp_path):
        save = self._make_save_fixture(tmp_path)
        target_dir = tmp_path / "esp32"
        target_dir.mkdir(parents=True)
        (target_dir / "result_x0.json").write_text("{}")
        (target_dir / "result_x1.json").write_text("{}")

        results = {"test_name": "x", "runs": 1, "settings": "", "metrics": []}
        save(results)

        assert (target_dir / "result_x2.json").exists()

    def test_default_test_name(self, tmp_path):
        save = self._make_save_fixture(tmp_path)
        results = {"runs": 1, "settings": "", "metrics": []}
        save(results)

        out_file = tmp_path / "esp32" / "result_unknown0.json"
        assert out_file.exists()

    def test_creates_target_dir(self, tmp_path):
        save = self._make_save_fixture(tmp_path, target="esp32s3")
        results = {"test_name": "test", "runs": 1, "settings": "", "metrics": []}
        save(results)

        assert (tmp_path / "esp32s3" / "result_test0.json").exists()

    def test_json_content_matches(self, tmp_path):
        save = self._make_save_fixture(tmp_path)
        results = {
            "test_name": "perf",
            "runs": 10,
            "settings": "opt=O2",
            "metrics": [{"name": "latency", "value": 5.5, "unit": "ms"}],
        }
        save(results)

        out_file = tmp_path / "esp32" / "result_perf0.json"
        data = json.loads(out_file.read_text())
        assert data == results
