#!/bin/bash
# Package install tests: pre (draft release via API proxy) | post (live gh-pages JSON)
# shellcheck disable=SC2181

RELEASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPTS_DIR="$(cd "$RELEASE_DIR/.." && pwd)"
source "$RELEASE_DIR/common.sh"
source "${SCRIPTS_DIR}/mock_upload_lib.sh"
source "${SCRIPTS_DIR}/env.sh"

MODE="${1:?Usage: test-package-install.sh pre|post}"
GITHUB_WORKSPACE="${GITHUB_WORKSPACE:-$(pwd)}"
RELEASE_PRE="${RELEASE_PRE:-false}"
RELEASE_TEST_MODE="$MODE"
SKETCH="$GITHUB_WORKSPACE/libraries/ESP32/examples/CI/CIBoardsTest/CIBoardsTest.ino"
# 115200 works on PTY mocks; default 921600 often fails on virtual serial ports.
MOCK_UPLOAD_FQBN="$("${SCRIPTS_DIR}/sketch_utils.sh" default_upload_test_fqbn esp32 esp32:esp32)"

declare -a TEST_RESULTS=()
TEST_FAILURES=0
TEST_INDEX=0
TEST_PLANNED=0

if [[ "$OS_IS_WINDOWS" == "1" ]]; then export PATH="$HOME/bin:$PATH"
else export PATH="/home/runner/bin:$HOME/bin:$PATH"; fi
source "${SCRIPTS_DIR}/install-arduino-cli.sh"

log_test_msg() {
    echo "[release-test] $*" >&2
}

classify_json_url_mode() {
    local url="$1"
    case "$url" in
        file://*) echo "local-file" ;;
        http://127.0.0.1:* | http://localhost:*)
            echo "local-http-server"
            ;;
        https://raw.githubusercontent.com/*/gh-pages/*)
            echo "gh-pages-live"
            ;;
        https://github.com/*/releases/download/* | https://api.github.com/*)
            echo "github-release-asset"
            ;;
        https://github.com/*)
            echo "github"
            ;;
        *)
            echo "remote"
            ;;
    esac
}

describe_package_json_at_url() {
    local json_url="$1" json_file="${2:-}" version platform_url archive mode
    mode="$(classify_json_url_mode "$json_url")"
    if [ -n "$json_file" ] && [ -f "$json_file" ]; then
        version=$(jq -r '.packages[0].platforms[0].version // empty' "$json_file")
        platform_url=$(jq -r '.packages[0].platforms[0].url // empty' "$json_file")
        archive=$(jq -r '.packages[0].platforms[0].archiveFileName // empty' "$json_file")
    elif command -v curl >/dev/null 2>&1; then
        local remote_json
        remote_json=$(curl -fsSL "$json_url" 2>/dev/null || true)
        if [ -n "$remote_json" ]; then
            version=$(jq -r '.packages[0].platforms[0].version // empty' <<<"$remote_json")
            platform_url=$(jq -r '.packages[0].platforms[0].url // empty' <<<"$remote_json")
            archive=$(jq -r '.packages[0].platforms[0].archiveFileName // empty' <<<"$remote_json")
        fi
    fi
    log_test_msg "  json url:       $json_url"
    log_test_msg "  url mode:       $mode"
    [ -n "$json_file" ] && log_test_msg "  json file:      $json_file"
    [ -n "$version" ] && log_test_msg "  json version:   $version"
    [ -n "$archive" ] && log_test_msg "  core archive:   $archive"
    [ -n "$platform_url" ] && log_test_msg "  core download:  $platform_url"
}

log_release_test_banner() {
    log_test_msg "════════════════════════════════════════════════════════════"
    log_test_msg "Release package install tests ($MODE-release)"
    log_test_msg "  OS:              $(uname -s)"
    log_test_msg "  core version:    $EXPECTED_CORE_VERSION"
    log_test_msg "  prerelease:      $RELEASE_PRE"
    log_test_msg "  sketch:          $SKETCH"
    log_test_msg "  upload FQBN:     $MOCK_UPLOAD_FQBN"
    log_test_msg "════════════════════════════════════════════════════════════"
}

