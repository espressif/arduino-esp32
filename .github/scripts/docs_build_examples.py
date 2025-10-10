#!/usr/bin/env python3
"""
Python port of .github/scripts/docs_build_examples.sh
Preserves behavior and CLI options of the original bash script.

Usage: docs_build_examples.py -ai <arduino_cli_path> -au <arduino_user_path> [options]
"""

import argparse
from argparse import RawDescriptionHelpFormatter
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent

# Determine SDKCONFIG_DIR like the shell script
ARDUINO_ESP32_PATH = os.environ.get("ARDUINO_ESP32_PATH")
GITHUB_WORKSPACE = os.environ.get("GITHUB_WORKSPACE")

if ARDUINO_ESP32_PATH and (Path(ARDUINO_ESP32_PATH) / "tools" / "esp32-arduino-libs").is_dir():
    SDKCONFIG_DIR = Path(ARDUINO_ESP32_PATH) / "tools" / "esp32-arduino-libs"
elif GITHUB_WORKSPACE and (Path(GITHUB_WORKSPACE) / "tools" / "esp32-arduino-libs").is_dir():
    SDKCONFIG_DIR = Path(GITHUB_WORKSPACE) / "tools" / "esp32-arduino-libs"
else:
    SDKCONFIG_DIR = Path("tools/esp32-arduino-libs")

# Wrapper functions to call sketch_utils.sh
SKETCH_UTILS = SCRIPT_DIR / "sketch_utils.sh"

KEEP_FILES = ["*.merged.bin", "ci.json"]
DOCS_BINARIES_DIR = Path("docs/_static/binaries")
GENERATE_DIAGRAMS = False
LAUNCHPAD_STORAGE_URL = ""


def run_cmd(cmd, check=True, capture_output=False, text=True):
    try:
        return subprocess.run(cmd, check=check, capture_output=capture_output, text=text)
    except subprocess.CalledProcessError as e:
        # CalledProcessError is raised only when check=True and the command exits non-zero
        print(f"ERROR: Command failed: {' '.join(cmd)}")
        print(f"Exit code: {e.returncode}")
        if hasattr(e, 'stdout') and e.stdout:
            print("--- stdout ---")
            print(e.stdout)
        if hasattr(e, 'stderr') and e.stderr:
            print("--- stderr ---")
            print(e.stderr)
        # Exit the whole script with the same return code to mimic shell behavior
        sys.exit(e.returncode)
    except FileNotFoundError:
        print(f"ERROR: Command not found: {cmd[0]}")
        sys.exit(127)


def check_requirements(sketch_dir, sdkconfig_path):
    # Call sketch_utils.sh check_requirements
    cmd = [str(SKETCH_UTILS), "check_requirements", sketch_dir, str(sdkconfig_path)]
    try:
        res = run_cmd(cmd, check=False, capture_output=True)
        return res.returncode == 0
    except Exception:
        return False


def install_libs(*args):
    cmd = [str(SKETCH_UTILS), "install_libs"] + list(args)
    return run_cmd(cmd, check=False)


def build_sketch(args_list):
    cmd = [str(SKETCH_UTILS), "build"] + args_list
    return run_cmd(cmd, check=False)


