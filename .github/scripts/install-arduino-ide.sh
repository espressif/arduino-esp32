#!/bin/bash

SCRIPTS_DIR_IDE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPTS_DIR_IDE}/env.sh"

ARDUINO_IDE_VERSION="1.8.19"

if [ "$OS_IS_LINUX" == "1" ]; then
    ARCHIVE_FORMAT="tar.xz"
    ARDUINO_IDE_URL="https://downloads.arduino.cc/arduino-${ARDUINO_IDE_VERSION}-linux64.tar.xz"
elif [ "$OS_IS_WINDOWS" == "1" ]; then
    ARCHIVE_FORMAT="zip"
    ARDUINO_IDE_URL="https://downloads.arduino.cc/arduino-${ARDUINO_IDE_VERSION}-windows.zip"
elif [ "$OS_IS_MACOS" == "1" ]; then
    ARCHIVE_FORMAT="zip"
    ARDUINO_IDE_URL="https://downloads.arduino.cc/arduino-${ARDUINO_IDE_VERSION}-macosx.zip"
else
    echo "ERROR: Unsupported OS for Arduino IDE installation"
    exit 1
fi

if [ "$OS_IS_MACOS" == "1" ]; then
    if [ -d "/Applications/Arduino.app/Contents/Java" ]; then
        export ARDUINO_IDE_PATH="/Applications/Arduino.app/Contents/Java"
    else
        export ARDUINO_IDE_PATH="$HOME/arduino-ide-${ARDUINO_IDE_VERSION}/Arduino.app/Contents/Java"
    fi
elif [ "$OS_IS_WINDOWS" == "1" ]; then
    export ARDUINO_IDE_PATH="$HOME/arduino_ide"
else
    export ARDUINO_IDE_PATH="$HOME/arduino_ide"
fi

if [ ! -d "$ARDUINO_IDE_PATH" ]; then
    echo "Installing Arduino IDE ${ARDUINO_IDE_VERSION} on $OS_NAME ..."
    echo "Downloading '$ARDUINO_IDE_URL' ..."
    if [ "$OS_IS_LINUX" == "1" ]; then
        wget -q -O "arduino.$ARCHIVE_FORMAT" "$ARDUINO_IDE_URL"
        echo "Extracting 'arduino.$ARCHIVE_FORMAT' ..."
        tar xf "arduino.$ARCHIVE_FORMAT"
        mv "arduino-${ARDUINO_IDE_VERSION}" "$ARDUINO_IDE_PATH"
    elif [ "$OS_IS_MACOS" == "1" ]; then
        mkdir -p "$HOME/arduino-ide-${ARDUINO_IDE_VERSION}"
        curl -fsSL -o "arduino.$ARCHIVE_FORMAT" "$ARDUINO_IDE_URL"
        echo "Extracting 'arduino.$ARCHIVE_FORMAT' ..."
        unzip -q "arduino.$ARCHIVE_FORMAT"
        mv "Arduino.app" "$HOME/arduino-ide-${ARDUINO_IDE_VERSION}/Arduino.app"
    else
        curl -fsSL -o "arduino.$ARCHIVE_FORMAT" "$ARDUINO_IDE_URL"
        echo "Extracting 'arduino.$ARCHIVE_FORMAT' ..."
        unzip -q "arduino.$ARCHIVE_FORMAT" > /dev/null
        mv "arduino-${ARDUINO_IDE_VERSION}" "$ARDUINO_IDE_PATH"
    fi
    rm -rf "arduino.$ARCHIVE_FORMAT"

    mkdir -p "$ARDUINO_USR_PATH/libraries"
    mkdir -p "$ARDUINO_USR_PATH/hardware"

    echo "Arduino IDE ${ARDUINO_IDE_VERSION} installed in '$ARDUINO_IDE_PATH'"
    echo ""
fi
