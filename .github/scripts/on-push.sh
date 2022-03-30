#!/bin/bash

set -e

export ARDUINO_BUILD_DIR="$HOME/.arduino/build.tmp"

function build(){
    local target=$1
    local fqbn=$2
    local chunk_index=$3
    local chunks_cnt=$4
    local sketches=$5

    local BUILD_SKETCH="${SCRIPTS_DIR}/sketch_utils.sh build"
    local BUILD_SKETCHES="${SCRIPTS_DIR}/sketch_utils.sh chunk_build"

    local args="$ARDUINO_IDE_PATH $ARDUINO_USR_PATH"

    args+=" \"$fqbn\""

    if [ "$OS_IS_LINUX" == "1" ]; then
        args+=" $target"
        args+=" $ARDUINO_ESP32_PATH/libraries"
        args+=" $chunk_index $chunks_cnt"
        ${BUILD_SKETCHES} ${args}
    else
        if [ "$OS_IS_WINDOWS" == "1" ]; then
            local ctags_version=`ls "$ARDUINO_IDE_PATH/tools-builder/ctags/"`
            local preprocessor_version=`ls "$ARDUINO_IDE_PATH/tools-builder/arduino-preprocessor/"`
            win_opts="-prefs=runtime.tools.ctags.path=$ARDUINO_IDE_PATH/tools-builder/ctags/$ctags_version
            -prefs=runtime.tools.arduino-preprocessor.path=$ARDUINO_IDE_PATH/tools-builder/arduino-preprocessor/$preprocessor_version"
            args+=" ${win_opts}"
        fi

        for sketch in ${sketches}; do
            ${BUILD_SKETCH} ${args} ${sketch}
        done
    fi
}

if [ -z "$GITHUB_WORKSPACE" ]; then
    export GITHUB_WORKSPACE="$PWD"
    export GITHUB_REPOSITORY="espressif/arduino-esp32"
fi

CHUNK_INDEX=$1
CHUNKS_CNT=$2
BUILD_PIO=0
if [ "$#" -lt 2 ] || [ "$CHUNKS_CNT" -le 0 ]; then
    CHUNK_INDEX=0
    CHUNKS_CNT=1
elif [ "$CHUNK_INDEX" -gt "$CHUNKS_CNT" ] &&  [ "$CHUNKS_CNT" -ge 2 ]; then
    CHUNK_INDEX=$CHUNKS_CNT
elif [ "$CHUNK_INDEX" -eq "$CHUNKS_CNT" ]; then
    BUILD_PIO=1
fi

#echo "Updating submodules ..."
#git -C "$GITHUB_WORKSPACE" submodule update --init --recursive > /dev/null 2>&1

SCRIPTS_DIR="./.github/scripts"
if [ "$BUILD_PIO" -eq 0 ]; then
    source ${SCRIPTS_DIR}/install-arduino-ide.sh
    source ${SCRIPTS_DIR}/install-arduino-core-esp32.sh

    FQBN_ESP32="espressif:esp32:esp32:PSRAM=enabled,PartitionScheme=huge_app"
    FQBN_ESP32S2="espressif:esp32:esp32s2:PSRAM=enabled,PartitionScheme=huge_app"
    FQBN_ESP32S3="espressif:esp32:esp32s3:PSRAM=opi,USBMode=default,PartitionScheme=huge_app"
    FQBN_ESP32C3="espressif:esp32:esp32c3:PartitionScheme=huge_app"

    SKETCHES_ESP32="\
      $ARDUINO_ESP32_PATH/libraries/WiFiClientSecure/examples/WiFiClientSecure/WiFiClientSecure.ino\
      $ARDUINO_ESP32_PATH/libraries/BLE/examples/BLE_server/BLE_server.ino\
      $ARDUINO_ESP32_PATH/libraries/ESP32/examples/Camera/CameraWebServer/CameraWebServer.ino\
    "

    SKETCHES_ESP32XX="\
      $ARDUINO_ESP32_PATH/libraries/WiFiClientSecure/examples/WiFiClientSecure/WiFiClientSecure.ino\
      $ARDUINO_ESP32_PATH/libraries/WiFi/examples/WiFiClient/WiFiClient.ino\
    "

    build "esp32s3" $FQBN_ESP32S3 $CHUNK_INDEX $CHUNKS_CNT $SKETCHES_ESP32
    build "esp32s2" $FQBN_ESP32S2 $CHUNK_INDEX $CHUNKS_CNT $SKETCHES_ESP32XX
    build "esp32c3" $FQBN_ESP32C3 $CHUNK_INDEX $CHUNKS_CNT $SKETCHES_ESP32XX
    build "esp32" $FQBN_ESP32 $CHUNK_INDEX $CHUNKS_CNT $SKETCHES_ESP32
