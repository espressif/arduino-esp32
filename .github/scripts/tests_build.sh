#!/bin/bash

target=$1
chunk_index=$2
chunk_max=$3

if [ "$chunk_index" -gt "$chunk_max" ] &&  [ "$chunk_max" -ge 2 ]; then
    chunk_index=$chunk_max
fi

case "$target" in
    "esp32") fqbn="espressif:esp32:esp32:PSRAM=enabled,PartitionScheme=huge_app"
    ;;
    "esp32s2") fqbn="espressif:esp32:esp32s2:PSRAM=enabled,PartitionScheme=huge_app"
    ;;
    "esp32c3") fqbn="espressif:esp32:esp32c3:PartitionScheme=huge_app"
    ;;
esac

if [ -z $fqbn ]; then
  echo "Unvalid chip $1"
  exit 0
fi

SCRIPTS_DIR="./.github/scripts"
BUILD_SKETCHES="${SCRIPTS_DIR}/sketch_utils.sh chunk_build"

source ${SCRIPTS_DIR}/install-arduino-ide.sh
source ${SCRIPTS_DIR}/install-arduino-core-esp32.sh

args="$ARDUINO_IDE_PATH $ARDUINO_USR_PATH"
args+=" \"$fqbn\"  $target $PWD/tests $chunk_index $chunk_max"
${BUILD_SKETCHES} ${args}

