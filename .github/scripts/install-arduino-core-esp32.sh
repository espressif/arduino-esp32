#!/bin/bash

export ARDUINO_ESP32_PATH="$ARDUINO_USR_PATH/hardware/espressif/esp32"
if [ ! -d "$ARDUINO_ESP32_PATH" ]; then
	echo "Installing ESP32 Arduino Core in '$ARDUINO_ESP32_PATH'..."
	script_init_path="$PWD"
	mkdir -p "$ARDUINO_USR_PATH/hardware/espressif" && \
	cd "$ARDUINO_USR_PATH/hardware/espressif"
	if [ $? -ne 0 ]; then exit 1; fi

	if [ "$GITHUB_REPOSITORY" == "espressif/arduino-esp32" ];  then
		echo "Linking Core..." && \
		ln -s $GITHUB_WORKSPACE esp32
	else
		echo "Cloning Core Repository..." && \
		git clone https://github.com/espressif/arduino-esp32.git esp32 > /dev/null 2>&1
		if [ $? -ne 0 ]; then echo "ERROR: GIT clone failed"; exit 1; fi
	fi

	cd esp32 && \
	echo "Updating submodules..." && \
	git submodule update --init --recursive > /dev/null 2>&1
	if [ $? -ne 0 ]; then echo "ERROR: Submodule update failed"; exit 1; fi

	echo "Installing Python Serial..." && \
	pip install pyserial > /dev/null
	if [ $? -ne 0 ]; then echo "ERROR: Install failed"; exit 1; fi

	if [ "$OS_IS_WINDOWS" == "1" ]; then
		echo "Installing Python Requests..."
		pip install requests > /dev/null
		if [ $? -ne 0 ]; then echo "ERROR: Install failed"; exit 1; fi
	fi

	echo "Downloading the tools and the toolchain..."
	cd tools && python get.py > /dev/null
	if [ $? -ne 0 ]; then echo "ERROR: Download failed"; exit 1; fi
	cd $script_init_path

	echo "ESP32 Arduino has been installed in '$ARDUINO_ESP32_PATH'"
	echo ""
fi
