#!/bin/bash

# Source centralized SoC configuration
SCRIPTS_DIR_CONFIG="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKETCH_UTILS="${SCRIPTS_DIR_CONFIG}/sketch_utils.sh"
source "${SCRIPTS_DIR_CONFIG}/socs_config.sh"

USAGE="
USAGE:
    ${0} -c -type <test_type> <chunk_build_opts>
        Example: ${0} -c -type validation -t esp32 -i 0 -m 15
    ${0} -s sketch_name <build_opts>
        Example: ${0} -s hello_world -t esp32
    ${0} -s sketch1,sketch2 -t target1,target2
        Example: ${0} -s hello_world,gpio -t esp32,esp32s3
        Note: Comma-separated lists are supported for both targets and sketches
    ${0} -clean
        Remove build and test generated files

    If no -t target is specified, builds for all BUILD_TEST_TARGETS
    If no -s sketch is specified, builds all sketches (chunk mode)
"

function clean {
    rm -rf tests/.pytest_cache
    find tests/ -type d -name 'build*' -exec rm -rf "{}" \+
    find tests/ -type d -name '__pycache__' -exec rm -rf "{}" \+
    find tests/ -name '*.xml' -exec rm -rf "{}" \+
    find tests/ -name 'result_*.json' -exec rm -rf "{}" \+
}

# Check if a test is a multi-device test
function is_multi_device_test {
    local test_dir=$1
    local has_multi_device
    if [ -f "$test_dir/ci.yml" ]; then
        has_multi_device=$(yq eval '.multi_device' "$test_dir/ci.yml" 2>/dev/null)
        if [[ "$has_multi_device" != "null" && "$has_multi_device" != "" ]]; then
            echo "1"
            return
        fi
    fi
    echo "0"
}

# Extract target from arguments and return target and remaining arguments
# Usage: extract_target_and_args "$@"
# Returns: Sets global variables 'target' and 'remaining_args' array
function extract_target_and_args {
    target=""
    remaining_args=()
    while [ -n "$1" ]; do
        case $1 in
        -t )
            shift
            target=$1
            remaining_args+=("-t" "$target")
            ;;
        * )
            remaining_args+=("$1")
            ;;
        esac
        shift
    done
}

# Build a single sketch for multi-device tests
function build_multi_device_sketch {
    local test_name=$1
    local sketch_path=$2
    local target=$3
    shift 3
    local build_args=("$@")
    local sketch_dir
    local sketch_name
    local test_dir
    local build_dir

    sketch_dir=$(dirname "$sketch_path")
    sketch_name=$(basename "$sketch_dir")
    test_dir=$(dirname "$sketch_dir")

    # Override build directory to use <test_name>_<sketch_name> pattern
    build_dir="$HOME/.arduino/tests/$target/${test_name}_${sketch_name}/build.tmp"

    echo "Building sketch $sketch_name for multi-device test $test_name"

    # Call sketch_utils.sh build function with custom build directory
    ARDUINO_BUILD_DIR="$build_dir" ${SKETCH_UTILS} build "${build_args[@]}" -s "$sketch_dir"
    return $?
}

# Build all sketches for a multi-device test
function build_multi_device_test {
    local test_dir=$1
    local target=$2
    shift 2
    local build_args=("$@")
    local test_name
    local devices

    test_name=$(basename "$test_dir")

    # Check if target is supported by this test
    if [ -f "$test_dir/ci.yml" ]; then
        # Check if target is explicitly disabled
        is_target=$(yq eval ".targets.${target}" "$test_dir/ci.yml" 2>/dev/null)
        if [[ "$is_target" == "false" ]]; then
            echo "Skipping multi-device test $test_name for $target (explicitly disabled)"
            return 0
        fi

        # Check if target meets the requirements using check_requirements from sketch_utils.sh
        has_requirements=$(${SKETCH_UTILS} check_requirements "$test_dir" "tools/esp32-arduino-libs/$target/sdkconfig")
        if [ "$has_requirements" == "0" ]; then
            echo "Skipping multi-device test $test_name for $target (requirements not met)"
            return 0
        fi
    fi

    echo "Building multi-device test $test_name"

    # Get the list of devices from ci.yml
    devices=$(yq eval '.multi_device | keys | .[]' "$test_dir/ci.yml" 2>/dev/null)

    if [[ -z "$devices" ]]; then
        echo "ERROR: No devices found in multi_device configuration for $test_name"
        return 1
    fi

    local result=0
    local sketch_name
    local sketch_path
    for device in $devices; do
        sketch_name=$(yq eval ".multi_device.$device" "$test_dir/ci.yml" 2>/dev/null)
        sketch_path="$test_dir/$sketch_name/$sketch_name.ino"

        if [ ! -f "$sketch_path" ]; then
            echo "ERROR: Sketch not found: $sketch_path"
            return 1
        fi

        build_multi_device_sketch "$test_name" "$sketch_path" "$target" "${build_args[@]}"
        result=$?
        if [ $result -ne 0 ]; then
            echo "ERROR: Failed to build sketch $sketch_name for test $test_name"
            return $result
        fi
    done

    return 0
}

