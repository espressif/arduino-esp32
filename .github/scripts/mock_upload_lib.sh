#!/bin/bash
# Mock upload CI helpers: TCP bootloader, esptool override, twice-upload, logging.

[ -n "$MOCK_UPLOAD_LIB_SOURCED" ] && return 0
MOCK_UPLOAD_LIB_SOURCED=1

_MOCK_LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPTS_DIR="${SCRIPTS_DIR:-$_MOCK_LIB_DIR}"

source "${SCRIPTS_DIR}/env.sh"
source "${SCRIPTS_DIR}/arduino_headless.sh"

if [ -n "${GITHUB_ACTIONS:-}" ] || [ -t 1 ]; then
    _MOCK_C_RESET=$'\033[0m'
    _MOCK_C_HEADER=$'\033[95m'
    _MOCK_C_STEP=$'\033[96m'
    _MOCK_C_PASS=$'\033[32m'
    _MOCK_C_FAIL=$'\033[31m'
    _MOCK_C_DIM=$'\033[90m'
else
    _MOCK_C_RESET=
    _MOCK_C_HEADER=
    _MOCK_C_STEP=
    _MOCK_C_PASS=
    _MOCK_C_FAIL=
    _MOCK_C_DIM=
fi

function _native_path_for_java {
    local p="$1"
    if [[ "${OS_IS_WINDOWS:-0}" == "1" ]] && command -v cygpath >/dev/null 2>&1; then
        cygpath -m "$p"
    else
        echo "$p"
    fi
}

function _windows_long_path {
    local target="$1" dir base
    if [ -d "$target" ]; then
        (cd "$target" && pwd -W | sed 's|\\|/|g')
    else
        dir="$(dirname "$target")"
        base="$(basename "$target")"
        echo "$(_windows_long_path "$dir")/$base"
    fi
}

