#!/bin/bash

# Source centralized SoC configuration
SCRIPTS_DIR_CONFIG="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
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
        tmp_sketch_path=$(find tests -name "${sketches_to_build[0]}".ino)
        test_type=$(basename "$(dirname "$(dirname "$tmp_sketch_path")")")
        echo "Sketch ${sketches_to_build[0]} test type: $test_type"
        test_folder="$PWD/tests/$test_type"
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
        # Chunk build mode - build all sketches
        local_args=("${args[@]}")
        BUILD_CMD="${SCRIPTS_DIR}/sketch_utils.sh chunk_build"
        local_args+=("-p" "$test_folder" "-i" "0" "-m" "1" "-t" "$current_target")
        ${BUILD_CMD} "${local_args[@]}" "$@"
    else
        # Build specific sketches
        for current_sketch in "${sketches_to_build[@]}"; do
            echo "  Building sketch: $current_sketch"
            local_args=("${args[@]}")
            BUILD_CMD="${SCRIPTS_DIR}/sketch_utils.sh build"

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

            local_args+=("-s" "$sketch_test_folder/$current_sketch" "-t" "$current_target")
            ${BUILD_CMD} "${local_args[@]}" "$@"
        done
    fi
done
