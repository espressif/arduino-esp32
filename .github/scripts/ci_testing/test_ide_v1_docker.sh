#!/bin/bash
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
#
# Docker-based Arduino IDE v1 testing across multiple platforms.
# (Renamed from test_ide_v1.sh — use ci_testing/release_validation.sh for native local validation.)
#
# This script detects the host OS/architecture and Docker capabilities,
# then runs IDE v1 install+compile tests in containers for each feasible
# platform. Platforms that cannot be tested are reported as SKIPPED.
#
# Usage:
#   bash .github/scripts/ci_testing/test_ide_v1_docker.sh [package_json_path]
#
# Arguments:
#   package_json_path  Path to package JSON file to test (default: build/package_esp32_dev_index.json)
#
# Environment:
#   RELEASE_VERSION    Version to install (e.g. 3.3.9). Auto-detected from JSON if unset.
#   SKETCH_PATH        Path to sketch to compile (default: libraries/ESP32/examples/CI/CIBoardsTest/CIBoardsTest.ino)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

PACKAGE_JSON="${1:-$REPO_ROOT/package/package_esp32_index.template.json}"
SKETCH_PATH="${SKETCH_PATH:-libraries/ESP32/examples/CI/CIBoardsTest/CIBoardsTest.ino}"
ARDUINO_IDE_VERSION="1.8.19"

# Results tracking (plain variables for Bash 3.x compat on macOS)
RESULT_linux64="PENDING"
RESULT_linuxarm="PENDING"
RESULT_windows="PENDING"
RESULT_macos="PENDING"

HOST_OS="$(uname -s)"
HOST_ARCH="$(uname -m)"

echo "==========================================="
echo "  IDE v1 Docker Test Runner"
echo "==========================================="
echo "Host: $HOST_OS ($HOST_ARCH)"
echo "Package JSON: $PACKAGE_JSON"
echo ""

if [ ! -f "$PACKAGE_JSON" ]; then
    echo "ERROR: Package JSON not found at $PACKAGE_JSON"
    exit 1
fi

# Check Docker availability
if ! command -v docker &>/dev/null; then
    echo "ERROR: Docker is not installed or not in PATH"
    echo "Install Docker to run IDE v1 platform tests"
    exit 1
fi

if ! docker info &>/dev/null; then
    echo "ERROR: Docker daemon is not running or not accessible"
    exit 1
fi

# Detect Docker capabilities
can_run_linux_amd64() {
    local host_arch="$HOST_ARCH"
    if [[ "$host_arch" == "x86_64" ]]; then
        return 0
    fi
    # Check buildx/QEMU for cross-arch
    if docker buildx ls 2>/dev/null | grep -q "linux/amd64"; then
        return 0
    fi
    return 1
}

can_run_linux_arm64() {
    local host_arch="$HOST_ARCH"
    if [[ "$host_arch" == "aarch64" || "$host_arch" == "arm64" ]]; then
        return 0
    fi
    if docker buildx ls 2>/dev/null | grep -q "linux/arm64"; then
        return 0
    fi
    return 1
}

can_run_windows() {
    if [[ "$HOST_OS" != "MINGW"* && "$HOST_OS" != "MSYS"* && "$HOST_OS" != "CYGWIN"* && "$HOST_OS" != *"NT"* ]]; then
        return 1
    fi
    # Check if Windows containers are enabled
    if docker info 2>/dev/null | grep -q "OSType: windows"; then
        return 0
    fi
    return 1
}

run_linux_test() {
    local label="$1"
    local docker_platform="$2"

    echo "--- Testing: $label ---"

    local -a docker_args=(--rm)
    if [[ "$docker_platform" == "linux/amd64" && "$HOST_ARCH" != "x86_64" ]]; then
        docker_args+=(--platform linux/amd64)
    elif [[ "$docker_platform" == "linux/arm64" && "$HOST_ARCH" != "aarch64" && "$HOST_ARCH" != "arm64" ]]; then
        docker_args+=(--platform linux/arm64)
    fi

    docker run "${docker_args[@]}" \
        -v "$REPO_ROOT:/workspace:ro" \
        -v "$PACKAGE_JSON:/test_package.json:ro" \
        -v "$SCRIPT_DIR/linux_ide_v1_test.sh:/linux_ide_v1_test.sh:ro" \
        ubuntu:22.04 \
        bash /linux_ide_v1_test.sh \
            "$ARDUINO_IDE_VERSION" \
            "/test_package.json" \
            "$SKETCH_PATH" \
            "${RELEASE_VERSION:-}"
}

