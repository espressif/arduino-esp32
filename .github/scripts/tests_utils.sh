#!/bin/bash

# Shared utility functions for test scripts

# Detect test type and folder from sketch name
# This function handles both multi-device tests (which have ci.yml at test level)
# and regular tests (which have .ino files in sketch directories)
#
# Usage: detect_test_type_and_folder "sketch_name"
# Returns: Sets global variables 'test_type' and 'test_folder'
# Exits with error if sketch is not found
function detect_test_type_and_folder {
    local sketch=$1

    # shellcheck disable=SC2034  # test_type and test_folder are used by caller
    # For multi-device tests, we need to find the test directory, not the device sketch directory
    # First, try to find a test directory with this name
    if [ -d "tests/validation/$sketch" ] && [ -f "tests/validation/$sketch/ci.yml" ]; then
        test_type="validation"
        test_folder="$PWD/tests/$test_type"
    elif [ -d "tests/performance/$sketch" ] && [ -f "tests/performance/$sketch/ci.yml" ]; then
        test_type="performance"
        test_folder="$PWD/tests/$test_type"
    else
        # Fall back to finding by .ino file (for regular tests)
        tmp_sketch_path=$(find tests -name "$sketch".ino | head -1)
        if [ -z "$tmp_sketch_path" ]; then
            echo "ERROR: Sketch $sketch not found"
            return 1
        fi
        test_type=$(basename "$(dirname "$(dirname "$tmp_sketch_path")")")
        test_folder="$PWD/tests/$test_type"
    fi
    echo "Sketch $sketch test type: $test_type"
    return 0
}

# Ensure the Go implementation of yq (mikefarah/yq, aka "yq-go") is installed.
#
# The test scripts parse ci.yml with `yq eval '...'`, which is mikefarah/yq v4
# syntax. The unrelated Python "yq" (a jq wrapper) does not understand it, so a
# missing or wrong yq surfaces as a misleading "ci.yml is not valid YAML" error.
# Fail early here with an actionable message instead.
#
# Usage: require_yq_go || exit 1
function require_yq_go {
    if ! command -v yq >/dev/null 2>&1; then
        echo "ERROR: 'yq' is required but not installed."
        echo "       Install the Go implementation (mikefarah/yq, aka yq-go):"
        echo "         - Linux:  sudo snap install yq   (or download from the releases page)"
        echo "         - macOS:  brew install yq"
        echo "         - Go:     go install github.com/mikefarah/yq/v4@latest"
        echo "       See https://github.com/mikefarah/yq/#install"
        return 1
    fi

    # Distinguish mikefarah/yq from the Python 'yq'. The former mentions its
    # project URL / name in --version and supports the 'yq eval' subcommand.
    local yq_version
    yq_version=$(yq --version 2>&1)
    if ! echo "$yq_version" | grep -qi 'mikefarah'; then
        echo "ERROR: The installed 'yq' is not the Go implementation (mikefarah/yq, aka yq-go)."
        echo "       The test scripts use 'yq eval' syntax, which the Python 'yq' does not support."
        echo "       Detected: ${yq_version}"
        echo "       Install mikefarah/yq: https://github.com/mikefarah/yq/#install"
        return 1
    fi

    return 0
}