log_package_json_inventory() {
    local json_file version
    log_test_msg "Package JSON in $OUTPUT_DIR:"
    shopt -s nullglob
    for json_file in "$OUTPUT_DIR"/package_esp32_*.json; do
        version=$(jq -r '.packages[0].platforms[0].version // "?"' "$json_file")
        log_test_msg "  - $(basename "$json_file") (platform $version)"
    done
    shopt -u nullglob
}

count_pre_tests() {
    local n=2
    [ "$RELEASE_PRE" = "false" ] && n=$((n + 2))
    echo "$n"
}

json_uses_github_release_urls() {
    local json_file="$1" url
    url=$(jq -r '.packages[0].platforms[0].url // empty' "$json_file")
    case "$url" in
        https://github.com/*/releases/download/*) return 0 ;;
    esac
    return 1
}

# prepare_pre_test_json JSON_NAME
# Merged JSON keeps untagged GitHub URLs; pre-test rewrites draft archives to API proxy.
prepare_pre_test_json() {
    local json_name="$1"
    local src="$OUTPUT_DIR/$json_name"
    local local_json="$OUTPUT_DIR/${json_name%.json}.proxy.json"

    if ! json_uses_github_release_urls "$src"; then
        log_test_msg "ERROR: $json_name must reference draft GitHub release URLs for pre-test"
        return 1
    fi
    rewrite_json_to_proxy "$src" "$local_json" "$OUTPUT_DIR/draft-assets.json" || return 1
    verify_pre_test_proxy_json "$local_json" "$OUTPUT_DIR/draft-assets.json" "$GITHUB_RELEASE_PROXY_URL" || return 1
    PRE_TEST_JSON_FILE="$local_json"
    PRE_TEST_JSON_URL="${GITHUB_RELEASE_PROXY_URL}/$(basename "$local_json")"
    PRE_TEST_LABEL="install from draft release (API proxy)"
}

count_post_tests() {
    local n=1
    [ "$RELEASE_PRE" = "false" ] && n=$((n + 1))
    n=$((n + 1))
    [ "$RELEASE_PRE" = "false" ] && n=$((n + 1))
    echo "$n"
}

begin_test() {
    local name="$1"
    TEST_INDEX=$((TEST_INDEX + 1))
    log_test_msg ""
    log_test_msg "── Test $TEST_INDEX/$TEST_PLANNED: $name ──"
}

# CI sets RELEASE_VERSION from the workflow input; local runs infer it from build artifacts.
EXPECTED_CORE_VERSION="$(resolve_release_core_version)" || {
    TEST_FAILURES=1
    exit 1
}
export EXPECTED_CORE_VERSION
export RELEASE_VERSION="$EXPECTED_CORE_VERSION"
log_release_test_banner

record_test() {
    TEST_RESULTS+=("$1|$2")
    [ "$1" = "FAIL" ] && TEST_FAILURES=$((TEST_FAILURES + 1))
}

run_test() {
    local name="$1" rc
    shift
    begin_test "$name"
    "$@"
    rc=$?
    case "$rc" in
        0)
            record_test PASS "$name"
            log_test_msg "→ PASS"
            ;;
        2)
            record_test SKIP "$name"
            log_test_msg "→ SKIP"
            ;;
        *)
            record_test FAIL "$name"
            log_test_msg "→ FAIL (exit $rc)"
            ;;
    esac
    return 0
}

