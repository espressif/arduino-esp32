#!/bin/bash
# PR/push mock upload CI entry: arduino-cli and arduino-nightly builder, twice per SoC.
# Usage: test-mock-upload.sh [cli|builder|all] [soc]
# Default: all tools, all CORE_SOCS.

set -eo pipefail

SCRIPTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPTS_DIR}/socs_config.sh"
source "${SCRIPTS_DIR}/mock_upload_lib.sh"

printf '%s=== Mock upload test setup (%s/%s) ===%s\n' \
    "$_MOCK_C_HEADER" "$(uname -s)" "$(uname -m)" "$_MOCK_C_RESET"

SKETCH_UTILS="${SCRIPTS_DIR}/sketch_utils.sh"

TOOL_FILTER="${1:-all}"
SOC_FILTER="${2:-}"

GITHUB_WORKSPACE="${GITHUB_WORKSPACE:-$(pwd)}"
SKETCH="$GITHUB_WORKSPACE/libraries/ESP32/examples/CI/CIBoardsTest/CIBoardsTest.ino"
if [[ "${OS_IS_WINDOWS:-0}" == "1" ]] && command -v cygpath >/dev/null 2>&1; then
    SKETCH="$(cygpath -m "$SKETCH")"
fi

declare -a TEST_RESULTS=()
TEST_FAILURES=0

if [[ "${OS_IS_WINDOWS:-0}" == "1" ]]; then
    export PATH="$HOME/bin:$PATH"
else
    export PATH="/home/runner/bin:$HOME/bin:$PATH"
fi

echo "Installing arduino-cli ..."
source "${SCRIPTS_DIR}/install-arduino-cli.sh"
echo "Installing arduino-nightly builder ..."
source "${SCRIPTS_DIR}/install-arduino-builder.sh"
echo "Installing ESP32 core (symlink + tools) ..."
source "${SCRIPTS_DIR}/install-arduino-core-esp32.sh"
echo "Setup complete. Sketch: $SKETCH"

record_test() {
    TEST_RESULTS+=("$1|$2")
    [ "$1" = "FAIL" ] && TEST_FAILURES=$((TEST_FAILURES + 1))
    return 0
}

run_soc_tool_test() {
    local tool="$1" soc="$2" fqbn="$3"
    local name="${tool}:${soc}"
    local build_dir rc=0 tool_label

    case "$tool" in
        cli) tool_label="arduino-cli" ;;
        builder) tool_label="arduino-builder" ;;
        *)
            echo "ERROR: unknown tool: $tool" >&2
            return 1
            ;;
    esac

    mock_upload_tool_header "$tool_label" "$soc"
    mock_upload_tool_meta "FQBN: $fqbn"
    mock_upload_tool_meta "sketch: $SKETCH"

    build_dir="$(mock_upload_mktemp_build_dir)"
    mock_upload_tool_meta "build dir: $build_dir"

    case "$tool" in
        cli)
            mock_upload_twice_cli "$fqbn" "$SKETCH" "$build_dir" || rc=$?
            ;;
        builder)
            mock_upload_twice_builder "$fqbn" "$SKETCH" "$build_dir" || rc=$?
            ;;
    esac
    rm -rf "$build_dir"
    if [ -n "${MOCK_UPLOAD_WINDOWS_STAGING:-}" ]; then
        rm -rf "$MOCK_UPLOAD_WINDOWS_STAGING"
        unset MOCK_UPLOAD_WINDOWS_STAGING
    fi

    if [ "$rc" -eq 0 ]; then
        mock_upload_result "$tool_label" "$soc" PASS
        record_test PASS "$name"
    else
        mock_upload_result "$tool_label" "$soc" FAIL
        record_test FAIL "$name"
    fi
    return 0
}

print_summary() {
    local pass=0 entry status name
    echo ""
    printf '%s=== Mock upload summary (%s, tool=%s) ===%s\n' \
        "$_MOCK_C_HEADER" "$(uname -s)" "$TOOL_FILTER" "$_MOCK_C_RESET"
    for entry in "${TEST_RESULTS[@]}"; do
        status="${entry%%|*}"
        name="${entry#*|}"
        case "$status" in
            PASS)
                pass=$((pass + 1))
                printf '  %s✓ PASS%s  %s\n' "$_MOCK_C_PASS" "$_MOCK_C_RESET" "$name"
                ;;
            FAIL)
                printf '  %s✗ FAIL%s  %s\n' "$_MOCK_C_FAIL" "$_MOCK_C_RESET" "$name"
                ;;
        esac
    done
    echo ""
    if [ "$TEST_FAILURES" -eq 0 ]; then
        printf '%sResult: PASSED (%s passed)%s\n' "$_MOCK_C_PASS" "$pass" "$_MOCK_C_RESET"
    else
        printf '%sResult: FAILED (%s failed, %s passed)%s\n' "$_MOCK_C_FAIL" "$TEST_FAILURES" "$pass" "$_MOCK_C_RESET"
    fi
}

case "$TOOL_FILTER" in
    cli|builder|all) ;;
    *)
        echo "Usage: $0 [cli|builder|all] [soc]" >&2
        exit 1
        ;;
esac

declare -a SOCS_TO_RUN=()
if [ -n "$SOC_FILTER" ]; then
    SOCS_TO_RUN=("$SOC_FILTER")
else
    SOCS_TO_RUN=("${CORE_SOCS[@]}")
fi

finish_tests() {
    print_summary
    exit $((TEST_FAILURES > 0 ? 1 : 0))
}
trap finish_tests EXIT

for soc in "${SOCS_TO_RUN[@]}"; do
    fqbn="$("$SKETCH_UTILS" default_upload_test_fqbn "$soc")"
    mock_upload_soc_banner "$soc"
    start_mock_bootloader "$soc" || {
        record_test FAIL "mock:${soc}"
        continue
    }

    if [ "$TOOL_FILTER" = "cli" ] || [ "$TOOL_FILTER" = "all" ]; then
        run_soc_tool_test cli "$soc" "$fqbn"
    fi
    if [ "$TOOL_FILTER" = "builder" ] || [ "$TOOL_FILTER" = "all" ]; then
        run_soc_tool_test builder "$soc" "$fqbn"
    fi

    stop_mock_bootloader
done
