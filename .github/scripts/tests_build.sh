#!/bin/bash

USAGE="
USAGE:
    ${0} -c -type <test_type> <chunk_build_opts>
       Example: ${0} -c -type validation -t esp32 -i 0 -m 15
    ${0} -s sketch_name <build_opts>
       Example: ${0} -s hello_world -t esp32
    ${0} -clean
       Remove build and test generated files
"

function clean(){
    rm -rf tests/.pytest_cache
    find tests/ -type d -name 'build*' -exec rm -rf "{}" \+
    find tests/ -type d -name '__pycache__' -exec rm -rf "{}" \+
    find tests/ -name '*.xml' -exec rm -rf "{}" \+
    find tests/ -name 'result_*.json' -exec rm -rf "{}" \+
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
    -type )
        shift
        test_type=$1
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

source ${SCRIPTS_DIR}/install-arduino-cli.sh
source ${SCRIPTS_DIR}/install-arduino-core-esp32.sh

args="-ai $ARDUINO_IDE_PATH -au $ARDUINO_USR_PATH"

if [[ $test_type == "all" ]] || [[ -z $test_type ]]; then
    if [ -n "$sketch" ]; then
        tmp_sketch_path=$(find tests -name $sketch.ino)
        test_type=$(basename $(dirname $(dirname "$tmp_sketch_path")))
        echo "Sketch $sketch test type: $test_type"
        test_folder="$PWD/tests/$test_type"
    else
      test_folder="$PWD/tests"
    fi
else
    test_folder="$PWD/tests/$test_type"
fi

if [ $chunk_build -eq 1 ]; then
    BUILD_CMD="${SCRIPTS_DIR}/sketch_utils.sh chunk_build"
    args+=" -p $test_folder -i 0 -m 1"
else
    BUILD_CMD="${SCRIPTS_DIR}/sketch_utils.sh build"
    args+=" -s $test_folder/$sketch"
fi

${BUILD_CMD} ${args} $*