# Run test in a Windows container
run_windows_test() {
    echo "--- Testing: windows ---"

    docker run --rm \
        -v "$REPO_ROOT:C:\\workspace:ro" \
        -v "$PACKAGE_JSON:C:\\test_package.json:ro" \
        -v "$SCRIPT_DIR/windows_ide_v1_test.ps1:C:\\windows_ide_v1_test.ps1:ro" \
        mcr.microsoft.com/windows/servercore:ltsc2022 \
        powershell -File C:\\windows_ide_v1_test.ps1 \
            -ArduinoIdeVersion "$ARDUINO_IDE_VERSION" \
            -PackageJsonPath "C:\\test_package.json" \
            -SketchPath "$SKETCH_PATH" \
            -ReleaseVersion "${RELEASE_VERSION:-}"
}

# Run test natively on macOS
run_macos_native_test() {
    echo "--- Testing: macos (native) ---"

    local tmp_dir
    tmp_dir="$(mktemp -d)"
    trap "rm -rf '$tmp_dir'" RETURN

    echo "=== Downloading Arduino IDE $ARDUINO_IDE_VERSION ==="
    local ide_url="https://downloads.arduino.cc/arduino-${ARDUINO_IDE_VERSION}-macosx.zip"
    curl -sL -o "$tmp_dir/arduino.zip" "$ide_url"
    unzip -q "$tmp_dir/arduino.zip" -d "$tmp_dir/"
    rm "$tmp_dir/arduino.zip"

    local arduino_app="$tmp_dir/Arduino.app"
    local arduino_cmd="$arduino_app/Contents/MacOS/Arduino"

    if [ ! -f "$arduino_cmd" ]; then
        echo "ERROR: Arduino binary not found at $arduino_cmd"
        ls -la "$tmp_dir/"
        return 1
    fi

    echo "=== Installing ESP32 core via IDE v1 ==="
    local package_url="file://${PACKAGE_JSON}"
    local version_arg=""
    if [ -n "${RELEASE_VERSION:-}" ]; then
        version_arg=":${RELEASE_VERSION}"
    fi

    "$arduino_cmd" \
        --pref "boardsmanager.additional.urls=$package_url" \
        --install-boards "esp32:esp32${version_arg}" 2>&1 | tee /tmp/install_output.txt
    local install_rc=${PIPESTATUS[0]}

    if [ $install_rc -ne 0 ]; then
        if grep -q "Platform is already installed" /tmp/install_output.txt; then
            echo "Core already installed, skipping installation"
        else
            echo "ERROR: Failed to install ESP32 core"
            return 1
        fi
    fi

    echo "=== Compiling test sketch ==="
    if ! "$arduino_cmd" \
        --pref "boardsmanager.additional.urls=$package_url" \
        --verify \
        --board esp32:esp32:esp32 \
        "$REPO_ROOT/${SKETCH_PATH}"; then
        echo "ERROR: Compilation failed"
        return 1
    fi

    echo "=== SUCCESS ==="
    return 0
}

# Execute tests for each platform
echo ""
echo "==========================================="
echo "  Running Platform Tests"
echo "==========================================="
echo ""

# Linux x86_64
if can_run_linux_amd64; then
    if run_linux_test "linux64 (x86_64)" "linux/amd64"; then
        RESULT_linux64="PASS"
    else
        RESULT_linux64="FAIL"
    fi
else
    RESULT_linux64="SKIPPED (no x86_64 support - host is $HOST_ARCH, no QEMU/buildx)"
fi

echo ""

# Linux arm64
if can_run_linux_arm64; then
    if run_linux_test "linuxarm (arm64/aarch64)" "linux/arm64"; then
        RESULT_linuxarm="PASS"
    else
        RESULT_linuxarm="FAIL"
    fi
else
    RESULT_linuxarm="SKIPPED (no arm64 support - host is $HOST_ARCH, no QEMU/buildx)"
fi

echo ""

# Windows
if can_run_windows; then
    if run_windows_test; then
        RESULT_windows="PASS"
    else
        RESULT_windows="FAIL"
    fi
else
    RESULT_windows="SKIPPED (not a Windows host or Windows containers not enabled)"
fi

# macOS – run natively on the host if we're on macOS
if [[ "$HOST_OS" == "Darwin" ]]; then
    if run_macos_native_test; then
        RESULT_macos="PASS"
    else
        RESULT_macos="FAIL"
    fi
else
    RESULT_macos="SKIPPED (not a macOS host)"
fi

# Print results
echo ""
echo "==========================================="
echo "  IDE v1 Test Matrix Results"
echo "==========================================="
echo ""

has_failure=0
for platform in linux64 linuxarm windows macos; do
    eval "result=\$RESULT_${platform}"
    printf "  %-10s %s\n" "$platform:" "$result"
    if [[ "$result" == "FAIL" ]]; then
        has_failure=1
    fi
done

echo ""
echo "==========================================="

if [ "$has_failure" -eq 1 ]; then
    echo "  RESULT: Some tests FAILED"
    exit 1
else
    echo "  RESULT: All testable platforms passed"
    exit 0
fi
