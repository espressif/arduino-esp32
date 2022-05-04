#!/bin/bash

SCRIPTS_DIR="./.github/scripts"
BUILD_CMD=""

if [ $# -eq 3 ]; then
    chunk_build=1
elif [ $# -eq 2 ]; then
    chunk_build=0
else
  echo "ERROR: Illegal number of parameters"
  echo "USAGE:
        ${0} <target> <sketch_dir>
        ${0} <target> <chunk> <total_chunks>
        "
  exit 0
fi

target=$1

case "$target" in
    "esp32") fqbn="espressif:esp32:esp32:PSRAM=enabled,PartitionScheme=huge_app"
    ;;
    "esp32s2") fqbn="espressif:esp32:esp32s2:PSRAM=enabled,PartitionScheme=huge_app"
    ;;
    "esp32c3") fqbn="espressif:esp32:esp32c3:PartitionScheme=huge_app"
    ;;
    "esp32s3") fqbn="espressif:esp32:esp32s3:PSRAM=opi,USBMode=default,PartitionScheme=huge_app"
    ;;
esac

if [ -z $fqbn ]; then
  echo "Unvalid chip $1"
  exit 0
fi

source ${SCRIPTS_DIR}/install-arduino-ide.sh
source ${SCRIPTS_DIR}/install-arduino-core-esp32.sh

args="$ARDUINO_IDE_PATH $ARDUINO_USR_PATH \"$fqbn\""

if [ $chunk_build -eq 1 ]; then
    chunk_index=$2
    chunk_max=$3

    if [ "$chunk_index" -gt "$chunk_max" ] &&  [ "$chunk_max" -ge 2 ]; then
        chunk_index=$chunk_max
    fi
    BUILD_CMD="${SCRIPTS_DIR}/sketch_utils.sh chunk_build"
    args+=" $target $PWD/tests $chunk_index $chunk_max"
else
    sketchdir=$2
    BUILD_CMD="${SCRIPTS_DIR}/sketch_utils.sh build"
    args+=" $PWD/tests/$sketchdir/$sketchdir.ino"
fi

${BUILD_CMD} ${args}

