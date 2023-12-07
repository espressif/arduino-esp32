#!/bin/bash

USAGE="
USAGE:
    ${0} -c <chunk_build_opts>
       Example: ${0} -c -t esp32 -i 0 -m 15
    ${0} -s sketch_name <build_opts>
       Example: ${0} -s hello_world -t esp32
    ${0} -clean
       Remove build and test generated files
"

function clean(){
    rm -rf tests/*/build*/
    rm -rf tests/.pytest_cache
    rm -rf tests/*/__pycache__/
    rm -rf tests/*/*.xml
}

SCRIPTS_DIR="./.github/scripts"
BUILD_CMD=""

chunk_build=0

while [ ! -z "$1" ]; do
    case $1 in
    -c )
        chunk_build=1
        ;;
    -s )
        shift
        sketch=$1
        ;;
    -h )
        echo "$USAGE"
        exit 0
        ;;
    -clean )
        clean
        exit 0
        ;;
    * )
      break
      ;;
    esac
    shift
done

#source ${SCRIPTS_DIR}/install-arduino-ide.sh
source ${SCRIPTS_DIR}/install-arduino-cli.sh
source ${SCRIPTS_DIR}/install-arduino-core-esp32.sh

args="-ai $ARDUINO_IDE_PATH -au $ARDUINO_USR_PATH"

if [ $chunk_build -eq 1 ]; then
    BUILD_CMD="${SCRIPTS_DIR}/sketch_utils.sh chunk_build"
    args+=" -p $PWD/tests"
else
    BUILD_CMD="${SCRIPTS_DIR}/sketch_utils.sh build"
    args+=" -s $PWD/tests/$sketch"
fi

${BUILD_CMD} ${args} $*

