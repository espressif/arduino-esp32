#!/bin/bash
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
#
# Run inside a Linux Docker container: install Arduino IDE 1.x, ESP32 core, compile test sketch.
#
# Usage: linux_ide_v1_test.sh <arduino_ide_version> <package_json_path> <sketch_path> [release_version]

set -euo pipefail

ARDUINO_IDE_VERSION="${1:?arduino_ide_version required}"
PACKAGE_JSON_PATH="${2:?package_json_path required}"
SKETCH_PATH="${3:?sketch_path required}"
RELEASE_VERSION="${4:-}"

echo "=== Installing dependencies ==="
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq wget xz-utils xvfb libxrender1 libxtst6 libxi6 libfreetype6 libfontconfig1 python3 python3-serial > /dev/null 2>&1

echo "=== Downloading Arduino IDE $ARDUINO_IDE_VERSION ==="
ARCH=$(uname -m)
if [[ "$ARCH" == "x86_64" ]]; then
    IDE_URL="https://downloads.arduino.cc/arduino-${ARDUINO_IDE_VERSION}-linux64.tar.xz"
elif [[ "$ARCH" == "aarch64" || "$ARCH" == "arm64" ]]; then
    IDE_URL="https://downloads.arduino.cc/arduino-${ARDUINO_IDE_VERSION}-linuxaarch64.tar.xz"
else
    echo "ERROR: Unsupported architecture $ARCH"
    exit 1
fi

wget -q -O /tmp/arduino.tar.xz "$IDE_URL"
tar xf /tmp/arduino.tar.xz -C /opt/
rm /tmp/arduino.tar.xz

ARDUINO_DIR="/opt/arduino-${ARDUINO_IDE_VERSION}"
ARDUINO_CMD="$ARDUINO_DIR/arduino"

if [ ! -f "$ARDUINO_CMD" ]; then
    echo "ERROR: Arduino binary not found at $ARDUINO_CMD"
    ls -la /opt/
    exit 1
fi

echo "=== Installing ESP32 core via IDE v1 ==="
PACKAGE_URL="file://${PACKAGE_JSON_PATH}"
VERSION_ARG=""
if [ -n "$RELEASE_VERSION" ]; then
    VERSION_ARG=":${RELEASE_VERSION}"
fi

# Use xvfb for headless operation (IDE v1 GUI requires a display even in CLI mode)
# "Platform is already installed!" is treated as success
xvfb-run "$ARDUINO_CMD" \
    --pref "boardsmanager.additional.urls=$PACKAGE_URL" \
    --install-boards "esp32:esp32${VERSION_ARG}" 2>&1 | tee /tmp/install_output.txt
install_rc=${PIPESTATUS[0]}

if [ "$install_rc" -ne 0 ]; then
    if grep -q "Platform is already installed" /tmp/install_output.txt; then
        echo "Core already installed, skipping installation"
    else
        echo "ERROR: Failed to install ESP32 core"
        exit 1
    fi
fi

echo "=== Compiling test sketch ==="
if ! xvfb-run "$ARDUINO_CMD" \
    --pref "boardsmanager.additional.urls=$PACKAGE_URL" \
    --verify \
    --board esp32:esp32:esp32 \
    "/workspace/${SKETCH_PATH}"; then
    echo "ERROR: Compilation failed"
    exit 1
fi

echo "=== SUCCESS ==="
