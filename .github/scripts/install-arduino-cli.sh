#!/bin/bash

SCRIPTS_DIR_CLI="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPTS_DIR_CLI}/env.sh"

export ARDUINO_IDE_PATH="$HOME/bin"
if [ ! -d "$ARDUINO_IDE_PATH" ] || [ ! -f "$ARDUINO_IDE_PATH/arduino-cli" ]; then
    echo "Installing Arduino CLI on $OS_NAME ..."
    mkdir -p "$ARDUINO_IDE_PATH"
    if [ "$OS_IS_WINDOWS" == "1" ]; then
        curl -fsSL https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Windows_64bit.zip -o arduino-cli.zip
        unzip -q arduino-cli.zip -d "$ARDUINO_IDE_PATH"
        rm arduino-cli.zip
    else
        curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR="$ARDUINO_IDE_PATH" sh
    fi
fi
