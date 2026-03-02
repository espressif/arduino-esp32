import argparse
import json
import logging
import sys
import os
import re
from datetime import datetime
from collections import defaultdict

# Configure logging
logging.basicConfig(level=logging.DEBUG, format='[%(levelname)s] %(message)s', stream=sys.stderr)

SUCCESS_SYMBOL = ":white_check_mark:"
FAILURE_SYMBOL = ":x:"
ERROR_SYMBOL = ":fire:"


def _natural_sort_key(s):
    """Sort key that treats embedded numbers numerically (e.g. 32 < 128 < 1024)."""
    return [int(part) if part.isdigit() else part.lower() for part in re.split(r'(\d+)', s)]


def _display_target(target):
    """Format target name for display (e.g. 'esp32s3' -> 'ESP32-S3')."""
    if target == "esp32":
        return target.upper()
    return target.replace("esp32", "esp32-").upper()


def _junit_status(junit):
    """Return (label, symbol) from a JUnit status dict, or default to Success."""
    if junit and junit["errors"] > 0:
        return "Error", ERROR_SYMBOL
    if junit and junit["failures"] > 0:
        return "Failure", FAILURE_SYMBOL
    return "Success", SUCCESS_SYMBOL


def _parse_performance_json(data, test_name_from_path=None):
    """
    Parse performance result JSON in canonical form only.
    See .github/CI_README.md (Performance test result format).
    Returns dict with test_name, runs, settings, metrics; or None if invalid.
    """
    if not isinstance(data, dict) or "test_name" not in data or "runs" not in data or "metrics" not in data:
        logging.warning("Performance JSON missing test_name, runs, or metrics (canonical format required), skipping")
        return None
    runs = int(data["runs"])
    if runs <= 0:
        return None
    metrics = data["metrics"]
    if not metrics:
        return None
    test_name = test_name_from_path if test_name_from_path else data["test_name"]
    return {
        "test_name": test_name,
        "runs": runs,
        "settings": data.get("settings", ""),
        "metrics": list(metrics),
    }


def _collect_and_aggregate_performance(perf_dir):
    """
    Find all result_*.json under perf_dir; collect by (target, test_name).
    Each file's metric "value" is already the average from the test. Only when
    multiple files exist for the same (target, test_name) do we compute a
    weighted average across files. Otherwise we use the single file's values.
    Path structure expected: .../<test_name>/<target>/result_*.json
    """
    by_target_test = defaultdict(list)  # (target, test_name) -> list of {runs, settings, metrics}

    for root, _dirs, files in os.walk(perf_dir):
        for f in files:
            if not f.startswith("result_") or not f.endswith(".json"):
                continue
            path = os.path.join(root, f)
            try:
                with open(path, "r") as fp:
                    data = json.load(fp)
            except (json.JSONDecodeError, OSError) as e:
                logging.warning("Failed to read %s: %s", path, e)
                continue

            parts = os.path.normpath(path).split(os.sep)
            try:
                target = parts[-2]
                test_folder = parts[-3]
            except IndexError:
                target = "unknown"
                test_folder = "unknown"

            norm = _parse_performance_json(data, test_name_from_path=test_folder)
            if norm is None:
                continue

            key = (target, norm["test_name"])
            # Store as {runs, settings, metrics: {name: {value, unit}}}
            metrics_dict = {}
            for m in norm["metrics"]:
                metrics_dict[m["name"]] = {"value": m["value"], "unit": m.get("unit", "")}
            by_target_test[key].append({
                "runs": norm["runs"],
                "settings": norm.get("settings", ""),
                "metrics": metrics_dict,
            })

    # Build output: one entry per (target, test_name). Single file -> use its values; multiple -> weighted average.
    result = {}
    for key, samples in by_target_test.items():
        if not samples:
            continue
        total_runs = sum(s["runs"] for s in samples)
        settings = samples[0].get("settings", "")
        if len(samples) == 1:
            # Use the test's values directly (already averages)
            metrics_out = {}
            for mname, m in samples[0]["metrics"].items():
                metrics_out[mname] = {"avg": round(m["value"], 2), "unit": m.get("unit", "")}
        else:
            # Multiple files: weighted average per metric
            metrics_out = {}
            all_metric_names = set()
            for s in samples:
                all_metric_names.update(s["metrics"].keys())
            for mname in all_metric_names:
                value_sum = 0.0
                runs_sum = 0
                unit = ""
                for s in samples:
                    if mname not in s["metrics"]:
                        continue
                    m = s["metrics"][mname]
                    value_sum += m["value"] * s["runs"]
                    runs_sum += s["runs"]
                    if m.get("unit"):
                        unit = m["unit"]
                metrics_out[mname] = {"avg": round(value_sum / runs_sum, 2) if runs_sum else 0, "unit": unit}
        result[key] = {"runs": total_runs, "settings": settings, "metrics": metrics_out}

    return result


