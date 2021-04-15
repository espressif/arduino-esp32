set(CORE_SRCS
  cores/esp32/base64.cpp
  cores/esp32/cbuf.cpp
  cores/esp32/esp32-hal-adc.c
  cores/esp32/esp32-hal-bt.c
  cores/esp32/esp32-hal-cpu.c
  cores/esp32/esp32-hal-dac.c
  cores/esp32/esp32-hal-gpio.c
  cores/esp32/esp32-hal-i2c.c
  cores/esp32/esp32-hal-log.c
  cores/esp32/esp32-hal-ledc.c
  cores/esp32/esp32-hal-matrix.c
  cores/esp32/esp32-hal-misc.c
  cores/esp32/esp32-hal-psram.c
  cores/esp32/esp32-hal-sigmadelta.c
  cores/esp32/esp32-hal-spi.c
  cores/esp32/esp32-hal-time.c
  cores/esp32/esp32-hal-timer.c
  cores/esp32/esp32-hal-tinyusb.c
  cores/esp32/esp32-hal-touch.c
  cores/esp32/esp32-hal-uart.c
  cores/esp32/esp32-hal-rmt.c
  cores/esp32/Esp.cpp
  cores/esp32/FunctionalInterrupt.cpp
  cores/esp32/HardwareSerial.cpp
  cores/esp32/IPAddress.cpp
  cores/esp32/IPv6Address.cpp
  cores/esp32/libb64/cdecode.c
  cores/esp32/libb64/cencode.c
  cores/esp32/main.cpp
  cores/esp32/MD5Builder.cpp
  cores/esp32/Print.cpp
  cores/esp32/stdlib_noniso.c
  cores/esp32/Stream.cpp
  cores/esp32/StreamString.cpp
  cores/esp32/USB.cpp
  cores/esp32/USBCDC.cpp
  cores/esp32/wiring_pulse.c
  cores/esp32/wiring_shift.c
  cores/esp32/WMath.cpp
  cores/esp32/WString.cpp
  )

set(LIBRARY_SRCS
  libraries/ArduinoOTA/src/ArduinoOTA.cpp
  libraries/AsyncUDP/src/AsyncUDP.cpp
  libraries/BluetoothSerial/src/BluetoothSerial.cpp
  libraries/DNSServer/src/DNSServer.cpp
  libraries/EEPROM/src/EEPROM.cpp
  libraries/ESPmDNS/src/ESPmDNS.cpp
  libraries/FFat/src/FFat.cpp
  libraries/FS/src/FS.cpp
  libraries/FS/src/vfs_api.cpp
  libraries/HTTPClient/src/HTTPClient.cpp
  libraries/HTTPUpdate/src/HTTPUpdate.cpp
  libraries/LITTLEFS/src/LITTLEFS.cpp
  libraries/NetBIOS/src/NetBIOS.cpp
  libraries/Preferences/src/Preferences.cpp
  libraries/RainMaker/src/RMaker.cpp
  libraries/RainMaker/src/RMakerNode.cpp
  libraries/RainMaker/src/RMakerParam.cpp
  libraries/RainMaker/src/RMakerDevice.cpp
  libraries/RainMaker/src/RMakerType.cpp
  libraries/SD_MMC/src/SD_MMC.cpp
  libraries/SD/src/SD.cpp
  libraries/SD/src/sd_diskio.cpp
  libraries/SD/src/sd_diskio_crc.c
  libraries/SimpleBLE/src/SimpleBLE.cpp
  libraries/SPIFFS/src/SPIFFS.cpp
  libraries/SPI/src/SPI.cpp
  libraries/Ticker/src/Ticker.cpp
  libraries/Update/src/Updater.cpp
  libraries/Update/src/HttpsOTAUpdate.cpp
  libraries/WebServer/src/WebServer.cpp
  libraries/WebServer/src/Parsing.cpp
  libraries/WebServer/src/detail/mimetable.cpp
  libraries/WiFiClientSecure/src/ssl_client.cpp
  libraries/WiFiClientSecure/src/WiFiClientSecure.cpp
  libraries/WiFi/src/ETH.cpp
  libraries/WiFi/src/WiFiAP.cpp
  libraries/WiFi/src/WiFiClient.cpp
  libraries/WiFi/src/WiFi.cpp
  libraries/WiFi/src/WiFiGeneric.cpp
  libraries/WiFi/src/WiFiMulti.cpp
  libraries/WiFi/src/WiFiScan.cpp
  libraries/WiFi/src/WiFiServer.cpp
  libraries/WiFi/src/WiFiSTA.cpp
  libraries/WiFi/src/WiFiUdp.cpp
  libraries/WiFiProv/src/WiFiProv.cpp
  libraries/Wire/src/Wire.cpp
  )

