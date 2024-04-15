#!/bin/bash

set -e

export ARDUINO_BUILD_DIR="$HOME/.arduino/build.tmp"

function build(){
    local target=$1
    local fqbn=$2
    local chunk_index=$3
    local chunks_cnt=$4
    shift; shift; shift; shift;
    local sketches=$*

    local BUILD_SKETCH="${SCRIPTS_DIR}/sketch_utils.sh build"
    local BUILD_SKETCHES="${SCRIPTS_DIR}/sketch_utils.sh chunk_build"

    local args="-ai $ARDUINO_IDE_PATH -au $ARDUINO_USR_PATH"

    args+=" -t $target -fqbn $fqbn"

    if [ "$OS_IS_LINUX" == "1" ]; then
        args+=" -p $ARDUINO_ESP32_PATH/libraries"
        args+=" -i $chunk_index -m $chunks_cnt"
        ${BUILD_SKETCHES} ${args}
    else
        for sketch in ${sketches}; do
            local sargs="$args -s $(dirname $sketch)"
            if [ "$OS_IS_WINDOWS" == "1" ] && [ -d "$ARDUINO_IDE_PATH/tools-builder" ]; then
                local ctags_version=`ls "$ARDUINO_IDE_PATH/tools-builder/ctags/"`
                local preprocessor_version=`ls "$ARDUINO_IDE_PATH/tools-builder/arduino-preprocessor/"`
                win_opts="-prefs=runtime.tools.ctags.path=$ARDUINO_IDE_PATH/tools-builder/ctags/$ctags_version
                -prefs=runtime.tools.arduino-preprocessor.path=$ARDUINO_IDE_PATH/tools-builder/arduino-preprocessor/$preprocessor_version"
                sargs+=" ${win_opts}"
            fi
            ${BUILD_SKETCH} ${sargs}
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
    #source ${SCRIPTS_DIR}/install-arduino-ide.sh
    source ${SCRIPTS_DIR}/install-arduino-cli.sh
    source ${SCRIPTS_DIR}/install-arduino-core-esp32.sh

    FQBN_ESP32="espressif:esp32:esp32:PSRAM=enabled,PartitionScheme=huge_app"
    FQBN_ESP32S2="espressif:esp32:esp32s2:PSRAM=enabled,PartitionScheme=huge_app"
    FQBN_ESP32S3="espressif:esp32:esp32s3:PSRAM=opi,USBMode=default,PartitionScheme=huge_app"
    FQBN_ESP32C3="espressif:esp32:esp32c3:PartitionScheme=huge_app"
    FQBN_ESP32C6="espressif:esp32:esp32c6:PartitionScheme=huge_app"
    FQBN_ESP32H2="espressif:esp32:esp32h2:PartitionScheme=huge_app"

    SKETCHES_ESP32="\
      $ARDUINO_ESP32_PATH/libraries/NetworkClientSecure/examples/WiFiClientSecure/WiFiClientSecure.ino\
      $ARDUINO_ESP32_PATH/libraries/BLE/examples/Server/Server.ino\
      $ARDUINO_ESP32_PATH/libraries/ESP32/examples/Camera/CameraWebServer/CameraWebServer.ino\
      $ARDUINO_ESP32_PATH/libraries/Insights/examples/MinimalDiagnostics/MinimalDiagnostics.ino\
    "
    #create sizes_file and echo start of JSON array with "boards" key
    sizes_file="$GITHUB_WORKSPACE/cli_compile_$CHUNK_INDEX.json"
    echo "{\"boards\": [" > $sizes_file

    #build sketches for different targets
    build "esp32s3" $FQBN_ESP32S3 $CHUNK_INDEX $CHUNKS_CNT $SKETCHES_ESP32
    build "esp32s2" $FQBN_ESP32S2 $CHUNK_INDEX $CHUNKS_CNT $SKETCHES_ESP32
    build "esp32c3" $FQBN_ESP32C3 $CHUNK_INDEX $CHUNKS_CNT $SKETCHES_ESP32
    build "esp32c6" $FQBN_ESP32C6 $CHUNK_INDEX $CHUNKS_CNT $SKETCHES_ESP32
    build "esp32h2" $FQBN_ESP32H2 $CHUNK_INDEX $CHUNKS_CNT $SKETCHES_ESP32
    build "esp32"   $FQBN_ESP32   $CHUNK_INDEX $CHUNKS_CNT $SKETCHES_ESP32


    echo "Debug(board) - removing last comma from the last JSON object"

    #remove last comma from the last JSON object
    sed -i '$ s/.$//' "$sizes_file"
    #echo end of JSON array
    echo "]}" >> $sizes_file

else
    source ${SCRIPTS_DIR}/install-platformio-esp32.sh
    # PlatformIO ESP32 Test
    BOARD="esp32dev"
    OPTIONS="board_build.partitions = huge_app.csv"
    build_pio_sketch "$BOARD" "$OPTIONS" "$PLATFORMIO_ESP32_PATH/libraries/WiFi/examples/WiFiClient/WiFiClient.ino" && \
    build_pio_sketch "$BOARD" "$OPTIONS" "$PLATFORMIO_ESP32_PATH/libraries/NetworkClientSecure/examples/WiFiClientSecure/WiFiClientSecure.ino" && \
    build_pio_sketch "$BOARD" "$OPTIONS" "$PLATFORMIO_ESP32_PATH/libraries/BluetoothSerial/examples/SerialToSerialBT/SerialToSerialBT.ino" && \
    build_pio_sketch "$BOARD" "$OPTIONS" "$PLATFORMIO_ESP32_PATH/libraries/BLE/examples/Server/Server.ino" && \
    build_pio_sketch "$BOARD" "$OPTIONS" "$PLATFORMIO_ESP32_PATH/libraries/ESP32/examples/Camera/CameraWebServer/CameraWebServer.ino"

    # Basic sanity testing for other series
    for board in "esp32-c3-devkitm-1" "esp32-s2-saola-1" "esp32-s3-devkitc-1"
    do
        python -m platformio ci --board "$board" "$PLATFORMIO_ESP32_PATH/libraries/WiFi/examples/WiFiClient" --project-option="board_build.partitions = huge_app.csv"
    done

    #build_pio_sketches "$BOARD" "$OPTIONS" "$PLATFORMIO_ESP32_PATH/libraries"
fi
