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
    ${0} -s ble -t esp32s31|esp32c6
        Mixed-target multi-DUT: device0 built for esp32s31, device1 for esp32c6.
        Binaries stay under ~/.arduino/tests/<soc>/... (same as comma builds).
        Pipe is only valid for multi-device tests.
    ${0} -clean
        Remove build and test generated files

    Options:
        --arduino-cli   Use arduino-cli for compilation instead of arduino_cmake.py

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

# Strip -t and its value from an argument list (stdout: remaining args as lines)
function filter_out_target_args {
    local skip_next=0
    for arg in "$@"; do
        if [ $skip_next -eq 1 ]; then
            skip_next=0
            continue
        fi
        if [ "$arg" == "-t" ]; then
            skip_next=1
            continue
        fi
        printf '%s\n' "$arg"
    done
}

# Build all sketches for a multi-device test.
# $2 may be a single SoC (same-target) or pipe-separated SoCs (mixed-target: device0|device1|...).
# Artifacts always land under ~/.arduino/tests/<soc>/<test>/<sketch>/build.tmp.
function build_multi_device_test {
    local test_dir=$1
    local target=$2
    shift 2
    local build_args=("$@")
    local test_name
    local devices
    local dut_targets=()
    local device_count=0
    local is_mixed=0

    test_name=$(basename "$test_dir")

    IFS='|' read -ra dut_targets <<< "$target"
    if [ ${#dut_targets[@]} -gt 1 ]; then
        is_mixed=1
    fi

    # Get the list of devices from ci.yml (sorted to match run_multi_device_test / dut index)
    devices=$(yq eval '.multi_device | keys | .[]' "$test_dir/ci.yml" 2>/dev/null | sort)

    if [[ -z "$devices" ]]; then
        echo "ERROR: No devices found in multi_device configuration for $test_name"
        return 1
    fi

    for device in $devices; do
        device_count=$((device_count + 1))
    done

    if [ $is_mixed -eq 1 ] && [ ${#dut_targets[@]} -ne "$device_count" ]; then
        echo "ERROR: Mixed target '$target' has ${#dut_targets[@]} SoCs but $test_name has $device_count devices"
        return 1
    fi

    # Same-target: duplicate the single SoC for every device
    if [ $is_mixed -eq 0 ]; then
        local single="${dut_targets[0]}"
        dut_targets=()
        for ((i=0; i<device_count; i++)); do
            dut_targets+=("$single")
        done
    fi

    # Per-DUT: skip entire mixed/same job if any SoC is disabled or fails requirements
    if [ -f "$test_dir/ci.yml" ]; then
        local soc
        for soc in "${dut_targets[@]}"; do
            local is_target
            is_target=$(yq eval ".targets.${soc}" "$test_dir/ci.yml" 2>/dev/null)
            if [[ "$is_target" == "false" ]]; then
                echo "Skipping multi-device test $test_name for $target (explicitly disabled for $soc)"
                return 0
            fi

            local has_requirements
            has_requirements=$(${SKETCH_UTILS} check_requirements "$test_dir" "tools/esp32-arduino-libs/$soc/sdkconfig")
            if [ "$has_requirements" == "0" ]; then
                echo "Skipping multi-device test $test_name for $target (requirements not met for $soc)"
                return 0
            fi
        done
    fi

    if [ $is_mixed -eq 1 ]; then
        echo "Building multi-device test $test_name (mixed targets: $target)"
    else
        echo "Building multi-device test $test_name"
    fi

    # Drop any -t from caller args; we set -t per device below
    local filtered_args=()
    while IFS= read -r line; do
        [ -n "$line" ] && filtered_args+=("$line")
    done < <(filter_out_target_args "${build_args[@]}")

    local result=0
    local sketch_name
    local sketch_path
    local sketch_dir
    local device_fqbn_append
    local device_target
    local dev_idx=0
    for device in $devices; do
        device_target="${dut_targets[$dev_idx]}"

        # multi_device values can be either a scalar (sketch name) or a map
        # with "sketch" and optional "fqbn_append" keys.
        local device_type
        device_type=$(yq eval ".multi_device.$device | type" "$test_dir/ci.yml" 2>/dev/null)

        if [ "$device_type" == "!!map" ]; then
            sketch_name=$(yq eval ".multi_device.$device.sketch" "$test_dir/ci.yml" 2>/dev/null)
            device_fqbn_append=$(yq eval ".multi_device.$device.fqbn_append // \"\"" "$test_dir/ci.yml" 2>/dev/null)
        else
            sketch_name=$(yq eval ".multi_device.$device" "$test_dir/ci.yml" 2>/dev/null)
            device_fqbn_append=""
        fi

        sketch_path="$test_dir/$sketch_name/$sketch_name.ino"
        sketch_dir="$test_dir/$sketch_name"

        if [ ! -f "$sketch_path" ]; then
            echo "ERROR: Sketch not found: $sketch_path"
            return 1
        fi

        echo "Building sketch $sketch_name for multi-device test $test_name (target: $device_target)"

        # -td: ci.yml lives in the parent test directory
        # -bn: sets the parent build dir to the test name (nested: $test_name/$sketch_name/build.tmp)
        local fa_args=()
        if [ -n "$device_fqbn_append" ]; then
            fa_args=(-fa "$device_fqbn_append")
        fi

        ${SKETCH_UTILS} build "${filtered_args[@]}" -t "$device_target" -s "$sketch_dir" \
            -td "$test_dir" -bn "$test_name" "${fa_args[@]}"
        result=$?
        if [ $result -ne 0 ]; then
            echo "ERROR: Failed to build sketch $sketch_name for test $test_name (target: $device_target)"
            return $result
        fi
        dev_idx=$((dev_idx + 1))
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
    --arduino-cli )
        use_arduino_cli=1
        ;;
    * )
        break
        ;;
    esac
    shift
done

set -e
source "${SCRIPTS_DIR}/env.sh"
if [ "${use_arduino_cli:-0}" -eq 1 ]; then
    source "${SCRIPTS_DIR}/install-arduino-cli.sh"
fi
source "${SCRIPTS_DIR}/install-arduino-core-esp32.sh"
source "${SCRIPTS_DIR}/tests_utils.sh"
set +e

args=("-au" "$ARDUINO_USR_PATH")
if [ "${use_arduino_cli:-0}" -eq 1 ]; then
    args+=("-ai" "$ARDUINO_IDE_PATH" "--arduino-cli")
fi

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
        detect_test_type_and_folder "$sketch" || exit 1
    else
        test_folder="$PWD/tests"
    fi
else
    test_folder="$PWD/tests/$test_type"
fi

# Loop through all targets to build (each entry may be a SoC or a pipe-separated mixed pair)
for current_target in "${targets_to_build[@]}"; do
    echo "Building for target: $current_target"
    is_mixed_target=0
    if [[ "$current_target" == *"|"* ]]; then
        is_mixed_target=1
    fi

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

        # Mixed-target jobs only apply to multi-device tests
        if [ $is_mixed_target -eq 1 ]; then
            if [ $multi_device_error -ne 0 ]; then
                exit $multi_device_error
            fi
            continue
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
                if ! detect_test_type_and_folder "$current_sketch"; then
                    continue
                fi
                sketch_test_folder="$test_folder"
            else
                sketch_test_folder="$test_folder"
            fi

            # Check if this is a multi-device test
            test_dir="$sketch_test_folder/$current_sketch"
            if [ -d "$test_dir" ] && [ "$(is_multi_device_test "$test_dir")" -eq 1 ]; then
                build_multi_device_test "$test_dir" "$current_target" "${args[@]}" "-t" "$current_target" "$@"
            elif [ $is_mixed_target -eq 1 ]; then
                echo "ERROR: Mixed target '$current_target' is only valid for multi-device tests (got: $current_sketch)"
                exit 1
            else
                local_args=("${args[@]}")
                BUILD_CMD="${SCRIPTS_DIR}/sketch_utils.sh build"
                local_args+=("-s" "$sketch_test_folder/$current_sketch" "-t" "$current_target")
                ${BUILD_CMD} "${local_args[@]}" "$@"
            fi
        done
    fi
done
