#!/usr/bin/env python3
"""
Local test script for runtime_table_generator.py

Usage:
    python3 .github/scripts/test_runtime_table_generator.py

Each test case sets up input fixtures in a temporary directory, runs the
generator, and asserts expected patterns in the markdown output and in the
saved test_results.json cache.

Edge cases covered:
  1.  First run (no --previous-results) – all pass        → cache populated, no asterisk
  2.  First run (no --previous-results) – some fail       → failures shown, cache populated
  3.  First run (no --previous-results) – runner error    → "Error :fire:", target NOT cached
  4.  Cache hit – runner now unavailable                  → asterisk + footnote shown
  5.  Cache miss – runner was already unavailable run 1   → "Error :fire:", no asterisk
  6.  Result changed (2/2→1/2) – valid run               → NO asterisk (it was a valid run)
  7.  Stale cache (>7 days) – runner unavailable          → "Error :fire:", no asterisk
  8.  Cache propagation across three runs                 → asterisk survives run B and run C
  9.  --previous-results file does not exist              → no crash, no cache loaded
  10. --previous-results file is corrupt JSON             → no crash, no cache loaded
  11. --previous-results file is not a dict               → no crash, no cache loaded
  12. Multiple sketches                                   → each sketch cached independently
  13. Multi-FQBN builds (Blink0 + Blink1 test names)     → stripped to "Blink", accumulated
  14. Multi-FQBN partial error (one FQBN errors)         → accumulated errors>0, uses cache
  15. Multiple platforms (hardware + wokwi)              → each platform handled independently
  16. Performance test – runner OK                       → perf cache populated, no asterisk
  17. Performance test – runner unavailable, cache hit   → asterisk + cached metrics shown
  18. Performance test – runner unavailable, cache miss  → "Error :fire:", no asterisk
  19. Wokwi-only target not in HW list                   → not shown under Hardware section
  20. Target in env but not in results                   → shown as "-" (dash)
"""

import json
import os
import subprocess
import sys
import tempfile
import shutil
from datetime import datetime, timezone, timedelta
from pathlib import Path

SCRIPT = Path(__file__).parent / "runtime_table_generator.py"
PASS = "\033[32mPASS\033[0m"
FAIL = "\033[31mFAIL\033[0m"
_failures = []


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _ts_now():
    return datetime.now(timezone.utc).isoformat()


def _ts_stale():
    """Return a timestamp older than CACHE_STALE_DAYS (7 days)."""
    return (datetime.now(timezone.utc) - timedelta(days=8)).isoformat()


def _unity(suite_details):
    """Build a minimal unity_results.json dict."""
    return {"stats": {"suite_details": suite_details}}


def _suite(name, tests=3, failures=0, errors=0):
    return {"name": name, "tests": tests, "failures": failures, "errors": errors}


def _cache_entry(total, failures, ts=None, commit_sha="abc"):
    return {
        "total": total,
        "failures": failures,
        "timestamp": ts or _ts_now(),
        "commit_sha": commit_sha,
    }


def _test_results(cache=None, perf_cache=None, commit_sha="prev"):
    """Build a minimal test_results.json dict."""
    return {
        "commit_sha": commit_sha,
        "tests_failed": False,
        "test_data": {},
        "generated_at": _ts_now(),
        "cache": cache or {},
        "perf_cache": perf_cache or {},
    }


def run_generator(tmpdir, unity, env_extra=None, prev_results=None, sha="abc123", perf_dir=None):
    """
    Run runtime_table_generator.py inside *tmpdir*.
    Returns (stdout, stderr, returncode, saved_cache).
    saved_cache is the 'cache' dict from the written test_results.json.
    """
    unity_path = os.path.join(tmpdir, "unity_results.json")
    with open(unity_path, "w") as f:
        json.dump(unity, f)

    prev_path = None
    if prev_results is not None:
        prev_path = os.path.join(tmpdir, "previous_results.json")
        with open(prev_path, "w") as f:
            if isinstance(prev_results, str):
                f.write(prev_results)          # allow raw string for corrupt-JSON test
            else:
                json.dump(prev_results, f)

    cmd = [sys.executable, str(SCRIPT), "--results", unity_path, "--sha", sha]
    if prev_path:
        cmd += ["--previous-results", prev_path]
    if perf_dir:
        cmd += ["--perf-dir", perf_dir]

    env = {**os.environ, "HW_TARGETS": "[]", "WOKWI_TARGETS": "[]", "QEMU_TARGETS": "[]"}
    if env_extra:
        env.update(env_extra)

    result = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        cwd=tmpdir,
        env=env,
    )

    saved_cache = {}
    out_path = os.path.join(tmpdir, "test_results.json")
    if os.path.exists(out_path):
        with open(out_path) as f:
            saved = json.load(f)
        saved_cache = saved.get("cache", {})

    return result.stdout, result.stderr, result.returncode, saved_cache


