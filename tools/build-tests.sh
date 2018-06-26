#!/bin/bash

#- set -e

if [ ! -z "$TRAVIS_TAG" ]; then
	echo "No sketch builds & tests required for tagged TravisCI builds, exiting"
	exit 0
fi

echo -e "travis_fold:start:sketch_test_env_prepare"
pip install pyserial
wget -O arduino.tar.xz https://www.arduino.cc/download.php?f=/arduino-nightly-linux64.tar.xz
tar xf arduino.tar.xz
mv arduino-nightly $HOME/arduino_ide
mkdir -p $HOME/Arduino/libraries
cd $HOME/arduino_ide/hardware
mkdir espressif
cd espressif
ln -s $TRAVIS_BUILD_DIR esp32
cd esp32
git submodule update --init --recursive
cd tools
python get.py
cd $TRAVIS_BUILD_DIR
export PATH="$HOME/arduino_ide:$TRAVIS_BUILD_DIR/tools/xtensa-esp32-elf/bin:$PATH"
source tools/common.sh
echo -e "travis_fold:end:sketch_test_env_prepare"

echo -e "travis_fold:start:sketch_test"
build_sketches $HOME/arduino_ide $TRAVIS_BUILD_DIR/libraries "-l $HOME/Arduino/libraries"
if [ $? -ne 0 ]; then exit 1; fi
echo -e "travis_fold:end:sketch_test"

echo -e "travis_fold:start:size_report"
cat size.log
echo -e "travis_fold:end:size_report"

echo -e "travis_fold:start:platformio_test_env_prepare"
pip install -U https://github.com/platformio/platformio/archive/develop.zip && \
platformio platform install https://github.com/platformio/platform-espressif32.git#feature/stage && \
sed -i 's/https:\/\/github\.com\/espressif\/arduino-esp32\.git/*/' ~/.platformio/platforms/espressif32/platform.json && \
ln -s $TRAVIS_BUILD_DIR ~/.platformio/packages/framework-arduinoespressif32
if [ $? -ne 0 ]; then exit 1; fi
echo -e "travis_fold:end:platformio_test_env_prepare"

echo -e "travis_fold:start:platformio_test"
platformio ci  --board esp32dev libraries/WiFi/examples/WiFiClient && \
platformio ci  --board esp32dev libraries/WiFiClientSecure/examples/WiFiClientSecure && \
platformio ci  --board esp32dev libraries/BluetoothSerial/examples/SerialToSerialBT && \
platformio ci  --board esp32dev libraries/BLE/examples/BLE_server && \
platformio ci  --board esp32dev libraries/AzureIoT/examples/GetStarted
if [ $? -ne 0 ]; then exit 1; fi
echo -e "travis_fold:end:platformio_test"
