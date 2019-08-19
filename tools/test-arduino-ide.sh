#!/bin/bash

export PATH="$HOME/arduino_ide:$TRAVIS_BUILD_DIR/tools/xtensa-esp32-elf/bin:$PATH"
source tools/common.sh
#build_sketches $HOME/arduino_ide $TRAVIS_BUILD_DIR/libraries "-l $HOME/Arduino/libraries"
if [ $? -ne 0 ]; then exit 1; fi
