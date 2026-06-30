#!/bin/bash

SCRIPTS_DIR_CORE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPTS_DIR_CORE}/env.sh"

if [ ! -d "$ARDUINO_ESP32_PATH" ]; then
    echo "Installing ESP32 Arduino Core ..."
    script_init_path="$PWD"
    mkdir -p "$ARDUINO_USR_PATH/hardware/espressif"
    cd "$ARDUINO_USR_PATH/hardware/espressif" || exit

    echo "Installing Python Serial ..."
    pip install pyserial > /dev/null

    if [ "${OS_IS_WINDOWS:-0}" == "1" ]; then
        echo "Installing Python Requests ..."
        pip install requests > /dev/null
    fi

    if [ -n "${GITHUB_REPOSITORY:-}" ]; then
        echo "Linking Core..."
        ln -sfn "$GITHUB_WORKSPACE" esp32
    else
        echo "Cloning Core Repository..."
        git clone https://github.com/espressif/arduino-esp32.git esp32 > /dev/null 2>&1
    fi

    cd esp32 || exit
    echo "Installing Platform Tools ..."
    if [ "${OS_IS_WINDOWS:-0}" == "1" ]; then
        (cd tools && ./get.exe)
    else
        (cd tools && python get.py)
    fi
    cd "$script_init_path" || exit

    echo "ESP32 Arduino has been installed in '$ARDUINO_ESP32_PATH'"
    echo ""
elif [ ! -d "$ARDUINO_ESP32_PATH/tools/xtensa-esp-elf" ] && \
     [ ! -d "$ARDUINO_ESP32_PATH/tools/riscv32-esp-elf" ]; then
    echo "ESP32 core linked but toolchains missing; running get.py ..."
    script_init_path="$PWD"
    if [ "${OS_IS_WINDOWS:-0}" == "1" ]; then
        (cd "$ARDUINO_ESP32_PATH/tools" && ./get.exe)
    else
        (cd "$ARDUINO_ESP32_PATH/tools" && python get.py)
    fi
    cd "$script_init_path" || exit
fi
