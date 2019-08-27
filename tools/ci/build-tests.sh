#!/bin/bash

if [ ! -z "$TRAVIS_TAG" ]; then
	echo "Skipping Test: Tagged build"
	exit 0
fi

CHUNK_INDEX=$1
CHUNKS_CNT=$2
BUILD_PIO=0
if [ "$#" -lt 2 ]; then
	echo "Building all sketches"
	CHUNK_INDEX=0
	CHUNKS_CNT=1
	BUILD_PIO=1
fi
if [ "$CHUNKS_CNT" -le 0 ]; then
	CHUNK_INDEX=0
	CHUNKS_CNT=1
	BUILD_PIO=1
fi
if [ "$CHUNK_INDEX" -gt "$CHUNKS_CNT" ]; then
	CHUNK_INDEX=$CHUNKS_CNT
fi
if [ "$CHUNK_INDEX" -eq "$CHUNKS_CNT" ]; then
	BUILD_PIO=1
fi

# CMake Test
if [ "$CHUNK_INDEX" -eq 0 ]; then
	echo -e "travis_fold:start:check_cmakelists"
	tools/ci/check-cmakelists.sh
	if [ $? -ne 0 ]; then exit 1; fi
	echo -e "travis_fold:end:check_cmakelists"
fi

if [ "$BUILD_PIO" -eq 0 ]; then
	# ArduinoIDE Test
	echo -e "travis_fold:start:prep_arduino_ide"
	tools/ci/prep-arduino-ide.sh
	if [ $? -ne 0 ]; then exit 1; fi
	echo -e "travis_fold:end:prep_arduino_ide"

	echo -e "travis_fold:start:test_arduino_ide"
	tools/ci/test-arduino-ide.sh $CHUNK_INDEX $CHUNKS_CNT
	if [ $? -ne 0 ]; then exit 1; fi
	echo -e "travis_fold:end:test_arduino_ide"

	echo -e "travis_fold:start:size_report"
	cat size.log
	echo -e "travis_fold:end:size_report"
else
	# PlatformIO Test
	echo -e "travis_fold:start:prep_platformio"
	cd tools && python get.py && cd ..
	tools/ci/prep-platformio.sh
	if [ $? -ne 0 ]; then exit 1; fi
	echo -e "travis_fold:end:prep_platformio"

	echo -e "travis_fold:start:test_platformio"
	tools/ci/test-platformio.sh
	if [ $? -ne 0 ]; then exit 1; fi
	echo -e "travis_fold:end:test_platformio"
fi