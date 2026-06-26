#!/bin/bash
# Test package JSON installation and compilation
# Disable shellcheck warning about $? uses.
# shellcheck disable=SC2181

set -e

SCRIPTS_DIR="./.github/scripts"

source "${SCRIPTS_DIR}/env.sh"

OUTPUT_DIR="$GITHUB_WORKSPACE/build"
PACKAGE_JSON_DEV="package_esp32_dev_index.json"
PACKAGE_JSON_REL="package_esp32_index.json"
RELEASE_PRE="${RELEASE_PRE:-false}"

# Strip a leading 'v' to normalise tags like v3.3.9 → 3.3.9.
# Empty when run outside of a GitHub Actions release event.
RELEASE_VERSION="${GITHUB_REF_NAME#v}"

# Convert path to absolute and handle Windows paths for file:// URLs
function get_file_url {
    local file_path="$1"

    if [[ "$OS_IS_WINDOWS" == "1" ]]; then
        # On Windows use cygpath to produce a proper Windows-style path that
        # native binaries (arduino-cli) can open.  cygpath is always available
        # in Git Bash / MSYS2 and handles every quirk of drive-letter paths.
        local abs_path
        abs_path=$(cygpath -m "$(realpath "$file_path" 2>/dev/null || echo "$file_path")")
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

# Verify that the installed esp32 core version matches the release tag.
function verify_installed_version {
    if [ -z "$RELEASE_VERSION" ]; then
        echo "WARNING: GITHUB_REF_NAME is not set; skipping version check"
        return 0
    fi

    local installed_version
    installed_version=$(arduino-cli core list 2>/dev/null | awk '/^esp32:esp32[[:space:]]/{print $2}')

    if [ "$installed_version" != "$RELEASE_VERSION" ]; then
        echo "ERROR: Expected esp32:esp32@$RELEASE_VERSION but found esp32:esp32@${installed_version:-<none>}"
        echo "       The local package JSON was likely not loaded properly."
        return 1
    fi
    echo "✓ Verified esp32:esp32@$RELEASE_VERSION is installed"
}

echo "Installing arduino-cli ..."
# Set up PATH based on OS
if [[ "$OS_IS_WINDOWS" == "1" ]]; then
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
arduino-cli core install "esp32:esp32${RELEASE_VERSION:+@$RELEASE_VERSION}" --additional-urls "$package_json_url"
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to install esp32 ($?)"
    exit 1
fi

verify_installed_version || exit 1

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
    arduino-cli core install "esp32:esp32${RELEASE_VERSION:+@$RELEASE_VERSION}" --additional-urls "$package_json_url"
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to install esp32 ($?)"
        exit 1
    fi

    verify_installed_version || exit 1

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
