#!/bin/bash

export ARDUINO_ESP32_PATH="$ARDUINO_USR_PATH/hardware/espressif/esp32"
if [ ! -d "$ARDUINO_ESP32_PATH" ]; then
	echo "Installing ESP32 Arduino Core ..."
	script_init_path="$PWD"
	mkdir -p "$ARDUINO_USR_PATH/hardware/espressif"
	cd "$ARDUINO_USR_PATH/hardware/espressif"

	echo "Installing Python Serial ..."
	pip install pyserial > /dev/null

	if [ "$OS_IS_WINDOWS" == "1" ]; then
		echo "Installing Python Requests ..."
		pip install requests > /dev/null
	fi

# I don't get the code below, you'd want to test the source code in the current repository (forked or not), not in the espressif repo right?
#if [ "$GITHUB_REPOSITORY" == "espressif/arduino-esp32" ];  then
	echo "Linking Core..."
	ln -s $GITHUB_WORKSPACE "$ARDUINO_ESP32_PATH"
#else
#	echo "Cloning Core Repository ..."
#	git clone --recursive https://github.com/espressif/arduino-esp32.git "$PLATFORMIO_ESP32_PATH" > /dev/null 2>&1
#fi

	echo "Updating Submodules ..."
	cd esp32
	git submodule update --init --recursive > /dev/null 2>&1

	echo "Installing Platform Tools ..."
	cd tools && python get.py
	cd $script_init_path

	echo "ESP32 Arduino has been installed in '$ARDUINO_ESP32_PATH'"
	echo ""
fi
