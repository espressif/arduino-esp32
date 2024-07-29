#!/bin/bash

OSBITS=`uname -m`
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

if [ "$OS_IS_MACOS" == "1" ]; then
    export ARDUINO_IDE_PATH="$HOME/bin"
    export ARDUINO_USR_PATH="$HOME/Documents/Arduino"
elif [ "$OS_IS_WINDOWS" == "1" ]; then
    export ARDUINO_IDE_PATH="$HOME/bin"
    export ARDUINO_USR_PATH="$HOME/Documents/Arduino"
else
    export ARDUINO_IDE_PATH="$HOME/bin"
    export ARDUINO_USR_PATH="$HOME/Arduino"
fi

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