def parse_args(argv):
    epilog_text = (
        "Example:\n"
        "  docs_build_examples.py -ai /usr/local/bin -au ~/.arduino15 -d -l https://storage.example.com\n\n"
        "This script finds Arduino sketches that include a 'ci.json' with an 'upload-binary'\n"
        "section and builds binaries for the listed targets. The built outputs are placed\n"
        "under docs/_static/binaries/<sketch_path>/<target>/\n"
    )

    p = argparse.ArgumentParser(
        description="Build examples that have ci.json with upload-binary targets",
        formatter_class=RawDescriptionHelpFormatter,
        epilog=epilog_text,
    )
    p.add_argument(
        "-ai",
        dest="arduino_cli_path",
        help=(
            "Path to Arduino CLI installation (directory containing the 'arduino-cli' binary)"
        ),
    )
    p.add_argument(
        "-au",
        dest="user_path",
        help="Arduino user path (for example: ~/.arduino15)",
    )
    p.add_argument(
        "-c",
        dest="cleanup",
        action="store_true",
        help="Clean up docs binaries directory and exit",
    )
    p.add_argument(
        "-d",
        dest="generate_diagrams",
        action="store_true",
        help="Generate diagrams for built examples using docs-embed",
    )
    p.add_argument(
        "-l",
        dest="launchpad_storage_url",
        help="LaunchPad storage URL to include in generated config",
    )
    return p.parse_args(argv)


def validate_prerequisites(args):
    if not args.arduino_cli_path:
        print("ERROR: Arduino CLI path not provided (-ai option required)")
        sys.exit(1)
    if not args.user_path:
        print("ERROR: Arduino user path not provided (-au option required)")
        sys.exit(1)
    arduino_cli_exe = Path(args.arduino_cli_path) / "arduino-cli"
    if not arduino_cli_exe.exists():
        print(f"ERROR: arduino-cli not found at {arduino_cli_exe}")
        sys.exit(1)
    if not Path(args.user_path).is_dir():
        print(f"ERROR: Arduino user path does not exist: {args.user_path}")
        sys.exit(1)


def cleanup_binaries():
    print(f"Cleaning up binaries directory: {DOCS_BINARIES_DIR}")
    if not DOCS_BINARIES_DIR.exists():
        print("Binaries directory does not exist, nothing to clean")
        return
    for root, dirs, files in os.walk(DOCS_BINARIES_DIR):
        for fname in files:
            fpath = Path(root) / fname
            parent = Path(root).name
            # Always remove sketch/ci.json
            if parent == "sketch" and fname == "ci.json":
                fpath.unlink()
                continue
            keep = False
            for pattern in KEEP_FILES:
                if Path(fname).match(pattern):
                    keep = True
                    break
            if not keep:
                print(f"Removing: {fpath}")
                fpath.unlink()
            else:
                print(f"Keeping: {fpath}")
    # remove empty dirs
    for root, dirs, files in os.walk(DOCS_BINARIES_DIR, topdown=False):
        if not os.listdir(root):
            try:
                os.rmdir(root)
            except Exception:
                pass
    print("Cleanup completed")


def find_examples_with_upload_binary():
    res = []
    for ino in Path('.').rglob('*.ino'):
        sketch_dir = ino.parent
        sketch_name = ino.stem
        dir_name = sketch_dir.name
        if dir_name != sketch_name:
            continue
        ci_json = sketch_dir / 'ci.json'
        if ci_json.exists():
            try:
                data = json.loads(ci_json.read_text())
                if 'upload-binary' in data and data['upload-binary']:
                    res.append(str(ino))
            except Exception:
                continue
    return res


def get_upload_binary_targets(sketch_dir):
    ci_json = Path(sketch_dir) / 'ci.json'
    try:
        data = json.loads(ci_json.read_text())
        targets = data.get('upload-binary', {}).get('targets', [])
        return targets
    except Exception:
        return []


