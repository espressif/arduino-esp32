#!/bin/bash

if [ ! -z "$TRAVIS_TAG" ]; then
	echo "Skipping Test: Tagged build"
	exit 0
fi

if [ ! -z "$GITHUB_WORKSPACE" ]; then
	export TRAVIS_BUILD_DIR="$GITHUB_WORKSPACE"
	export TRAVIS_REPO_SLUG="$GITHUB_REPOSITORY"
elif [ ! -z "$TRAVIS_BUILD_DIR" ]; then
	export GITHUB_WORKSPACE="$TRAVIS_BUILD_DIR"
	export GITHUB_REPOSITORY="$TRAVIS_REPO_SLUG"
else
	export GITHUB_WORKSPACE="$PWD"
	export GITHUB_REPOSITORY="espressif/arduino-esp32"
fi

CHUNK_INDEX=$1
CHUNKS_CNT=$2
BUILD_PIO=0
if [ "$#" -lt 2 ] ||  [ "$CHUNKS_CNT" -le 0 ]; then
	echo "Building all sketches"
	CHUNK_INDEX=0
	CHUNKS_CNT=1
fi
if [ "$CHUNK_INDEX" -gt "$CHUNKS_CNT" ]; then
	CHUNK_INDEX=$CHUNKS_CNT
fi
if [ "$CHUNK_INDEX" -eq "$CHUNKS_CNT" ]; then
	BUILD_PIO=1
fi

# CMake Test
if [ "$CHUNK_INDEX" -eq 0 ]; then
	bash ./tools/ci/check-cmakelists.sh
	if [ $? -ne 0 ]; then exit 1; fi
fi

if [ "$BUILD_PIO" -eq 0 ]; then
	# ArduinoIDE Test
	source ./tools/ci/install-arduino-ide.sh
	source ./tools/ci/install-arduino-core-esp32.sh
	build_sketches "$GITHUB_WORKSPACE/libraries" "espressif:esp32:esp32:PSRAM=enabled,PartitionScheme=huge_app" "$CHUNK_INDEX" "$CHUNKS_CNT"
else
	# PlatformIO Test
	source ./tools/ci/install-platformio-esp32.sh
	python -m platformio ci  --board esp32dev libraries/WiFi/examples/WiFiClient && \
	python -m platformio ci  --board esp32dev libraries/WiFiClientSecure/examples/WiFiClientSecure && \
	python -m platformio ci  --board esp32dev libraries/BluetoothSerial/examples/SerialToSerialBT && \
	python -m platformio ci  --board esp32dev libraries/BLE/examples/BLE_server && \
	python -m platformio ci  --board esp32dev libraries/AzureIoT/examples/GetStarted && \
	python -m platformio ci  --board esp32dev libraries/ESP32/examples/Camera/CameraWebServer --project-option="board_build.partitions = huge_app.csv"
	#build_pio_sketches libraries esp32dev $CHUNK_INDEX $CHUNKS_CNT
	if [ $? -ne 0 ]; then exit 1; fi
fi