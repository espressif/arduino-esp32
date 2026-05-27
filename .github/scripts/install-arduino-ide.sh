#!/bin/bash

#OSTYPE: 'linux-gnu', ARCH: 'x86_64' => linux64
#OSTYPE: 'msys', ARCH: 'x86_64' => win32
#OSTYPE: 'darwin18', ARCH: 'i386' => macos

SCRIPTS_DIR_IDE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPTS_DIR_IDE}/env.sh"

if [ "$OS_IS_LINUX" == "1" ]; then
    ARCHIVE_FORMAT="tar.xz"
elif [ "$OS_IS_WINDOWS" == "1" ]; then
    ARCHIVE_FORMAT="zip"
else
    ARCHIVE_FORMAT="zip"
fi

if [ "$OS_IS_MACOS" == "1" ]; then
    export ARDUINO_IDE_PATH="/Applications/Arduino.app/Contents/Java"
elif [ "$OS_IS_WINDOWS" == "1" ]; then
    export ARDUINO_IDE_PATH="$HOME/arduino_ide"
else
    export ARDUINO_IDE_PATH="$HOME/arduino_ide"
fi

# Updated as of Nov 3rd 2020
ARDUINO_IDE_URL="https://github.com/espressif/arduino-esp32/releases/download/1.0.4/arduino-nightly-"

# Currently not working
#ARDUINO_IDE_URL="https://www.arduino.cc/download.php?f=/arduino-nightly-"

if [ ! -d "$ARDUINO_IDE_PATH" ]; then
    echo "Installing Arduino IDE on $OS_NAME ..."
    echo "Downloading '$ARDUINO_IDE_URL$OS_NAME.$ARCHIVE_FORMAT' to 'arduino.$ARCHIVE_FORMAT' ..."
    if [ "$OS_IS_LINUX" == "1" ]; then
        wget -O "arduino.$ARCHIVE_FORMAT" "$ARDUINO_IDE_URL$OS_NAME.$ARCHIVE_FORMAT" > /dev/null 2>&1
        echo "Extracting 'arduino.$ARCHIVE_FORMAT' ..."
        tar xf "arduino.$ARCHIVE_FORMAT" > /dev/null
        mv arduino-nightly "$ARDUINO_IDE_PATH"
    else
        curl -o "arduino.$ARCHIVE_FORMAT" -L "$ARDUINO_IDE_URL$OS_NAME.$ARCHIVE_FORMAT" > /dev/null 2>&1
        echo "Extracting 'arduino.$ARCHIVE_FORMAT' ..."
        unzip "arduino.$ARCHIVE_FORMAT" > /dev/null
        if [ "$OS_IS_MACOS" == "1" ]; then
            mv "Arduino.app" "/Applications/Arduino.app"
        else
            mv arduino-nightly "$ARDUINO_IDE_PATH"
        fi
    fi
    rm -rf "arduino.$ARCHIVE_FORMAT"

    mkdir -p "$ARDUINO_USR_PATH/libraries"
    mkdir -p "$ARDUINO_USR_PATH/hardware"

    echo "Arduino IDE Installed in '$ARDUINO_IDE_PATH'"
    echo ""
fi
