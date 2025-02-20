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

# Generate the table

print("## Runtime Tests Report")
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
print("")

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
    print(f"[Build, Hardware and QEMU run](https://github.com/{os.environ['GITHUB_REPOSITORY']}/actions/runs/{os.environ['BUILD_RUN_ID']}) / [Wokwi run](https://github.com/{os.environ['GITHUB_REPOSITORY']}/actions/runs/{os.environ['WOKWI_RUN_ID']})")
except KeyError:
    pass