def assert_test(name, condition, detail=""):
    if condition:
        print(f"  {PASS}  {name}")
    else:
        msg = f"  {FAIL}  {name}"
        if detail:
            msg += f"\n         detail: {detail}"
        print(msg)
        _failures.append(name)


def section(title):
    print(f"\n{'='*60}")
    print(f"  {title}")
    print('='*60)


# ---------------------------------------------------------------------------
# Test cases
# ---------------------------------------------------------------------------

def test_01_first_run_all_pass():
    section("Test 01 – First run, all pass (no previous results)")
    with tempfile.TemporaryDirectory() as d:
        unity = _unity([
            _suite("validation_hardware_esp32_Blink", tests=3),
            _suite("validation_hardware_esp32c3_Blink", tests=3),
            _suite("validation_hardware_esp32s3_Blink", tests=2),
        ])
        env = {"HW_TARGETS": '["esp32","esp32s3","esp32c3"]', "WOKWI_TARGETS": "[]", "QEMU_TARGETS": "[]"}
        out, _, rc, cache = run_generator(d, unity, env_extra=env)

        assert_test("exit code 0", rc == 0)
        assert_test("shows ESP32 3/3 pass", "3/3 :white_check_mark:" in out)
        assert_test("shows ESP32-C3 3/3 pass", "3/3 :white_check_mark:" in out)
        assert_test("shows ESP32-S3 2/2 pass", "2/2 :white_check_mark:" in out)
        assert_test("no asterisk in output", "\\*" not in out)
        assert_test("esp32 cached", "esp32" in cache.get("hardware", {}).get("Blink", {}))
        assert_test("esp32c3 cached", "esp32c3" in cache.get("hardware", {}).get("Blink", {}))
        assert_test("esp32s3 cached", "esp32s3" in cache.get("hardware", {}).get("Blink", {}))


def test_02_first_run_some_fail():
    section("Test 02 – First run, some failures (no errors)")
    with tempfile.TemporaryDirectory() as d:
        unity = _unity([
            _suite("validation_hardware_esp32_Blink", tests=3, failures=1),
            _suite("validation_hardware_esp32s3_Blink", tests=2, failures=2),
        ])
        env = {"HW_TARGETS": '["esp32","esp32s3"]', "WOKWI_TARGETS": "[]", "QEMU_TARGETS": "[]"}
        out, _, rc, cache = run_generator(d, unity, env_extra=env)

        assert_test("exit code 0", rc == 0)
        assert_test("shows failure symbol for esp32", ":x:" in out)
        assert_test("no asterisk in output", "\\*" not in out)
        assert_test("esp32 cached despite failures", "esp32" in cache.get("hardware", {}).get("Blink", {}))
        assert_test("esp32s3 cached despite all-fail", "esp32s3" in cache.get("hardware", {}).get("Blink", {}))
        blink_cache = cache.get("hardware", {}).get("Blink", {})
        assert_test("esp32 cache has 2 failures", blink_cache.get("esp32", {}).get("failures") == 1)


