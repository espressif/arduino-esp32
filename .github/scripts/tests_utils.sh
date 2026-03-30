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
            exit 1
        fi
        test_type=$(basename "$(dirname "$(dirname "$tmp_sketch_path")")")
        test_folder="$PWD/tests/$test_type"
    fi
    echo "Sketch $sketch test type: $test_type"
}