function _windows_path_for_ide_pref {
    local p="$1" unix
    if [[ "$p" == [A-Za-z]:/* ]]; then
        unix="$(cygpath -u "$p" 2>/dev/null || echo "$p")"
    else
        unix="$p"
    fi
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -w "$unix"
    else
        echo "$p" | sed 's|/|\\|g'
    fi
}

function _mock_arduino_packages_dir {
    if [[ "${OS_IS_MACOS:-0}" == "1" ]]; then
        echo "$HOME/Library/Arduino15"
    elif [[ "${OS_IS_WINDOWS:-0}" == "1" ]]; then
        echo "${LOCALAPPDATA:-$HOME/AppData/Local}/Arduino15"
    else
        echo "$HOME/.arduino15"
    fi
}

function mock_upload_mktemp_build_dir {
    local dir
    dir="$(mktemp -d)"
    if [[ "${OS_IS_WINDOWS:-0}" == "1" ]]; then
        _windows_long_path "$dir"
    else
        echo "$dir"
    fi
}

function mock_upload_prepare_windows_builder_paths {
    local sketch="$1" staging sketch_name sketch_dir src_dir
    sketch_name="$(basename "$sketch")"
    sketch_dir="$(basename "$sketch" .ino)"
    src_dir="$(dirname "$sketch")"
    staging="$(mktemp -d)"
    mkdir -p "$staging/$sketch_dir/build"
    cp -a "$src_dir/." "$staging/$sketch_dir/"
    MOCK_UPLOAD_WINDOWS_STAGING="$staging"
    export MOCK_UPLOAD_WINDOWS_STAGING
    printf '%s\n' \
        "$(_windows_long_path "$staging/$sketch_dir")/$sketch_name" \
        "$(_windows_long_path "$staging/$sketch_dir/build")"
}

function _dump_arduino_app_log {
    local -a candidates=() log
    if [[ "${OS_IS_WINDOWS:-0}" == "1" ]]; then
        candidates+=(
            "$(_mock_arduino_packages_dir)/logs/application.log"
            "${LOCALAPPDATA}/Arduino15/logs/application.log"
            "${HOME}/AppData/Local/Arduino15/logs/application.log"
            "${ARDUINO_USR_PATH:-}/logs/application.log"
            "${ARDUINO_BUILDER_PATH:-}/portable/logs/application.log"
        )
    else
        candidates+=("${HOME}/.arduino15/logs/application.log")
    fi
    for log in "${candidates[@]}"; do
        [ -n "$log" ] || continue
        if [ ! -f "$log" ] && command -v cygpath >/dev/null 2>&1; then
            log="$(cygpath -u "$log" 2>/dev/null || echo "$log")"
        fi
        [ -f "$log" ] || continue
        echo "--- Arduino application.log (last 80 lines): $log ---" >&2
        tail -n 80 "$log" >&2
        return 0
    done
}

function start_mock_bootloader {
    local chip="${1:-}"
    MOCK_BOOTLOADER_PORT=""
    MOCK_BOOTLOADER_CLI_PORT=""
    MOCK_BOOTLOADER_SERIAL_PORT=""

    local tcp_port="${MOCK_BOOTLOADER_TCP_PORT:-9876}"
    local mock_timeout="${MOCK_BOOTLOADER_TIMEOUT:-600}"
    local -a chip_args=()

    command -v esp32-mock-bootloader >/dev/null 2>&1 || {
        echo "ERROR: esp32-mock-bootloader not found (pip install esp32-mock-bootloader)" >&2
        return 1
    }

    if [ -n "$chip" ]; then
        chip_args=(--chip "$chip")
    fi

    esp32-mock-bootloader start --port "$tcp_port" --startup-timeout "$mock_timeout" "${chip_args[@]}" || {
        echo "ERROR: mock bootloader failed to start" >&2
        return 1
    }

    MOCK_BOOTLOADER_PORT="$(esp32-mock-bootloader url --port "$tcp_port")"
    MOCK_BOOTLOADER_CLI_PORT="$MOCK_BOOTLOADER_PORT"
    MOCK_BOOTLOADER_SERIAL_PORT="$MOCK_BOOTLOADER_PORT"
}

function stop_mock_bootloader {
    local tcp_port="${MOCK_BOOTLOADER_TCP_PORT:-9876}"
    command -v esp32-mock-bootloader >/dev/null 2>&1 && \
        esp32-mock-bootloader stop --port "$tcp_port" 2>/dev/null || true
    unset MOCK_BOOTLOADER_PORT MOCK_BOOTLOADER_CLI_PORT MOCK_BOOTLOADER_SERIAL_PORT
}

function mock_upload_soc_banner {
    local soc="$1"
    echo ""
    printf '%s########################################%s\n' "$_MOCK_C_HEADER" "$_MOCK_C_RESET"
    printf '%s# SoC: %s%s\n' "$_MOCK_C_HEADER" "$soc" "$_MOCK_C_RESET"
    printf '%s# mock bootloader: TCP socket (esptool socket://)%s\n' "$_MOCK_C_HEADER" "$_MOCK_C_RESET"
    printf '%s########################################%s\n' "$_MOCK_C_HEADER" "$_MOCK_C_RESET"
}

function mock_upload_tool_header {
    local tool_label="$1" soc="$2"
    echo ""
    printf '%s=== [%s] SoC: %s ===%s\n' "$_MOCK_C_HEADER" "$tool_label" "$soc" "$_MOCK_C_RESET"
}

function mock_upload_tool_meta {
    printf '%s    %s%s\n' "$_MOCK_C_DIM" "$*" "$_MOCK_C_RESET"
}

function mock_upload_result {
    local tool_label="$1" soc="$2" status="$3"
    if [ "$status" = "PASS" ]; then
        printf '%s--- [%s] %s: PASS (both uploads) ---%s\n' "$_MOCK_C_PASS" "$tool_label" "$soc" "$_MOCK_C_RESET"
    else
        printf '%s--- [%s] %s: FAIL (see upload step above) ---%s\n' "$_MOCK_C_FAIL" "$tool_label" "$soc" "$_MOCK_C_RESET" >&2
    fi
}

function mock_upload_log_step {
    local tool="$1" step="$2" total="$3" detail="$4"
    echo ""
    printf '%s>>> [%s] upload %s/%s: %s%s\n' "$_MOCK_C_STEP" "$tool" "$step" "$total" "$detail" "$_MOCK_C_RESET"
}

function mock_upload_twice_cli {
    local fqbn="$1" sketch="$2" build_dir="$3"
    local cli="${ARDUINO_CLI_PATH:-${ARDUINO_IDE_PATH:?}/arduino-cli}"
    local curroptions currcli_fqbn port
    local tool="arduino-cli"

    if [[ "${OS_IS_WINDOWS:-0}" == "1" ]]; then
        sketch="$(_native_path_for_java "$sketch")"
        build_dir="$(_native_path_for_java "$build_dir")"
    fi

    port="${MOCK_BOOTLOADER_CLI_PORT:-$MOCK_BOOTLOADER_PORT}"
    [ -n "$port" ] || { echo "ERROR: mock bootloader port not set" >&2; return 1; }
    [ -x "$cli" ] || { echo "ERROR: arduino-cli not found at $cli" >&2; return 1; }
    mkdir -p "$build_dir"

    curroptions=$(echo "$fqbn" | cut -d':' -f4-)
    currcli_fqbn=$(echo "$fqbn" | cut -d':' -f1-3)

    mock_upload_log_step "$tool" 1 2 "compile + upload (full flash, no flasher cache yet)"
    "$cli" compile --fqbn "$currcli_fqbn" --board-options "$curroptions" \
        --build-path "$build_dir" -p "$port" --upload "$sketch" || return 1
    mock_upload_log_step "$tool" 2 2 "upload only (fast reflash via flasher --diff-with)"
    "$cli" upload --fqbn "$currcli_fqbn" --board-options "$curroptions" \
        --input-dir "$build_dir" -p "$port" "$sketch" || return 1
}

function mock_upload_twice_headless {
    local fqbn="$1" sketch="$2" build_dir="$3" runner="$4" tool_label="$5"
    shift 5
    local -a prefix_args=("$@") args port project_name build_path_pref

    port="${MOCK_BOOTLOADER_SERIAL_PORT:-$MOCK_BOOTLOADER_PORT}"
    [ -n "$port" ] || { echo "ERROR: mock bootloader serial port not set" >&2; return 1; }
    if [[ "${OS_IS_WINDOWS:-0}" == "1" ]]; then
        local -a win_paths=()
        mapfile -t win_paths < <(mock_upload_prepare_windows_builder_paths "$sketch")
        sketch="${win_paths[0]}"
        build_dir="${win_paths[1]}"
    fi
    project_name="$(basename "$sketch")"
    mkdir -p "$build_dir"
    build_path_pref="$build_dir"
    if [[ "${OS_IS_WINDOWS:-0}" == "1" ]]; then
        build_path_pref="$(_windows_path_for_ide_pref "$build_dir")"
    fi
    args=(
        "${prefix_args[@]}"
        --board "$fqbn"
        --pref "build.path=$build_path_pref"
        --pref "build.project_name=$project_name"
        --pref "serial.port=$port"
        --port "$port"
        --upload
        "$sketch"
    )
    mock_upload_log_step "$tool_label" 1 2 "compile + upload (full flash, IDE 1.x upload path)"
    if [[ "${OS_IS_WINDOWS:-0}" == "1" ]]; then
        echo "    sketch (staged): $sketch" >&2
        echo "    build.path (pref): $build_path_pref" >&2
    fi
    if ! run_with_timeout 900 "$runner" "${args[@]}"; then
        _dump_arduino_app_log
        return 1
    fi
    mock_upload_log_step "$tool_label" 2 2 "compile + upload again (fast reflash via flasher --diff-with)"
    if ! run_with_timeout 900 "$runner" "${args[@]}"; then
        _dump_arduino_app_log
        return 1
    fi
}

function mock_upload_twice_builder {
    mock_upload_twice_headless "$1" "$2" "$3" run_arduino_builder "arduino-builder"
}

function mock_upload_twice_ide_v1 {
    mock_upload_twice_headless "$1" "$2" "$3" run_arduino_ide_v1 "arduino-ide-v1" "${@:4}"
}