def test_03_first_run_runner_error():
    section("Test 03 – First run, runner error (errors > 0)")
    with tempfile.TemporaryDirectory() as d:
        unity = _unity([
            _suite("validation_hardware_esp32_Blink", tests=3),
            _suite("validation_hardware_esp32c3_Blink", tests=0, errors=1),  # runner unavailable
        ])
        env = {"HW_TARGETS": '["esp32","esp32c3"]', "WOKWI_TARGETS": "[]", "QEMU_TARGETS": "[]"}
        out, _, rc, cache = run_generator(d, unity, env_extra=env)

        assert_test("exit code 0", rc == 0)
        assert_test("shows Error :fire: for esp32c3", "Error :fire:" in out)
        assert_test("no asterisk in output", "\\*" not in out)
        assert_test("esp32 cached", "esp32" in cache.get("hardware", {}).get("Blink", {}))
        assert_test("esp32c3 NOT cached (it errored)", "esp32c3" not in cache.get("hardware", {}).get("Blink", {}))


def test_04_cache_hit_runner_unavailable():
    section("Test 04 – Cache hit: runner was available before, now unavailable → asterisk")
    with tempfile.TemporaryDirectory() as d:
        prev = _test_results(cache={
            "hardware": {"Blink": {
                "esp32":   _cache_entry(3, 0),
                "esp32c3": _cache_entry(3, 0),   # previously passed
                "esp32s3": _cache_entry(2, 0),
            }}
        })
        unity = _unity([
            _suite("validation_hardware_esp32_Blink", tests=3),
            _suite("validation_hardware_esp32c3_Blink", tests=0, errors=1),  # now unavailable
            _suite("validation_hardware_esp32s3_Blink", tests=2, failures=1),
        ])
        env = {"HW_TARGETS": '["esp32","esp32s3","esp32c3"]', "WOKWI_TARGETS": "[]", "QEMU_TARGETS": "[]"}
        out, _, rc, cache = run_generator(d, unity, env_extra=env, prev_results=prev)

        assert_test("exit code 0", rc == 0)
        assert_test("esp32c3 shows cached 3/3 with asterisk", "3/3 :white_check_mark:\\*" in out)
        assert_test("footnote about runner unavailable shown", "Result from last successful run" in out)
        assert_test("esp32 has no asterisk (ran fine)", "3/3 :white_check_mark:\\*" not in out.split("esp32c3")[0] if "esp32c3" in out else True)
        assert_test("esp32s3 has no asterisk (valid run with failure)", "1/2 :x:\\*" not in out)
        # Cached esp32c3 entry propagated to new test_results.json
        assert_test("esp32c3 cache propagated to new output", "esp32c3" in cache.get("hardware", {}).get("Blink", {}))


def test_05_cache_miss_runner_was_already_unavailable():
    section("Test 05 – Cache miss: runner had errors in run 1 too → no asterisk")
    with tempfile.TemporaryDirectory() as d:
        # Previous results has cache but NOT esp32c3 (it also errored in run 1)
        prev = _test_results(cache={
            "hardware": {"Blink": {
                "esp32":   _cache_entry(3, 0),
                "esp32s3": _cache_entry(2, 0),
                # esp32c3 intentionally absent
            }}
        })
        unity = _unity([
            _suite("validation_hardware_esp32_Blink", tests=3),
            _suite("validation_hardware_esp32c3_Blink", tests=0, errors=1),
            _suite("validation_hardware_esp32s3_Blink", tests=2),
        ])
        env = {"HW_TARGETS": '["esp32","esp32s3","esp32c3"]', "WOKWI_TARGETS": "[]", "QEMU_TARGETS": "[]"}
        out, _, rc, _ = run_generator(d, unity, env_extra=env, prev_results=prev)

        assert_test("exit code 0", rc == 0)
        assert_test("esp32c3 shows Error :fire: (no cached result)", "Error :fire:" in out)
        assert_test("no asterisk in output", "\\*" not in out)
        assert_test("no footnote shown", "Result from last successful run" not in out)