print_test_summary() {
    local pass=0 skip=0 entry status name
    {
        echo ""
        echo "=== Test summary ($MODE-release, $(uname -s)) ==="
        for entry in "${TEST_RESULTS[@]}"; do
            status="${entry%%|*}"
            name="${entry#*|}"
            case "$status" in
                PASS)
                    pass=$((pass + 1))
                    echo "  ✓ PASS  $name"
                    ;;
                SKIP)
                    skip=$((skip + 1))
                    echo "  - SKIP  $name"
                    ;;
                FAIL)
                    echo "  ✗ FAIL  $name"
                    ;;
                *)
                    echo "  ✗ UNKNOWN STATUS $name"
                    ;;
            esac
        done
        echo ""
        if [ "$TEST_FAILURES" -eq 0 ]; then
            echo "Result: PASSED ($pass passed${skip:+, $skip skipped})"
        else
            echo "Result: FAILED ($TEST_FAILURES failed, $pass passed${skip:+, $skip skipped})"
        fi
    } >&2
    [ "$TEST_FAILURES" -eq 0 ]
}

finish_tests() {
    print_test_summary
    exit $((TEST_FAILURES > 0 ? 1 : 0))
}
trap finish_tests EXIT

test_cli_install_only() {
    local url="$1" label="$2" json_file="${3:-}"
    log_test_msg "  client:         arduino-cli"
    log_test_msg "  package index:  $label"
    describe_package_json_at_url "$url" "$json_file"
    log_test_msg "  [1/3] core install esp32:esp32@${EXPECTED_CORE_VERSION} ..."
    install_esp32_core_for_test "$url"
    log_test_msg "  [2/3] verify installed version + toolchain ..."
    verify_installed_version
    verify_core_toolchain
    log_test_msg "  [3/3] uninstall esp32:esp32 ..."
    arduino-cli core uninstall esp32:esp32
}

test_cli_url() {
    local url="$1" label="$2" json_file="${3:-}"
    log_test_msg "  client:         arduino-cli"
    log_test_msg "  package index:  $label"
    describe_package_json_at_url "$url" "$json_file"
    log_test_msg "  [1/4] core install esp32:esp32@${EXPECTED_CORE_VERSION} ..."
    install_esp32_core_for_test "$url" || return 1
    verify_installed_version
    local build_dir
    build_dir="$(mock_upload_mktemp_build_dir)"
    log_test_msg "  [2/3] mock bootloader + compile/upload (twice) ..."
    start_mock_bootloader esp32
    mock_upload_twice_cli "$MOCK_UPLOAD_FQBN" "$SKETCH" "$build_dir"
    stop_mock_bootloader
    rm -rf "$build_dir"
    log_test_msg "  [3/3] uninstall esp32:esp32 ..."
    arduino-cli core uninstall esp32:esp32
}

test_ide_v1_install_only() {
    local url="$1" label="$2" json_file="${3:-}"

    log_test_msg "  client:         Arduino IDE 1.x"
    log_test_msg "  package index:  $label"
    describe_package_json_at_url "$url" "$json_file"
    source "${SCRIPTS_DIR}/install-arduino-ide.sh"
    if [ ! -d "$ARDUINO_IDE_PATH" ]; then
        log_test_msg "WARNING: IDE v1 not installed, skipping"
        return 2
    fi
    log_test_msg "  [1/2] IDE v1 board install esp32:esp32:${EXPECTED_CORE_VERSION} ..."
    ide_v1_install_boards "$url" ":$EXPECTED_CORE_VERSION"
    log_test_msg "  [2/2] verify installed version + IDE v1 toolchains ..."
    verify_installed_version || return 1
    ide_v1_toolchain_ready || return 1
    purge_stale_esp32_toolchains
}

