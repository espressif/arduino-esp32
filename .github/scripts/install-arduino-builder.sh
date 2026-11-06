#!/bin/bash
# Slim arduino-nightly tree for headless builder upload CI (not full Arduino IDE 1.8.19).

SCRIPTS_DIR_BUILDER="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPTS_DIR_BUILDER}/env.sh"

ARDUINO_NIGHTLY_VERSION="${ARDUINO_NIGHTLY_VERSION:-1.0.4}"
ARDUINO_BUILDER_URL="https://github.com/espressif/arduino-esp32/releases/download/${ARDUINO_NIGHTLY_VERSION}/arduino-nightly-"

if [ "$OS_IS_LINUX" == "1" ]; then
    ARCHIVE_FORMAT="tar.xz"
elif [ "$OS_IS_WINDOWS" == "1" ] || [ "$OS_IS_MACOS" == "1" ]; then
    ARCHIVE_FORMAT="zip"
else
    echo "ERROR: Unsupported OS for arduino-builder installation"
    exit 1
fi

if [ "$OS_IS_MACOS" == "1" ]; then
    export ARDUINO_BUILDER_PATH="$HOME/arduino_nightly/Arduino.app/Contents/Java"
else
    export ARDUINO_BUILDER_PATH="$HOME/arduino_ide"
fi

if [ ! -d "$ARDUINO_BUILDER_PATH" ]; then
    echo "Installing arduino-nightly builder on $OS_NAME ..."
    archive="arduino.${ARCHIVE_FORMAT}"
    echo "Downloading '${ARDUINO_BUILDER_URL}${OS_NAME}.${ARCHIVE_FORMAT}' ..."
    if [ "$OS_IS_LINUX" == "1" ]; then
        wget -q -O "$archive" "${ARDUINO_BUILDER_URL}${OS_NAME}.${ARCHIVE_FORMAT}"
        echo "Extracting '$archive' ..."
        tar xf "$archive"
        mv arduino-nightly "$ARDUINO_BUILDER_PATH"
    else
        curl -fsSL -o "$archive" -L "${ARDUINO_BUILDER_URL}${OS_NAME}.${ARCHIVE_FORMAT}"
        echo "Extracting '$archive' ..."
        unzip -q "$archive"
        if [ "$OS_IS_MACOS" == "1" ]; then
            mkdir -p "$HOME/arduino_nightly"
            mv "Arduino.app" "$HOME/arduino_nightly/Arduino.app"
        else
            mv arduino-nightly "$ARDUINO_BUILDER_PATH"
        fi
    fi
    rm -f "$archive"

    mkdir -p "$ARDUINO_USR_PATH/libraries"
    mkdir -p "$ARDUINO_USR_PATH/hardware"

    echo "arduino-nightly builder installed in '$ARDUINO_BUILDER_PATH'"
    echo ""
fi