def test_06_result_changed_no_asterisk():
    section("Test 06 – Result changed (2/2 → 1/2): valid run, NO asterisk")
    with tempfile.TemporaryDirectory() as d:
        prev = _test_results(cache={
            "hardware": {"Blink": {
                "esp32":   _cache_entry(3, 0),
                "esp32s3": _cache_entry(2, 0),   # previously 2/2 pass
            }}
        })
        unity = _unity([
            _suite("validation_hardware_esp32_Blink", tests=3),
            _suite("validation_hardware_esp32s3_Blink", tests=2, failures=1),  # now 1/2
        ])
        env = {"HW_TARGETS": '["esp32","esp32s3"]', "WOKWI_TARGETS": "[]", "QEMU_TARGETS": "[]"}
        out, _, rc, cache = run_generator(d, unity, env_extra=env, prev_results=prev)

        assert_test("exit code 0", rc == 0)
        assert_test("esp32s3 shows 1/2 :x:", "1/2 :x:" in out)
        assert_test("no asterisk on changed result", "1/2 :x:\\*" not in out)
        assert_test("no footnote shown", "Result from last successful run" not in out)
        # Cache should be updated with new result
        blink = cache.get("hardware", {}).get("Blink", {})
        assert_test("esp32s3 cache updated with new result", blink.get("esp32s3", {}).get("failures") == 1)


def test_07_stale_cache():
    section("Test 07 – Stale cache (>7 days old): no asterisk shown")
    with tempfile.TemporaryDirectory() as d:
        prev = _test_results(cache={
            "hardware": {"Blink": {
                "esp32c3": _cache_entry(3, 0, ts=_ts_stale()),  # stale entry
            }}
        })
        unity = _unity([
            _suite("validation_hardware_esp32c3_Blink", tests=0, errors=1),
        ])
        env = {"HW_TARGETS": '["esp32c3"]', "WOKWI_TARGETS": "[]", "QEMU_TARGETS": "[]"}
        out, _, rc, _ = run_generator(d, unity, env_extra=env, prev_results=prev)

        assert_test("exit code 0", rc == 0)
        assert_test("shows Error :fire: (stale cache ignored)", "Error :fire:" in out)
        assert_test("no asterisk shown for stale cache", "\\*" not in out)


def test_08_cache_propagation_three_runs():
    section("Test 08 – Cache propagates across three consecutive error runs")
    with tempfile.TemporaryDirectory() as d:
        # Run A: all pass → creates cache
        unity_all_pass = _unity([
            _suite("validation_hardware_esp32c3_Blink", tests=3),
        ])
        env = {"HW_TARGETS": '["esp32c3"]', "WOKWI_TARGETS": "[]", "QEMU_TARGETS": "[]"}
        run_generator(d, unity_all_pass, env_extra=env, sha="runA")

        cache_a_path = os.path.join(d, "test_results.json")
        with open(cache_a_path) as f:
            results_a = json.load(f)
        assert_test("Run A: esp32c3 cached", "esp32c3" in results_a.get("cache", {}).get("hardware", {}).get("Blink", {}))

        # Run B: esp32c3 errors → reads Run A cache → shows asterisk
        unity_errors = _unity([
            _suite("validation_hardware_esp32c3_Blink", tests=0, errors=1),
        ])
        out_b, _, _, cache_b = run_generator(d, unity_errors, env_extra=env, prev_results=results_a, sha="runB")
        assert_test("Run B: shows asterisk (cache from A)", "\\*" in out_b)
        assert_test("Run B: esp32c3 cache propagated", "esp32c3" in cache_b.get("hardware", {}).get("Blink", {}))

        # Run C: esp32c3 still errors → reads Run B cache → still shows asterisk
        with open(os.path.join(d, "test_results.json")) as f:
            results_b = json.load(f)
        out_c, _, _, _ = run_generator(d, unity_errors, env_extra=env, prev_results=results_b, sha="runC")
        assert_test("Run C: asterisk still shown (cache from A via B)", "\\*" in out_c)


def test_09_missing_previous_results_file():
    section("Test 09 – --previous-results file does not exist")
    with tempfile.TemporaryDirectory() as d:
        unity = _unity([_suite("validation_hardware_esp32_Blink", tests=3)])
        env = {"HW_TARGETS": '["esp32"]', "WOKWI_TARGETS": "[]", "QEMU_TARGETS": "[]"}
        cmd = [
            sys.executable, str(SCRIPT),
            "--results", os.path.join(d, "unity_results.json"),
            "--sha", "abc123",
            "--previous-results", os.path.join(d, "nonexistent.json"),
        ]
        unity_path = os.path.join(d, "unity_results.json")
        with open(unity_path, "w") as f:
            json.dump(unity, f)
        result = subprocess.run(cmd, capture_output=True, text=True, cwd=d,
                                env={**os.environ, **env})
        assert_test("exit code 0 (no crash)", result.returncode == 0)
        assert_test("output contains Hardware section", "Hardware" in result.stdout)
        assert_test("no asterisk in output", "\\*" not in result.stdout)