test_ide_v1_url() {
    local url="$1" label="$2" json_file="${3:-}" rc
    local -a ide_args

    log_test_msg "  client:         Arduino IDE 1.x"
    log_test_msg "  package index:  $label"
    describe_package_json_at_url "$url" "$json_file"
    source "${SCRIPTS_DIR}/install-arduino-ide.sh"
    if [ ! -d "$ARDUINO_IDE_PATH" ]; then
        log_test_msg "WARNING: IDE v1 not installed, skipping"
        return 2
    fi
    log_test_msg "  [1/4] IDE v1 board install esp32:esp32:${EXPECTED_CORE_VERSION} ..."
    ide_v1_install_boards "$url" ":$EXPECTED_CORE_VERSION" || return 1
    verify_installed_version || return 1

    local build_dir
    build_dir="$(mock_upload_mktemp_build_dir)"
    log_test_msg "  [2/3] mock bootloader + IDE v1 compile/upload (twice) ..."
    start_mock_bootloader esp32 || return 1
    mock_upload_twice_ide_v1 "$MOCK_UPLOAD_FQBN" "$SKETCH" "$build_dir" \
        --pref "boardsmanager.additional.urls=$url"
    rc=$?
    stop_mock_bootloader
    rm -rf "$build_dir"
    [ "$rc" -eq 0 ] || { log_test_msg "ERROR: IDE v1 failed (exit $rc)"; return 1; }
    # Arduino IDE 1.8 macOS is x86_64 (Rosetta on Apple Silicon) and installs x86_64 toolchains.
    log_test_msg "  [3/3] purge stale toolchains after IDE v1 ..."
    purge_stale_esp32_toolchains
}

run_pre_tests() {
    local DEV=package_esp32_dev_index.json REL=package_esp32_index.json

    TEST_PLANNED="$(count_pre_tests)"
    log_test_msg "Test plan: $TEST_PLANNED case(s) on this runner"
    log_test_msg "Each case: install merged package JSON from draft release (API proxy) → compile → mock upload (twice)"

    clean_release_test_staging_files

    [ -f "$OUTPUT_DIR/$DEV" ] || {
        log_test_msg "ERROR: $OUTPUT_DIR/$DEV not found"
        TEST_FAILURES=1
        return 1
    }
    log_package_json_inventory
    [ -f "$OUTPUT_DIR/draft-assets.json" ] || {
        log_test_msg "ERROR: $OUTPUT_DIR/draft-assets.json not found (required for draft release proxy)"
        TEST_FAILURES=1
        return 1
    }
    start_github_release_proxy "$OUTPUT_DIR" "$OUTPUT_DIR/draft-assets.json" || {
        log_test_msg "ERROR: failed to start GitHub release proxy"
        TEST_FAILURES=1
        return 1
    }
    verify_draft_release_proxy_smoke "$OUTPUT_DIR/draft-assets.json" || {
        log_test_msg "ERROR: draft release proxy smoke test failed"
        TEST_FAILURES=1
        return 1
    }
    trap 'stop_local_package_server; finish_tests' EXIT
    log_test_msg "Release test proxy: $GITHUB_RELEASE_PROXY_URL (JSON local; draft archives via GitHub API only)"

    log_test_msg ""
    log_test_msg "Phase: arduino-cli"
    prepare_pre_test_json "$DEV"
    run_test "arduino-cli: $DEV ($PRE_TEST_LABEL)" \
        test_cli_url "$PRE_TEST_JSON_URL" "$DEV ($PRE_TEST_LABEL)" "$PRE_TEST_JSON_FILE"

    if [ "$RELEASE_PRE" = "false" ] && [ -f "$OUTPUT_DIR/$REL" ]; then
        prepare_pre_test_json "$REL"
        run_test "arduino-cli: $REL ($PRE_TEST_LABEL)" \
            test_cli_url "$PRE_TEST_JSON_URL" "$REL ($PRE_TEST_LABEL)" "$PRE_TEST_JSON_FILE"
    elif [ "$RELEASE_PRE" = "true" ]; then
        log_test_msg "Skipping $REL arduino-cli tests (prerelease build)"
        record_test SKIP "arduino-cli: $REL (prerelease)"
    fi

    log_test_msg ""
    log_test_msg "Phase: Arduino IDE 1.x"
    arduino-cli core uninstall esp32:esp32 2>/dev/null || true
    purge_stale_esp32_toolchains
    prepare_ide_v1_package_test
    prepare_pre_test_json "$DEV"
    run_test "IDE v1: $DEV ($PRE_TEST_LABEL)" \
        test_ide_v1_url "$PRE_TEST_JSON_URL" "$DEV ($PRE_TEST_LABEL)" "$PRE_TEST_JSON_FILE"
    if [ "$RELEASE_PRE" = "false" ] && [ -f "$OUTPUT_DIR/$REL" ]; then
        prepare_pre_test_json "$REL"
        run_test "IDE v1: $REL ($PRE_TEST_LABEL)" \
            test_ide_v1_url "$PRE_TEST_JSON_URL" "$REL ($PRE_TEST_LABEL)" "$PRE_TEST_JSON_FILE"
    elif [ "$RELEASE_PRE" = "true" ]; then
        log_test_msg "Skipping $REL IDE v1 tests (prerelease build)"
        record_test SKIP "IDE v1: $REL (prerelease)"
    fi
}