parser = argparse.ArgumentParser(description="Generate runtime test results report from JUnit/unity JSON and optional performance data.")
parser.add_argument("--results", required=True, help="Path to the unity_results.json file")
parser.add_argument("--sha", default=None, help="Commit SHA (falls back to GITHUB_SHA env var)")
parser.add_argument("--perf-dir", default=None, help="Path to directory containing performance result_*.json files")
args = parser.parse_args()

with open(args.results, "r") as f:
    data = json.load(f)
    tests = sorted(data["stats"]["suite_details"], key=lambda x: x["name"])

commit_sha = args.sha or os.environ.get("GITHUB_SHA")
performance_data_dir = args.perf_dir.rstrip(os.sep) if args.perf_dir else None
if not commit_sha:
    parser.error("Commit SHA is required: provide via --sha or set the GITHUB_SHA environment variable.")

# Generate the table

print("## Runtime Test Results")
print("")

try:
    if os.environ["IS_FAILING"] == "true":
        print(f"{FAILURE_SYMBOL} **The test workflows are failing. Please check the run logs.** {FAILURE_SYMBOL}")
        print("")
    else:
        print(f"{SUCCESS_SYMBOL} **The test workflows are passing.** {SUCCESS_SYMBOL}")
        print("")
except KeyError as e:
    logging.debug(f"IS_FAILING environment variable not set: {e}")

print("### Validation Tests")

# Read platform-specific target lists from environment variables
# Map env var names to test suite platform names: hw->hardware, wokwi->wokwi, qemu->qemu
platform_targets = {}
try:
    hw_targets = json.loads(os.environ.get("HW_TARGETS", "[]"))
    wokwi_targets = json.loads(os.environ.get("WOKWI_TARGETS", "[]"))
    qemu_targets = json.loads(os.environ.get("QEMU_TARGETS", "[]"))

    platform_targets["hardware"] = sorted(hw_targets) if hw_targets else []
    platform_targets["wokwi"] = sorted(wokwi_targets) if wokwi_targets else []
    platform_targets["qemu"] = sorted(qemu_targets) if qemu_targets else []
except (json.JSONDecodeError, KeyError) as e:
    print(f"Warning: Could not parse platform targets from environment: {e}", file=sys.stderr)
    platform_targets = {"hardware": [], "wokwi": [], "qemu": []}

proc_test_data = {}
perf_junit_status = {}  # (target, test_name) -> {"tests": int, "failures": int, "errors": int}

for test in tests:
    try:
        test_type, platform, target, rest = test["name"].split("_", 3)
    except ValueError as e:
        test_name = test.get("name", "unknown")
        logging.warning(f"Skipping test with unexpected name format '{test_name}': {e}")
        continue

    # Remove an optional trailing numeric index (multi-FQBN builds)
    m = re.match(r"(.+?)(\d+)?$", rest)
    test_name = m.group(1) if m else rest

    if test_type == "performance":
        key = (target, test_name)
        if key not in perf_junit_status:
            perf_junit_status[key] = {"tests": 0, "failures": 0, "errors": 0}
        perf_junit_status[key]["tests"] += test["tests"]
        perf_junit_status[key]["failures"] += test["failures"]
        perf_junit_status[key]["errors"] += test["errors"]
        continue

    if platform not in proc_test_data:
        proc_test_data[platform] = {}

    if test_name not in proc_test_data[platform]:
        proc_test_data[platform][test_name] = {}

    if target not in proc_test_data[platform][test_name]:
        proc_test_data[platform][test_name][target] = {
            "failures": 0,
            "total": 0,
            "errors": 0
        }

    proc_test_data[platform][test_name][target]["total"] += test["tests"]
    proc_test_data[platform][test_name][target]["failures"] += test["failures"]
    proc_test_data[platform][test_name][target]["errors"] += test["errors"]