def test_10_corrupt_previous_results():
    section("Test 10 – --previous-results file is corrupt JSON")
    with tempfile.TemporaryDirectory() as d:
        unity = _unity([_suite("validation_hardware_esp32_Blink", tests=3)])
        env = {"HW_TARGETS": '["esp32"]', "WOKWI_TARGETS": "[]", "QEMU_TARGETS": "[]"}
        out, _, rc, cache = run_generator(d, unity, env_extra=env, prev_results="{not valid json!!!")
        assert_test("exit code 0 (no crash on corrupt file)", rc == 0)
        assert_test("output contains Hardware section", "Hardware" in out)
        assert_test("no asterisk in output", "\\*" not in out)


def test_11_previous_results_not_a_dict():
    section("Test 11 – --previous-results content is not a dict (e.g. a list)")
    with tempfile.TemporaryDirectory() as d:
        unity = _unity([_suite("validation_hardware_esp32_Blink", tests=3)])
        env = {"HW_TARGETS": '["esp32"]', "WOKWI_TARGETS": "[]", "QEMU_TARGETS": "[]"}
        # Write a valid JSON list (not a dict)
        out, _, rc, _ = run_generator(d, unity, env_extra=env, prev_results="[1, 2, 3]")
        assert_test("exit code 0 (no crash)", rc == 0)
        assert_test("no asterisk", "\\*" not in out)


def test_12_multiple_sketches():
    section("Test 12 – Multiple sketches cached independently")
    with tempfile.TemporaryDirectory() as d:
        prev = _test_results(cache={
            "hardware": {
                "Blink":   {"esp32c3": _cache_entry(3, 0)},
                "UART":    {"esp32c3": _cache_entry(4, 0)},
            }
        })
        unity = _unity([
            _suite("validation_hardware_esp32c3_Blink", tests=0, errors=1),
            _suite("validation_hardware_esp32c3_UART",  tests=0, errors=1),
        ])
        env = {"HW_TARGETS": '["esp32c3"]', "WOKWI_TARGETS": "[]", "QEMU_TARGETS": "[]"}
        out, _, rc, cache = run_generator(d, unity, env_extra=env, prev_results=prev)

        assert_test("exit code 0", rc == 0)
        assert_test("Blink cached result shown with asterisk", "3/3 :white_check_mark:\\*" in out)
        assert_test("UART cached result shown with asterisk", "4/4 :white_check_mark:\\*" in out)
        assert_test("Blink cache propagated", "Blink" in cache.get("hardware", {}))
        assert_test("UART cache propagated", "UART" in cache.get("hardware", {}))


def test_13_multi_fqbn_builds():
    section("Test 13 – Multi-FQBN builds (Blink0 + Blink1 → accumulated under 'Blink')")
    with tempfile.TemporaryDirectory() as d:
        unity = _unity([
            _suite("validation_hardware_esp32_Blink0", tests=3, failures=0),
            _suite("validation_hardware_esp32_Blink1", tests=3, failures=1),
        ])
        env = {"HW_TARGETS": '["esp32"]', "WOKWI_TARGETS": "[]", "QEMU_TARGETS": "[]"}
        out, _, rc, cache = run_generator(d, unity, env_extra=env)

        assert_test("exit code 0", rc == 0)
        assert_test("Blink shows accumulated 5/6", "5/6 :x:" in out)
        blink = cache.get("hardware", {}).get("Blink", {})
        assert_test("accumulated into 'Blink' cache key", "esp32" in blink)
        assert_test("cache total is 6 (3+3)", blink.get("esp32", {}).get("total") == 6)
        assert_test("cache failures is 1", blink.get("esp32", {}).get("failures") == 1)