set(BLE_SRCS
  libraries/BLE/src/BLE2902.cpp
  libraries/BLE/src/BLE2904.cpp
  libraries/BLE/src/BLEAddress.cpp
  libraries/BLE/src/BLEAdvertisedDevice.cpp
  libraries/BLE/src/BLEAdvertising.cpp
  libraries/BLE/src/BLEBeacon.cpp
  libraries/BLE/src/BLECharacteristic.cpp
  libraries/BLE/src/BLECharacteristicMap.cpp
  libraries/BLE/src/BLEClient.cpp
  libraries/BLE/src/BLEDescriptor.cpp
  libraries/BLE/src/BLEDescriptorMap.cpp
  libraries/BLE/src/BLEDevice.cpp
  libraries/BLE/src/BLEEddystoneTLM.cpp
  libraries/BLE/src/BLEEddystoneURL.cpp
  libraries/BLE/src/BLEExceptions.cpp
  libraries/BLE/src/BLEHIDDevice.cpp
  libraries/BLE/src/BLERemoteCharacteristic.cpp
  libraries/BLE/src/BLERemoteDescriptor.cpp
  libraries/BLE/src/BLERemoteService.cpp
  libraries/BLE/src/BLEScan.cpp
  libraries/BLE/src/BLESecurity.cpp
  libraries/BLE/src/BLEServer.cpp
  libraries/BLE/src/BLEService.cpp
  libraries/BLE/src/BLEServiceMap.cpp
  libraries/BLE/src/BLEUtils.cpp
  libraries/BLE/src/BLEUUID.cpp
  libraries/BLE/src/BLEValue.cpp
  libraries/BLE/src/FreeRTOS.cpp
  libraries/BLE/src/GeneralUtils.cpp
  )


set(includedirs
  variants/${IDF_TARGET}/
  cores/esp32/
  libraries/ArduinoOTA/src
  libraries/AsyncUDP/src
  libraries/BLE/src
  libraries/BluetoothSerial/src
  libraries/DNSServer/src
  libraries/EEPROM/src
  libraries/ESP32/src
  libraries/ESPmDNS/src
  libraries/FFat/src
  libraries/FS/src
  libraries/HTTPClient/src
  libraries/HTTPUpdate/src
  libraries/LITTLEFS/src
  libraries/NetBIOS/src
  libraries/Preferences/src
  libraries/RainMaker/src
  libraries/SD_MMC/src
  libraries/SD/src
  libraries/SimpleBLE/src
  libraries/SPIFFS/src
  libraries/SPI/src
  libraries/Ticker/src
  libraries/Update/src
  libraries/WebServer/src
  libraries/WiFiClientSecure/src
  libraries/WiFi/src
  libraries/WiFiProv/src
  libraries/Wire/src
  )

set(srcs ${CORE_SRCS} ${LIBRARY_SRCS} ${BLE_SRCS})
set(priv_includes cores/esp32/libb64)
set(requires spi_flash mbedtls mdns esp_adc_cal)
set(priv_requires fatfs nvs_flash app_update spiffs bootloader_support openssl bt arduino_tinyusb main)

if(NOT CONFIG_ARDUINO_SELECTIVE_COMPILATION OR CONFIG_ARDUINO_SELECTIVE_ArduinoOTA)
  list(APPEND priv_requires esp_https_ota)
endif()
if(NOT CONFIG_ARDUINO_SELECTIVE_COMPILATION OR CONFIG_ARDUINO_SELECTIVE_LITTLEFS)
  list(APPEND priv_requires esp_littlefs)
endif()

idf_component_register(INCLUDE_DIRS ${includedirs} PRIV_INCLUDE_DIRS ${priv_includes} SRCS ${srcs} REQUIRES ${requires} PRIV_REQUIRES ${priv_requires})

if(IDF_TARGET STREQUAL "esp32")
target_compile_options(${COMPONENT_TARGET} PUBLIC -DARDUINO=10812 -DARDUINO_ESP32_DEV -DARDUINO_ARCH_ESP32 -DARDUINO_BOARD="ESP32_DEV" -DARDUINO_VARIANT="esp32" -DESP32)
endif()
if(IDF_TARGET STREQUAL "esp32s2")
target_compile_options(${COMPONENT_TARGET} PUBLIC -DARDUINO=10812 -DARDUINO_ESP32S2_DEV -DARDUINO_ARCH_ESP32 -DARDUINO_BOARD="ESP32S2_DEV" -DARDUINO_VARIANT="esp32s2" -DESP32)
endif()