def build_example_for_target(sketch_dir, target, relative_path, args):
    print(f"\n > Building example: {relative_path} for target: {target}")
    output_dir = DOCS_BINARIES_DIR / relative_path / target
    output_dir.mkdir(parents=True, exist_ok=True)

    sdkconfig = SDKCONFIG_DIR / target / 'sdkconfig'
    if not check_requirements(str(sketch_dir), sdkconfig):
        print(f"Target {target} does not meet the requirements for {Path(sketch_dir).name}. Skipping.")
        return True

    # Build the sketch using sketch_utils.sh build - pass args as in shell script
    build_args = [
        "-ai",
        args.arduino_cli_path,
        "-au",
        args.user_path,
        "-s",
        str(sketch_dir),
        "-t",
        target,
        "-b",
        str(output_dir),
        "--first-only",
    ]
    res = build_sketch(build_args)
    if res.returncode == 0:
        print(f"Successfully built {relative_path} for {target}")
        ci_json = Path(sketch_dir) / 'ci.json'
        if ci_json.exists():
            shutil.copy(ci_json, output_dir / 'ci.json')
        if GENERATE_DIAGRAMS:
            print(f"Generating diagram for {relative_path} ({target})...")
            rc = run_cmd(
                [
                    "docs-embed",
                    "--path",
                    str(output_dir),
                    "diagram-from-ci",
                    "--platform",
                    target,
                    "--override",
                ],
                check=False,
            )
            if rc.returncode == 0:
                print("Diagram generated successfully for {relative_path} ({target})")
            else:
                print("WARNING: Failed to generate diagram for {relative_path} ({target})")
        if LAUNCHPAD_STORAGE_URL:
            print(f"Generating LaunchPad config for {relative_path} ({target})...")
            rc = run_cmd(
                [
                    "docs-embed",
                    "--path",
                    str(output_dir),
                    "launchpad-config",
                    LAUNCHPAD_STORAGE_URL,
                    "--repo-url-prefix",
                    "https://github.com/espressif/arduino-esp32/tree/master",
                    "--override",
                ],
                check=False,
            )
            if rc.returncode == 0:
                print("LaunchPad config generated successfully for {relative_path} ({target})")
            else:
                print("WARNING: Failed to generate LaunchPad config for {relative_path} ({target})")
        return True
    else:
        print(f"ERROR: Failed to build {relative_path} for {target}")
        return False


def build_all_examples(args):
    total_built = 0
    total_failed = 0

    if DOCS_BINARIES_DIR.exists():
        shutil.rmtree(DOCS_BINARIES_DIR)
        print(f"Removed existing build directory: {DOCS_BINARIES_DIR}")

    examples = find_examples_with_upload_binary()
    if not examples:
        print("No examples found with upload-binary configuration")
        return 0

    print('\nExamples to be built:')
    print('====================')
    for i, example in enumerate(examples, start=1):
        sketch_dir = Path(example).parent
        relative_path = str(sketch_dir).lstrip('./')
        targets = get_upload_binary_targets(sketch_dir)
        if targets:
            print(f"{i}. {relative_path} (targets: {' '.join(targets)})")
    print()

    for example in examples:
        sketch_dir = Path(example).parent
        relative_path = str(sketch_dir).lstrip('./')
        targets = get_upload_binary_targets(sketch_dir)
        if not targets:
            print(f"WARNING: No targets found for {relative_path}")
            continue
        print(f"Building {relative_path} for targets: {targets}")
        for target in targets:
            ok = build_example_for_target(sketch_dir, target, relative_path, args)
            if ok:
                total_built += 1
            else:
                total_failed += 1

    print('\nBuild summary:')
    print(f"  Successfully built: {total_built}")
    print(f"  Failed builds: {total_failed}")
    print(f"  Output directory: {DOCS_BINARIES_DIR}")
    return total_failed


def main(argv):
    global GENERATE_DIAGRAMS, LAUNCHPAD_STORAGE_URL
    args = parse_args(argv)
    if args.cleanup:
        cleanup_binaries()
        return
    validate_prerequisites(args)
    GENERATE_DIAGRAMS = args.generate_diagrams
    LAUNCHPAD_STORAGE_URL = args.launchpad_storage_url or ""
    DOCS_BINARIES_DIR.mkdir(parents=True, exist_ok=True)
    result = build_all_examples(args)
    if result == 0:
        print('\nAll examples built successfully!')
    else:
        print('\nSome builds failed. Check the output above for details.')
        sys.exit(1)


if __name__ == '__main__':
    main(sys.argv[1:])
