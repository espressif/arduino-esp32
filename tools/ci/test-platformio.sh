#!/bin/bash

python -m platformio ci  --board esp32dev libraries/WiFi/examples/WiFiClient && \
python -m platformio ci  --board esp32dev libraries/WiFiClientSecure/examples/WiFiClientSecure && \
python -m platformio ci  --board esp32dev libraries/BluetoothSerial/examples/SerialToSerialBT && \
python -m platformio ci  --board esp32dev libraries/BLE/examples/BLE_server && \
python -m platformio ci  --board esp32dev libraries/AzureIoT/examples/GetStarted && \
python -m platformio ci  --board esp32dev libraries/ESP32/examples/Camera/CameraWebServer --project-option="board_build.partitions = huge_app.csv"
if [ $? -ne 0 ]; then exit 1; fi
