import json
import sys
import os
from datetime import datetime

SUCCESS_SYMBOL = ":white_check_mark:"
FAILURE_SYMBOL = ":x:"
ERROR_SYMBOL = ":fire:"

# Load the JSON file passed as argument to the script
with open(sys.argv[1], "r") as f:
    data = json.load(f)
    tests = sorted(data["stats"]["suite_details"], key=lambda x: x["name"])

# Get commit SHA from command line argument or environment variable
commit_sha = None
if len(sys.argv) < 2 or len(sys.argv) > 3:
    print("Usage: python table_generator.py <test_results.json> [commit_sha]", file=sys.stderr)
    sys.exit(1)
elif len(sys.argv) == 3: # Commit SHA is provided as argument
    commit_sha = sys.argv[2]
elif "GITHUB_SHA" in os.environ: # Commit SHA is provided as environment variable
    commit_sha = os.environ["GITHUB_SHA"]
else: # Commit SHA is not provided
    print("Commit SHA is not provided. Please provide it as an argument or set the GITHUB_SHA environment variable.", file=sys.stderr)
    sys.exit(1)

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
except KeyError:
    pass

print("### Validation Tests")

proc_test_data = {}
target_list = []

for test in tests:
    if test["name"].startswith("performance_"):
        continue

    _, platform, target, test_name = test["name"].split("_", 3)
    test_name = test_name[:-1]

    if target not in target_list:
        target_list.append(target)

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

target_list = sorted(target_list)

for platform in proc_test_data:
    print("")
    print(f"#### {platform.capitalize()}")
    print("")
    print("Test", end="")

    for target in target_list:
        # Make target name uppercase and add hyfen if not esp32
        if target != "esp32":
            target = target.replace("esp32", "esp32-")

        print(f"|{target.upper()}", end="")

    print("")
    print("-" + "|:-:" * len(target_list))

    for test_name, targets in proc_test_data[platform].items():
        print(f"{test_name}", end="")
        for target in target_list:
            if target in targets:
                test_data = targets[target]
                if test_data["errors"] > 0:
                    print(f"|Error {ERROR_SYMBOL}", end="")
                else:
                    print(f"|{test_data['total']-test_data['failures']}/{test_data['total']}", end="")
                    if test_data["failures"] > 0:
                        print(f" {FAILURE_SYMBOL}", end="")
                    else:
                        print(f" {SUCCESS_SYMBOL}", end="")
            else:
                print("|-", end="")
        print("")

print("\n")
print(f"Generated on: {datetime.now().strftime('%Y/%m/%d %H:%M:%S')}")
print("")

try:
    print(f"[Commit](https://github.com/{os.environ['GITHUB_REPOSITORY']}/commit/{commit_sha}) / [Build, Hardware and QEMU run](https://github.com/{os.environ['GITHUB_REPOSITORY']}/actions/runs/{os.environ['BUILD_RUN_ID']}) / [Wokwi run](https://github.com/{os.environ['GITHUB_REPOSITORY']}/actions/runs/{os.environ['WOKWI_RUN_ID']})")
except KeyError:
    pass

# Save test results to JSON file
results_data = {
    "commit_sha": commit_sha,
    "tests_failed": os.environ["IS_FAILING"] == "true",
    "test_data": proc_test_data,
    "generated_at": datetime.now().isoformat()
}

with open("test_results.json", "w") as f:
    json.dump(results_data, f, indent=2)

print(f"\nTest results saved to test_results.json", file=sys.stderr)
print(f"Commit SHA: {commit_sha}", file=sys.stderr)
print(f"Tests failed: {results_data['tests_failed']}", file=sys.stderr)