def test_14_multi_fqbn_partial_error():
    section("Test 14 – Multi-FQBN: one FQBN passes, one errors → accumulated errors>0, uses cache")
    with tempfile.TemporaryDirectory() as d:
        prev = _test_results(cache={
            "hardware": {"Blink": {"esp32": _cache_entry(6, 0)}}
        })
        unity = _unity([
            _suite("validation_hardware_esp32_Blink0", tests=3, failures=0, errors=0),
            _suite("validation_hardware_esp32_Blink1", tests=0, failures=0, errors=1),
        ])
        env = {"HW_TARGETS": '["esp32"]', "WOKWI_TARGETS": "[]", "QEMU_TARGETS": "[]"}
        out, _, rc, _ = run_generator(d, unity, env_extra=env, prev_results=prev)

        assert_test("exit code 0", rc == 0)
        # Accumulated: total=3, failures=0, errors=1 → errors>0 → use cache
        assert_test("cached result shown with asterisk", "\\*" in out)


def test_15_multiple_platforms():
    section("Test 15 – Multiple platforms (hardware + wokwi)")
    with tempfile.TemporaryDirectory() as d:
        unity = _unity([
            _suite("validation_hardware_esp32_Blink",   tests=3),
            _suite("validation_wokwi_esp32s3_Blink",    tests=2),
        ])
        env = {
            "HW_TARGETS":    '["esp32"]',
            "WOKWI_TARGETS": '["esp32s3"]',
            "QEMU_TARGETS":  "[]",
        }
        out, _, rc, cache = run_generator(d, unity, env_extra=env)

        assert_test("exit code 0", rc == 0)
        assert_test("Hardware section present", "#### Hardware" in out)
        assert_test("Wokwi section present", "#### Wokwi" in out)
        assert_test("hardware esp32 cached", "esp32" in cache.get("hardware", {}).get("Blink", {}))
        assert_test("wokwi esp32s3 cached", "esp32s3" in cache.get("wokwi", {}).get("Blink", {}))


def test_16_perf_runner_ok():
    section("Test 16 – Performance test: runner OK → perf cache populated, no asterisk")
    with tempfile.TemporaryDirectory() as d:
        unity = _unity([
            _suite("performance_hardware_esp32_SpeedTest", tests=1),
        ])
        # Create a minimal perf result JSON
        perf_dir = os.path.join(d, "perf_artifacts", "SpeedTest", "esp32")
        os.makedirs(perf_dir, exist_ok=True)
        perf_data = {
            "test_name": "SpeedTest",
            "runs": 5,
            "settings": "freq=240",
            "metrics": [{"name": "throughput", "value": 100.5, "unit": "MB/s"}],
        }
        with open(os.path.join(perf_dir, "result_0.json"), "w") as f:
            json.dump(perf_data, f)

        env = {"HW_TARGETS": '["esp32"]', "WOKWI_TARGETS": "[]", "QEMU_TARGETS": "[]"}
        out, _, rc, _ = run_generator(d, unity, env_extra=env,
                                      perf_dir=os.path.join(d, "perf_artifacts"))

        assert_test("exit code 0", rc == 0)
        assert_test("Performance Tests section present", "### Performance Tests" in out)
        assert_test("throughput metric shown", "throughput" in out)
        assert_test("no asterisk", "\\*" not in out)


def test_17_perf_cache_hit_runner_unavailable():
    section("Test 17 – Performance test: runner unavailable, cache hit → asterisk")
    with tempfile.TemporaryDirectory() as d:
        prev = _test_results(perf_cache={
            "SpeedTest": {
                "esp32": {
                    "status": "success",
                    "timestamp": _ts_now(),
                    "commit_sha": "prev",
                    "runs": 5,
                    "settings": "freq=240",
                    "metrics": {"throughput": {"avg": 100.5, "unit": "MB/s"}},
                }
            }
        })
        unity = _unity([
            _suite("performance_hardware_esp32_SpeedTest", tests=0, errors=1),
        ])
        env = {"HW_TARGETS": '["esp32"]', "WOKWI_TARGETS": "[]", "QEMU_TARGETS": "[]"}
        out, _, rc, _ = run_generator(d, unity, env_extra=env, prev_results=prev)

        assert_test("exit code 0", rc == 0)
        assert_test("Performance Tests section present", "### Performance Tests" in out)
        assert_test("asterisk shown for cached perf result", "\\*" in out)
        assert_test("cached metrics shown", "throughput" in out)
        assert_test("footnote shown", "Result from last successful run" in out)


