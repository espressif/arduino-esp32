#!/bin/bash
# Common environment setup for build and test scripts
# Sets: OS_IS_*, OS_NAME, ARDUINO_USR_PATH, ARDUINO_ESP32_PATH,
#       GITHUB_WORKSPACE, REPO_ROOT, SDKCONFIG_DIR

# Skip if already sourced
[ -n "$ENV_SOURCED" ] && return 0
ENV_SOURCED=1

ENV_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# OS detection
OSBITS=$(uname -m)
if [[ "$OSTYPE" == "linux"* ]]; then
    export OS_IS_LINUX="1"
    if [[ "$OSBITS" == "i686" ]]; then
        OS_NAME="linux32"
    elif [[ "$OSBITS" == "x86_64" ]]; then
        OS_NAME="linux64"
    elif [[ "$OSBITS" == "armv7l" || "$OSBITS" == "aarch64" ]]; then
        OS_NAME="linuxarm"
    else
        OS_NAME="$OSTYPE-$OSBITS"
        echo "Unknown OS '$OS_NAME'"
        exit 1
    fi
elif [[ "$OSTYPE" == "darwin"* ]]; then
    export OS_IS_MACOS="1"
    OS_NAME="macosx"
elif [[ "$OSTYPE" == "cygwin" ]] || [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
    export OS_IS_WINDOWS="1"
    OS_NAME="windows"
else
    OS_NAME="$OSTYPE-$OSBITS"
    echo "Unknown OS '$OS_NAME'"
    exit 1
fi
export OS_NAME

# Arduino paths
if [ "$OS_IS_MACOS" == "1" ]; then
    export ARDUINO_USR_PATH="$HOME/Documents/Arduino"
elif [ "$OS_IS_WINDOWS" == "1" ]; then
    export ARDUINO_USR_PATH="$HOME/Documents/Arduino"
else
    export ARDUINO_USR_PATH="$HOME/Arduino"
fi

export ARDUINO_ESP32_PATH="$ARDUINO_USR_PATH/hardware/espressif/esp32"

# GITHUB_WORKSPACE fallback for local runs
if [ -z "$GITHUB_WORKSPACE" ]; then
    export GITHUB_WORKSPACE="$(cd "$ENV_DIR/../.." && pwd)"
fi

# Repository root and sdkconfig paths
if [ -d "$ARDUINO_ESP32_PATH/tools/esp32-arduino-libs" ]; then
    REPO_ROOT="$ARDUINO_ESP32_PATH"
elif [ -d "$GITHUB_WORKSPACE/tools/esp32-arduino-libs" ]; then
    REPO_ROOT="$GITHUB_WORKSPACE"
else
    REPO_ROOT="$(cd "$ENV_DIR/../.." && pwd)"
fi

export REPO_ROOT
export SDKCONFIG_DIR="$REPO_ROOT/tools/esp32-arduino-libs"
