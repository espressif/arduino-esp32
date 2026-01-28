#!/bin/bash
# Test package JSON installation and compilation
# Disable shellcheck warning about $? uses.
# shellcheck disable=SC2181

set -e

SCRIPTS_DIR="./.github/scripts"
OUTPUT_DIR="$GITHUB_WORKSPACE/build"
PACKAGE_JSON_DEV="package_esp32_dev_index.json"
PACKAGE_JSON_REL="package_esp32_index.json"

# Get release info
RELEASE_PRE="${RELEASE_PRE:-false}"

# Convert path to absolute and handle Windows paths for file:// URLs
function get_file_url {
    local file_path="$1"

    # Get absolute path
    if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
        # On Windows, convert to absolute path and use forward slashes
        abs_path=$(cd "$(dirname "$file_path")" && pwd -W)/$(basename "$file_path") 2>/dev/null || echo "$file_path"
        # Ensure forward slashes and proper file:// format for Windows
        abs_path="${abs_path//\\//}"
        echo "file:///$abs_path"
    else
        # On Unix systems, just ensure absolute path
        if [[ "$file_path" = /* ]]; then
            echo "file://$file_path"
        else
            echo "file://$(cd "$(dirname "$file_path")" && pwd)/$(basename "$file_path")"
        fi
    fi
}

echo "Installing arduino-cli ..."
# Set up PATH based on OS
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    export PATH="$HOME/bin:$PATH"
else
    export PATH="/home/runner/bin:$HOME/bin:$PATH"
fi

source "${SCRIPTS_DIR}/install-arduino-cli.sh"

# For the Chinese mirror, we can't test the package JSONs as the Chinese mirror might not be updated yet.
# So we only test the main package JSON files.

echo ""
echo "==========================================="
echo "Testing $PACKAGE_JSON_DEV"
echo "==========================================="

echo "Installing esp32 core ..."
package_json_url=$(get_file_url "$OUTPUT_DIR/$PACKAGE_JSON_DEV")
echo "Package JSON URL: $package_json_url"
arduino-cli core install esp32:esp32 --additional-urls "$package_json_url"
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to install esp32 ($?)"
    exit 1
fi

echo "Compiling example ..."
arduino-cli compile --fqbn esp32:esp32:esp32 "$GITHUB_WORKSPACE"/libraries/ESP32/examples/CI/CIBoardsTest/CIBoardsTest.ino
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to compile example ($?)"
    exit 1
fi

echo "Uninstalling esp32 core ..."
arduino-cli core uninstall esp32:esp32
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to uninstall esp32 ($?)"
    exit 1
fi

echo "✓ Test successful for $PACKAGE_JSON_DEV"

if [ "$RELEASE_PRE" == "false" ]; then
    echo ""
    echo "==========================================="
    echo "Testing $PACKAGE_JSON_REL"
    echo "==========================================="

    echo "Installing esp32 core ..."
    package_json_url=$(get_file_url "$OUTPUT_DIR/$PACKAGE_JSON_REL")
    echo "Package JSON URL: $package_json_url"
    arduino-cli core install esp32:esp32 --additional-urls "$package_json_url"
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to install esp32 ($?)"
        exit 1
    fi

    echo "Compiling example ..."
    arduino-cli compile --fqbn esp32:esp32:esp32 "$GITHUB_WORKSPACE"/libraries/ESP32/examples/CI/CIBoardsTest/CIBoardsTest.ino
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to compile example ($?)"
        exit 1
    fi

    echo "Uninstalling esp32 core ..."
    arduino-cli core uninstall esp32:esp32
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to uninstall esp32 ($?)"
        exit 1
    fi

    echo "✓ Test successful for $PACKAGE_JSON_REL"
fi

echo ""
echo "==========================================="
echo "✓ All tests passed on $(uname -s)!"
echo "==========================================="