else
    source ${SCRIPTS_DIR}/install-platformio-esp32.sh
    # PlatformIO ESP32 Test
    BOARD="esp32dev"
    OPTIONS="board_build.partitions = huge_app.csv"
    build_pio_sketch "$BOARD" "$OPTIONS" "$PLATFORMIO_ESP32_PATH/libraries/WiFi/examples/WiFiClient/WiFiClient.ino" && \
    build_pio_sketch "$BOARD" "$OPTIONS" "$PLATFORMIO_ESP32_PATH/libraries/WiFiClientSecure/examples/WiFiClientSecure/WiFiClientSecure.ino" && \
    build_pio_sketch "$BOARD" "$OPTIONS" "$PLATFORMIO_ESP32_PATH/libraries/BluetoothSerial/examples/SerialToSerialBT/SerialToSerialBT.ino" && \
    build_pio_sketch "$BOARD" "$OPTIONS" "$PLATFORMIO_ESP32_PATH/libraries/BLE/examples/BLE_server/BLE_server.ino" && \
    build_pio_sketch "$BOARD" "$OPTIONS" "$PLATFORMIO_ESP32_PATH/libraries/ESP32/examples/Camera/CameraWebServer/CameraWebServer.ino"

    # PlatformIO ESP32 Test
    # OPTIONS="board_build.mcu = esp32s2"
    # build_pio_sketch "$BOARD" "$OPTIONS" "$PLATFORMIO_ESP32_PATH/libraries/WiFi/examples/WiFiClient/WiFiClient.ino" && \
    # build_pio_sketch "$BOARD" "$OPTIONS" "$PLATFORMIO_ESP32_PATH/libraries/WiFiClientSecure/examples/WiFiClientSecure/WiFiClientSecure.ino"

    python -m platformio ci --board "$BOARD" "$PLATFORMIO_ESP32_PATH/libraries/WiFi/examples/WiFiClient" --project-option="board_build.mcu = esp32s2" --project-option="board_build.partitions = huge_app.csv"
    python -m platformio ci --board "$BOARD" "$PLATFORMIO_ESP32_PATH/libraries/WiFi/examples/WiFiClient" --project-option="board_build.mcu = esp32c3" --project-option="board_build.partitions = huge_app.csv"

    echo "Hacking in S3 support ..."
    replace_script="import json; import os;"
    replace_script+="fp=open(os.path.expanduser('~/.platformio/platforms/espressif32/platform.json'), 'r+');"
    replace_script+="data=json.load(fp);"
    replace_script+="data['packages']['toolchain-xtensa-esp32']['optional']=True;"
    replace_script+="data['packages']['toolchain-xtensa-esp32s3']['optional']=False;"
    replace_script+="data['packages']['tool-esptoolpy']['owner']='tasmota';"
    replace_script+="data['packages']['tool-esptoolpy']['version']='https://github.com/tasmota/esptool/releases/download/v3.3/esptool-3.3.zip';"
    replace_script+="fp.seek(0);fp.truncate();json.dump(data, fp, indent=2);fp.close()"
    python -c "$replace_script"

    python -m platformio ci --board "$BOARD" "$PLATFORMIO_ESP32_PATH/libraries/WiFi/examples/WiFiClient" --project-option="board_build.mcu = esp32s3" --project-option="board_build.partitions = huge_app.csv"

    #build_pio_sketches "$BOARD" "$OPTIONS" "$PLATFORMIO_ESP32_PATH/libraries"
fi