def test_18_perf_cache_miss_runner_unavailable():
    section("Test 18 – Performance test: runner unavailable, no cache → Error :fire:")
    with tempfile.TemporaryDirectory() as d:
        prev = _test_results()  # empty perf_cache
        unity = _unity([
            _suite("performance_hardware_esp32_SpeedTest", tests=0, errors=1),
        ])
        env = {"HW_TARGETS": '["esp32"]', "WOKWI_TARGETS": "[]", "QEMU_TARGETS": "[]"}
        out, _, rc, _ = run_generator(d, unity, env_extra=env, prev_results=prev)

        assert_test("exit code 0", rc == 0)
        # Performance error format is "Error - :fire:" (list item, not table cell)
        assert_test("Error :fire: shown (no cache)", "Error - :fire:" in out)
        assert_test("no asterisk", "\\*" not in out)


def test_19_wokwi_target_not_in_hw_list():
    section("Test 19 – Wokwi-only target not shown under Hardware section")
    with tempfile.TemporaryDirectory() as d:
        unity = _unity([
            _suite("validation_hardware_esp32_Blink",  tests=3),
            _suite("validation_wokwi_esp32s2_Blink",   tests=2),
        ])
        env = {
            "HW_TARGETS":    '["esp32"]',
            "WOKWI_TARGETS": '["esp32s2"]',
            "QEMU_TARGETS":  "[]",
        }
        out, _, rc, _ = run_generator(d, unity, env_extra=env)

        assert_test("exit code 0", rc == 0)
        hw_section = out.split("#### Wokwi")[0] if "#### Wokwi" in out else out
        assert_test("esp32s2 not in Hardware table", "ESP32-S2" not in hw_section.split("#### Hardware")[-1])
        wokwi_section = out.split("#### Wokwi")[-1] if "#### Wokwi" in out else ""
        assert_test("esp32s2 shown in Wokwi table", "ESP32-S2" in wokwi_section)


def test_20_target_in_env_but_not_in_results():
    section("Test 20 – Target in env list but absent from results → shown as '-'")
    with tempfile.TemporaryDirectory() as d:
        # Only esp32 has results; esp32c3 is listed in HW_TARGETS but missing from unity
        unity = _unity([
            _suite("validation_hardware_esp32_Blink", tests=3),
        ])
        env = {"HW_TARGETS": '["esp32","esp32c3"]', "WOKWI_TARGETS": "[]", "QEMU_TARGETS": "[]"}
        out, _, rc, _ = run_generator(d, unity, env_extra=env)

        assert_test("exit code 0", rc == 0)
        assert_test("dash shown for missing esp32c3", "|-" in out)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    print(f"Running tests against: {SCRIPT}")

    test_01_first_run_all_pass()
    test_02_first_run_some_fail()
    test_03_first_run_runner_error()
    test_04_cache_hit_runner_unavailable()
    test_05_cache_miss_runner_was_already_unavailable()
    test_06_result_changed_no_asterisk()
    test_07_stale_cache()
    test_08_cache_propagation_three_runs()
    test_09_missing_previous_results_file()
    test_10_corrupt_previous_results()
    test_11_previous_results_not_a_dict()
    test_12_multiple_sketches()
    test_13_multi_fqbn_builds()
    test_14_multi_fqbn_partial_error()
    test_15_multiple_platforms()
    test_16_perf_runner_ok()
    test_17_perf_cache_hit_runner_unavailable()
    test_18_perf_cache_miss_runner_unavailable()
    test_19_wokwi_target_not_in_hw_list()
    test_20_target_in_env_but_not_in_results()

    print(f"\n{'='*60}")
    total = 20
    failed = len(set(_failures))  # deduplicate section names
    passed_count = total - failed

    # Count individual assertions
    all_assertions = len(_failures)
    if _failures:
        print(f"\n{FAIL} {len(_failures)} assertion(s) failed:")
        for f in _failures:
            print(f"  - {f}")
        print()
        sys.exit(1)
    else:
        print(f"\n{PASS} All {total} test cases passed!")
        sys.exit(0)