run_post_tests() {
    local REPO="${GITHUB_REPOSITORY:-espressif/arduino-esp32}"
    local STAGING=release-staging
    local DEV=package_esp32_dev_index.json REL=package_esp32_index.json
    local DEV_URL="https://raw.githubusercontent.com/${REPO}/gh-pages/${STAGING}/${DEV}"
    local REL_URL="https://raw.githubusercontent.com/${REPO}/gh-pages/${STAGING}/${REL}"

    TEST_PLANNED="$(count_post_tests)"
    log_test_msg "Test plan: $TEST_PLANNED install-only case(s) on this runner"
    log_test_msg "Each case: install from release-staging gh-pages JSON (upload already validated in pre-release)"
    log_test_msg "Live gh-pages JSON base: https://raw.githubusercontent.com/${REPO}/gh-pages/${STAGING}/"

    verify_gh_pages_package_json "$DEV_URL" "$EXPECTED_CORE_VERSION" || {
        TEST_FAILURES=1
        return 1
    }
    if [ "$RELEASE_PRE" = "false" ]; then
        verify_gh_pages_package_json "$REL_URL" "$EXPECTED_CORE_VERSION" || {
            TEST_FAILURES=1
            return 1
        }
    fi

    log_test_msg ""
    log_test_msg "Phase: arduino-cli"
    run_test "arduino-cli: $DEV (gh-pages install)" \
        test_cli_install_only "$DEV_URL" "$DEV (gh-pages)"
    if [ "$RELEASE_PRE" = "false" ]; then
        run_test "arduino-cli: $REL (gh-pages install)" \
            test_cli_install_only "$REL_URL" "$REL (gh-pages)"
    else
        log_test_msg "Skipping $REL arduino-cli tests (prerelease build)"
        record_test SKIP "arduino-cli: $REL (prerelease)"
    fi

    log_test_msg ""
    log_test_msg "Phase: Arduino IDE 1.x"
    arduino-cli core uninstall esp32:esp32 2>/dev/null || true
    purge_stale_esp32_toolchains
    prepare_ide_v1_package_test
    run_test "IDE v1: $DEV (gh-pages install)" \
        test_ide_v1_install_only "$DEV_URL" "$DEV (gh-pages)"
    if [ "$RELEASE_PRE" = "false" ]; then
        run_test "IDE v1: $REL (gh-pages install)" \
            test_ide_v1_install_only "$REL_URL" "$REL (gh-pages)"
    else
        log_test_msg "Skipping $REL IDE v1 tests (prerelease build)"
        record_test SKIP "IDE v1: $REL (prerelease)"
    fi
}

case "$MODE" in
    pre) run_pre_tests ;;
    post) run_post_tests ;;
    *)
        echo "Unknown mode: $MODE" >&2
        TEST_FAILURES=1
        ;;
esac
