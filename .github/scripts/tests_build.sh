#!/bin/bash

USAGE="
USAGE:
    ${0} -c -type <test_type> <chunk_build_opts>
        Example: ${0} -c -type validation -t esp32 -i 0 -m 15
    ${0} -s sketch_name <build_opts>
        Example: ${0} -s hello_world -t esp32
    ${0} -clean
        Remove build and test generated files
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
    ARDUINO_BUILD_DIR="$build_dir" ${SCRIPTS_DIR}/sketch_utils.sh build "${build_args[@]}" -s "$sketch_dir"
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

source "${SCRIPTS_DIR}/install-arduino-cli.sh"
source "${SCRIPTS_DIR}/install-arduino-core-esp32.sh"
source "${SCRIPTS_DIR}/tests_utils.sh"

args=("-ai" "$ARDUINO_IDE_PATH" "-au" "$ARDUINO_USR_PATH")

if [[ $test_type == "all" ]] || [[ -z $test_type ]]; then
    if [ -n "$sketch" ]; then
        detect_test_type_and_folder "$sketch"
    else
        test_folder="$PWD/tests"
    fi
else
    test_folder="$PWD/tests/$test_type"
fi

if [ $chunk_build -eq 1 ]; then
    # For chunk builds, we need to handle multi-device tests separately
    # First, build all multi-device tests, then build regular tests

    # Extract target from remaining arguments
    extract_target_and_args "$@"

    if [ -z "$target" ]; then
        echo "ERROR: Target (-t) is required for chunk builds"
        exit 1
    fi

    # Find and build all multi-device tests in the test folder
    multi_device_error=0
    if [ -d "$test_folder" ]; then
        for test_dir in "$test_folder"/*; do
            if [ -d "$test_dir" ] && [ "$(is_multi_device_test "$test_dir")" -eq 1 ]; then
                build_multi_device_test "$test_dir" "$target" "${args[@]}" "${remaining_args[@]}"
                result=$?
                if [ $result -ne 0 ]; then
                    multi_device_error=$result
                fi
            fi
        done
    fi

    # Now build regular (non-multi-device) tests using chunk_build
    BUILD_CMD="${SCRIPTS_DIR}/sketch_utils.sh chunk_build"
    args+=("-p" "$test_folder")
    ${BUILD_CMD} "${args[@]}" "${remaining_args[@]}"
    regular_error=$?

    # Return error if either multi-device or regular builds failed
    if [ $multi_device_error -ne 0 ]; then
        exit $multi_device_error
    fi
    exit $regular_error
else
    # Check if this is a multi-device test
    test_dir="$test_folder/$sketch"
    if [ -d "$test_dir" ] && [ "$(is_multi_device_test "$test_dir")" -eq 1 ]; then
        # Extract target from remaining arguments
        extract_target_and_args "$@"

        if [ -z "$target" ]; then
            echo "ERROR: Target (-t) is required for multi-device tests"
            exit 1
        fi

        build_multi_device_test "$test_dir" "$target" "${args[@]}" "${remaining_args[@]}"
    else
        BUILD_CMD="${SCRIPTS_DIR}/sketch_utils.sh build"
        args+=("-s" "$test_folder/$sketch")
        ${BUILD_CMD} "${args[@]}" "$@"
    fi
fi
