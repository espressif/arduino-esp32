#!/bin/bash

SCRIPTS_DIR_CORE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPTS_DIR_CORE}/env.sh"

_install_platform_tools() {
    local tools_dir="$1"
    local args=(--clean-unused)
    if [ "${ESP32_LIBS_CACHE_HIT:-}" = "1" ]; then
        echo "Libs cache hit; skipping get.py archive verification"
        args+=(--skip-verify)
    fi
    if [ "${OS_IS_WINDOWS:-0}" == "1" ]; then
        (cd "$tools_dir" && ./get.exe "${args[@]}")
    else
        (cd "$tools_dir" && python get.py "${args[@]}")
    fi
}

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
    _install_platform_tools tools
    cd "$script_init_path" || exit

    echo "ESP32 Arduino has been installed in '$ARDUINO_ESP32_PATH'"
    echo ""
elif [ ! -d "$ARDUINO_ESP32_PATH/tools/xtensa-esp-elf" ] && \
     [ ! -d "$ARDUINO_ESP32_PATH/tools/riscv32-esp-elf" ]; then
    echo "ESP32 core linked but toolchains missing; running get.py ..."
    script_init_path="$PWD"
    _install_platform_tools "$ARDUINO_ESP32_PATH/tools"
    cd "$script_init_path" || exit
fi
