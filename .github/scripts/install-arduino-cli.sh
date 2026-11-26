#!/bin/bash

SCRIPTS_DIR_CLI="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPTS_DIR_CLI}/env.sh"

export ARDUINO_IDE_PATH="$HOME/bin"
if [ ! -d "$ARDUINO_IDE_PATH" ] || [ ! -f "$ARDUINO_IDE_PATH/arduino-cli" ]; then
    echo "Installing Arduino CLI on $OS_NAME ..."
    mkdir -p "$ARDUINO_IDE_PATH"
    if [ "$OS_IS_WINDOWS" == "1" ]; then
        curl -fsSL --retry 5 --retry-all-errors --retry-delay 2 --connect-timeout 30 \
            https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Windows_64bit.zip -o arduino-cli.zip
        unzip -q arduino-cli.zip -d "$ARDUINO_IDE_PATH"
        rm arduino-cli.zip
    else
        install_script="$(mktemp)"
        rc=0
        if curl -fsSL --retry 5 --retry-all-errors --retry-delay 2 --connect-timeout 30 \
            -o "$install_script" \
            https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh; then
            BINDIR="$ARDUINO_IDE_PATH" sh "$install_script" || rc=$?
        else
            rc=$?
        fi
        rm -f "$install_script"
        if [ "$rc" -ne 0 ]; then
            if (return 0 2>/dev/null); then
                return "$rc"
            fi
            exit "$rc"
        fi
    fi
fi