SCRIPTS_DIR="./.github/scripts"
BUILD_CMD=""

chunk_build=0

while [ -n "$1" ]; do
    case $1 in
    -c )
        chunk_build=1
        ;;
    -s )
        shift
        sketch=$1
        ;;
    -t )
        shift
        target=$1
        ;;
    -h )
        echo "$USAGE"
        exit 0
        ;;
    -type )
        shift
        test_type=$1
        ;;
    -clean )
        clean
        exit 0
        ;;
    * )
        break
        ;;
    esac
    shift
done

set -e
source "${SCRIPTS_DIR}/install-arduino-cli.sh"
source "${SCRIPTS_DIR}/install-arduino-core-esp32.sh"
source "${SCRIPTS_DIR}/tests_utils.sh"
set +e

args=("-ai" "$ARDUINO_IDE_PATH" "-au" "$ARDUINO_USR_PATH")

# Parse comma-separated targets
if [ -n "$target" ]; then
    IFS=',' read -ra targets_to_build <<< "$target"
fi

# Parse comma-separated sketches
if [ -n "$sketch" ]; then
    IFS=',' read -ra sketches_to_build <<< "$sketch"
fi

# Handle default target and sketch logic
if [ -z "$target" ] && [ -z "$sketch" ]; then
    # No target or sketch specified - build all sketches for all targets
    echo "No target or sketch specified, building all sketches for all targets"
    chunk_build=1
    targets_to_build=("${BUILD_TEST_TARGETS[@]}")
    sketches_to_build=()
elif [ -z "$target" ]; then
    # No target specified, but sketch(es) specified - build sketches for all targets
    echo "No target specified, building sketch(es) '${sketches_to_build[*]}' for all targets"
    targets_to_build=("${BUILD_TEST_TARGETS[@]}")
elif [ -z "$sketch" ]; then
    # No sketch specified, but target(s) specified - build all sketches for targets
    echo "No sketch specified, building all sketches for target(s) '${targets_to_build[*]}'"
    chunk_build=1
    sketches_to_build=()
fi

if [[ $test_type == "all" ]] || [[ -z $test_type ]]; then
    if [ ${#sketches_to_build[@]} -eq 1 ]; then
        detect_test_type_and_folder "$sketch"
    else
        test_folder="$PWD/tests"
    fi
else
    test_folder="$PWD/tests/$test_type"
fi

# Loop through all targets to build
for current_target in "${targets_to_build[@]}"; do
    echo "Building for target: $current_target"

    if [ $chunk_build -eq 1 ]; then
        # For chunk builds, we need to handle multi-device tests separately
        # First, build all multi-device tests, then build regular tests

        # Find and build all multi-device tests in the test folder
        multi_device_error=0
        if [ -d "$test_folder" ]; then
            for test_dir in "$test_folder"/*; do
                if [ -d "$test_dir" ] && [ "$(is_multi_device_test "$test_dir")" -eq 1 ]; then
                    build_multi_device_test "$test_dir" "$current_target" "${args[@]}" "-t" "$current_target" "$@"
                    result=$?
                    if [ $result -ne 0 ]; then
                        multi_device_error=$result
                    fi
                fi
            done
        fi

        # Now build regular (non-multi-device) tests using chunk_build
        local_args=("${args[@]}")
        BUILD_CMD="${SKETCH_UTILS} chunk_build"
        local_args+=("-p" "$test_folder" "-i" "0" "-m" "1" "-t" "$current_target")
        ${BUILD_CMD} "${local_args[@]}" "$@"
        regular_error=$?

        # Return error if either multi-device or regular builds failed
        if [ $multi_device_error -ne 0 ]; then
            exit $multi_device_error
        fi
        if [ $regular_error -ne 0 ]; then
            exit $regular_error
        fi
    else
        # Build specific sketches
        for current_sketch in "${sketches_to_build[@]}"; do
            echo "  Building sketch: $current_sketch"

            # Find test folder for this sketch if needed
            if [[ $test_type == "all" ]] || [[ -z $test_type ]]; then
                tmp_sketch_path=$(find tests -name "$current_sketch".ino)
                if [ -z "$tmp_sketch_path" ]; then
                    echo "ERROR: Sketch $current_sketch not found"
                    continue
                fi
                sketch_test_type=$(basename "$(dirname "$(dirname "$tmp_sketch_path")")")
                sketch_test_folder="$PWD/tests/$sketch_test_type"
            else
                sketch_test_folder="$test_folder"
            fi

            # Check if this is a multi-device test
            test_dir="$sketch_test_folder/$current_sketch"
            if [ -d "$test_dir" ] && [ "$(is_multi_device_test "$test_dir")" -eq 1 ]; then
                build_multi_device_test "$test_dir" "$current_target" "${args[@]}" "-t" "$current_target" "$@"
            else
                local_args=("${args[@]}")
                BUILD_CMD="${SCRIPTS_DIR}/sketch_utils.sh build"
                local_args+=("-s" "$sketch_test_folder/$current_sketch" "-t" "$current_target")
                ${BUILD_CMD} "${local_args[@]}" "$@"
            fi
        done
    fi
done