# Render only executed tests grouped by platform/target/test
for platform in proc_test_data:
    print("")
    print(f"#### {platform.capitalize()}")
    print("")

    # Get platform-specific target list
    target_list = platform_targets.get(platform, [])

    if not target_list:
        print(f"No targets configured for platform: {platform}")
        continue

    print("Test", end="")

    for target in target_list:
        print(f"|{_display_target(target)}", end="")

    print("")
    print("-" + "|:-:" * len(target_list))

    platform_executed = proc_test_data.get(platform, {})
    for test_name in sorted(platform_executed.keys()):
        print(f"{test_name}", end="")
        for target in target_list:
            executed_cell = platform_executed.get(test_name, {}).get(target)
            if executed_cell:
                if executed_cell["errors"] > 0:
                    print(f"|Error {ERROR_SYMBOL}", end="")
                else:
                    print(f"|{executed_cell['total']-executed_cell['failures']}/{executed_cell['total']}", end="")
                    if executed_cell["failures"] > 0:
                        print(f" {FAILURE_SYMBOL}", end="")
                    else:
                        print(f" {SUCCESS_SYMBOL}", end="")
            else:
                print("|-", end="")
        print("")

# Performance Tests section (when performance_data_dir is provided)
by_target_test = {}
if performance_data_dir and os.path.isdir(performance_data_dir):
    by_target_test = _collect_and_aggregate_performance(performance_data_dir)

# Merge JUnit status with metric data. Also include tests that appear in
# JUnit results but have no result_*.json files (failed/errored before
# producing output).
tests_with_targets = defaultdict(dict)  # test_name -> { target: entry }
for (target, test_name), entry in by_target_test.items():
    tests_with_targets[test_name][target] = entry
for (target, test_name) in perf_junit_status:
    if target not in tests_with_targets.get(test_name, {}):
        tests_with_targets[test_name][target] = None

if tests_with_targets:
    print("")
    print("### Performance Tests")
    print("")

    for test_name in sorted(tests_with_targets.keys()):
        print(f"- **{test_name}**")
        for target in sorted(tests_with_targets[test_name].keys()):
            entry = tests_with_targets[test_name][target]
            status_label, status_symbol = _junit_status(perf_junit_status.get((target, test_name)))
            print(f"  - {_display_target(target)} - {status_label} - {status_symbol}")

            if entry:
                settings_str = entry.get("settings") or ""
                runs = entry["runs"]
                runs_line = f"{settings_str} - {runs} runs" if settings_str else f"{runs} runs"
                print(f"    - {runs_line}:")
                for mname in sorted(entry["metrics"].keys(), key=_natural_sort_key):
                    agg = entry["metrics"][mname]
                    avg = agg["avg"]
                    unit = agg.get("unit", "")
                    unit_str = f" {unit}" if unit else ""
                    print(f"      - {mname}: {avg}{unit_str}")
        print("")

print("\n")
print(f"Generated on: {datetime.now().strftime('%Y/%m/%d %H:%M:%S')}")
print("")

try:
    repo = os.environ['GITHUB_REPOSITORY']
    commit_url = f"https://github.com/{repo}/commit/{commit_sha}"
    build_workflow_url = f"https://github.com/{repo}/actions/runs/{os.environ['BUILD_RUN_ID']}"
    wokwi_hw_workflow_url = f"https://github.com/{repo}/actions/runs/{os.environ['WOKWI_RUN_ID']}"
    results_workflow_url = f"https://github.com/{repo}/actions/runs/{os.environ['RESULTS_RUN_ID']}"
    results_url = os.environ['RESULTS_URL']
    print(f"[Commit]({commit_url}) / [Build and QEMU run]({build_workflow_url}) / [Hardware and Wokwi run]({wokwi_hw_workflow_url}) / [Results processing]({results_workflow_url})")
    print("")
    print(f"[Test results]({results_url})")
except KeyError as e:
    logging.debug(f"Required environment variable for URL generation not set: {e}")

# Save test results to JSON file
results_data = {
    "commit_sha": commit_sha,
    "tests_failed": os.environ.get("IS_FAILING") == "true",
    "test_data": proc_test_data,
    "generated_at": datetime.now().isoformat()
}
if tests_with_targets:
    perf_out = {}
    for test_name, targets in tests_with_targets.items():
        for target, entry in targets.items():
            status_label, _ = _junit_status(perf_junit_status.get((target, test_name)))
            perf_entry = {"status": status_label.lower()}
            if entry:
                perf_entry.update({"runs": entry["runs"], "settings": entry["settings"], "metrics": entry["metrics"]})
            perf_out[f"{target}_{test_name}"] = perf_entry
    results_data["performance_data"] = perf_out

with open("test_results.json", "w") as f:
    json.dump(results_data, f, indent=2)

print(f"\nTest results saved to test_results.json", file=sys.stderr)
print(f"Commit SHA: {commit_sha}", file=sys.stderr)
print(f"Tests failed: {results_data['tests_failed']}", file=sys.stderr)
