#!/bin/bash

pip install -U https://github.com/platformio/platformio/archive/develop.zip && \
python -m platformio platform install https://github.com/platformio/platform-espressif32.git#feature/stage && \
sed -i 's/https:\/\/github\.com\/espressif\/arduino-esp32\.git/*/' ~/.platformio/platforms/espressif32/platform.json && \
ln -s $TRAVIS_BUILD_DIR ~/.platformio/packages/framework-arduinoespressif32
