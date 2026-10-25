#!/bin/bash

set -e

# Source centralized SoC configuration
SCRIPTS_DIR="./.github/scripts"
source "${SCRIPTS_DIR}/socs_config.sh"

# Linux SoC-outer jobs set BUILD_TARGET to one CORE_SOCS id.
if [ -n "${BUILD_TARGET:-}" ]; then
    CORE_SOCS=("$BUILD_TARGET")
fi

export ARDUINO_BUILD_DIR="$HOME/.arduino/build.tmp"
SKETCH_BUILD_JOBS="${SKETCH_BUILD_JOBS:-auto}"

if [ "$SKETCH_BUILD_JOBS" = "auto" ]; then
    SKETCH_BUILD_JOBS=$("${SCRIPTS_DIR}/sketch_utils.sh" auto_sketch_jobs)
fi

if ! [[ "$SKETCH_BUILD_JOBS" =~ ^[1-9][0-9]*$ ]]; then
    echo "ERROR: SKETCH_BUILD_JOBS must be 'auto' or a positive integer"
    exit 2
fi

function build {
    local target=$1
    local chunk_index=$2
    local chunks_cnt=$3
    local build_log=$4
    local log_level=${5:-none}
    local sketches_file=$6
    shift 6
    local sketches=("$@")

    local BUILD_SKETCH_LIST="${SCRIPTS_DIR}/sketch_utils.sh build_list"
    local BUILD_SKETCHES="${SCRIPTS_DIR}/sketch_utils.sh chunk_build"

    local args=("-au" "$ARDUINO_USR_PATH" "-t" "$target")

    if [ "$OS_IS_LINUX" == "1" ]; then
        args+=(
            "-p" "$ARDUINO_ESP32_PATH/libraries"
            "-i" "$chunk_index"
            "-m" "$chunks_cnt"
            "-d" "$log_level"
            "-j" "$SKETCH_BUILD_JOBS"
            "--isolated-build-dirs"
        )
        if [ -n "$sketches_file" ]; then
            args+=("-f" "$sketches_file")
        fi
        if [ "$build_log" -eq 1 ]; then
            args+=("-l" "$build_log")
        fi
        ${BUILD_SKETCHES} "${args[@]}"
    else
        local sketches_list
        local result
        local xtra_opts=()
        sketches_list=$(mktemp "${TMPDIR:-/tmp}/arduino-push-sketches.XXXXXX") || return 1
        for sketch in "${sketches[@]}"; do
            printf '%s\n' "$(dirname "$sketch")" >> "$sketches_list"
        done
        if [ "$OS_IS_WINDOWS" == "1" ] && [ -d "$ARDUINO_IDE_PATH/tools-builder" ]; then
            local ctags_version
            local preprocessor_version
            ctags_version=$(ls "$ARDUINO_IDE_PATH/tools-builder/ctags/")
            preprocessor_version=$(ls "$ARDUINO_IDE_PATH/tools-builder/arduino-preprocessor/")
            xtra_opts+=(
                "-prefs=runtime.tools.ctags.path=$ARDUINO_IDE_PATH/tools-builder/ctags/$ctags_version"
                "-prefs=runtime.tools.arduino-preprocessor.path=$ARDUINO_IDE_PATH/tools-builder/arduino-preprocessor/$preprocessor_version"
            )
        fi

        if ${BUILD_SKETCH_LIST} -F "$sketches_list" -j "$SKETCH_BUILD_JOBS" \
            --isolated-build-dirs "${args[@]}" -i "$chunk_index" "${xtra_opts[@]}"; then
            result=0
        else
            result=$?
        fi
        rm -f "$sketches_list"
        return "$result"
    fi
}

if [ -z "$GITHUB_REPOSITORY" ]; then
    export GITHUB_REPOSITORY="espressif/arduino-esp32"
fi

CHUNK_INDEX=$1
CHUNKS_CNT=$2
BUILD_LOG=$3
LOG_LEVEL=$4
SKETCHES_FILE=$5
if [ "$#" -lt 2 ] || [ "$CHUNKS_CNT" -le 0 ]; then
    CHUNK_INDEX=0
    CHUNKS_CNT=1
elif [ "$CHUNK_INDEX" -gt "$CHUNKS_CNT" ] &&  [ "$CHUNKS_CNT" -ge 2 ]; then
    CHUNK_INDEX=$CHUNKS_CNT
fi

if [ -z "$BUILD_LOG" ] || [ "$BUILD_LOG" -le 0 ]; then
    BUILD_LOG=0
fi

#echo "Updating submodules ..."
#git -C "$GITHUB_WORKSPACE" submodule update --init --recursive > /dev/null 2>&1

source "${SCRIPTS_DIR}/env.sh"
source "${SCRIPTS_DIR}/install-arduino-core-esp32.sh"

SKETCHES_ESP32=(
    "$ARDUINO_ESP32_PATH/libraries/NetworkClientSecure/examples/WiFiClientSecure/WiFiClientSecure.ino"
    "$ARDUINO_ESP32_PATH/libraries/BLE/examples/Server/Server.ino"
    "$ARDUINO_ESP32_PATH/libraries/ESP32/examples/Camera/CameraWebServer/CameraWebServer.ino"
    "$ARDUINO_ESP32_PATH/libraries/Insights/examples/MinimalDiagnostics/MinimalDiagnostics.ino"
)
#create sizes_file (include target so SoC-outer matrix jobs do not collide)
if [ -n "${BUILD_TARGET:-}" ]; then
    sizes_file="$GITHUB_WORKSPACE/cli_compile_${BUILD_TARGET}_${CHUNK_INDEX}.json"
else
    sizes_file="$GITHUB_WORKSPACE/cli_compile_$CHUNK_INDEX.json"
fi
export ARDUINO_BUILD_SIZES_FILE="$sizes_file"

if [ "$BUILD_LOG" -eq 1 ]; then
    #create sizes_file and echo start of JSON array with "boards" key
    echo "{\"boards\": [" > "$sizes_file"
fi

#build sketches for different targets (using centralized config)
for target in "${CORE_SOCS[@]}"; do
    build "$target" "$CHUNK_INDEX" "$CHUNKS_CNT" "$BUILD_LOG" "$LOG_LEVEL" "$SKETCHES_FILE" "${SKETCHES_ESP32[@]}"
done

if [ "$BUILD_LOG" -eq 1 ]; then
    # Remove the last comma from the last JSON object.
    "${SCRIPTS_DIR}/sketch_utils.sh" remove_trailing_comma "$sizes_file"
    #echo end of JSON array
    echo "]}" >> "$sizes_file"
fi
