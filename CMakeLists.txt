set(CORE_SRCS
  cores/esp32/base64.cpp
  cores/esp32/cbuf.cpp
  cores/esp32/esp32-hal-adc.c
  cores/esp32/esp32-hal-bt.c
  cores/esp32/esp32-hal-cpu.c
  cores/esp32/esp32-hal-dac.c
  cores/esp32/esp32-hal-gpio.c
  cores/esp32/esp32-hal-i2c.c
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

set(ARDUINO_ALL_LIBRARIES
  ArduinoOTA
  AsyncUDP
  BLE
  BluetoothSerial
  DNSServer
  EEPROM
  ESPmDNS
  FFat
  FS
  HTTPClient
  HTTPUpdate
  LITTLEFS
  NetBIOS
  Preferences
  RainMaker
  SD_MMC
  SD
  SPIFFS
  SPI
  Ticker
  Update
  WebServer
  WiFiClientSecure
  WiFi
  WiFiProv
  Wire
  )

set(ARDUINO_LIBRARY_ArduinoOTA_SRCS libraries/ArduinoOTA/src/ArduinoOTA.cpp)
set(ARDUINO_LIBRARY_ArduinoOTA_REQUIRES esp_https_ota)

set(ARDUINO_LIBRARY_AsyncUDP_SRCS libraries/AsyncUDP/src/AsyncUDP.cpp)

set(ARDUINO_LIBRARY_BluetoothSerial_SRCS
  libraries/BluetoothSerial/src/BluetoothSerial.cpp
  libraries/BluetoothSerial/src/BTAddress.cpp
  libraries/BluetoothSerial/src/BTAdvertisedDeviceSet.cpp
  libraries/BluetoothSerial/src/BTScanResultsSet.cpp)

set(ARDUINO_LIBRARY_DNSServer_SRCS libraries/DNSServer/src/DNSServer.cpp)

set(ARDUINO_LIBRARY_EEPROM_SRCS libraries/EEPROM/src/EEPROM.cpp)

set(ARDUINO_LIBRARY_ESPmDNS_SRCS libraries/ESPmDNS/src/ESPmDNS.cpp)

set(ARDUINO_LIBRARY_FFat_SRCS libraries/FFat/src/FFat.cpp)

set(ARDUINO_LIBRARY_FS_SRCS
  libraries/FS/src/FS.cpp
  libraries/FS/src/vfs_api.cpp)

set(ARDUINO_LIBRARY_HTTPClient_SRCS libraries/HTTPClient/src/HTTPClient.cpp)

set(ARDUINO_LIBRARY_HTTPUpdate_SRCS libraries/HTTPUpdate/src/HTTPUpdate.cpp)

set(ARDUINO_LIBRARY_LITTLEFS_SRCS libraries/LITTLEFS/src/LITTLEFS.cpp)
set(ARDUINO_LIBRARY_LITTLEFS_REQUIRES esp_littlefs)

set(ARDUINO_LIBRARY_NetBIOS_SRCS libraries/NetBIOS/src/NetBIOS.cpp)

set(ARDUINO_LIBRARY_Preferences_SRCS libraries/Preferences/src/Preferences.cpp)

set(ARDUINO_LIBRARY_RainMaker_SRCS
  libraries/RainMaker/src/RMaker.cpp
  libraries/RainMaker/src/RMakerNode.cpp
  libraries/RainMaker/src/RMakerParam.cpp
  libraries/RainMaker/src/RMakerDevice.cpp
  libraries/RainMaker/src/RMakerType.cpp)

set(ARDUINO_LIBRARY_SD_MMC_SRCS libraries/SD_MMC/src/SD_MMC.cpp)

set(ARDUINO_LIBRARY_SD_SRCS
  libraries/SD/src/SD.cpp
  libraries/SD/src/sd_diskio.cpp
  libraries/SD/src/sd_diskio_crc.c)

set(ARDUINO_LIBRARY_SimpleBLE_SRCS libraries/SimpleBLE/src/SimpleBLE.cpp)

set(ARDUINO_LIBRARY_SPIFFS_SRCS libraries/SPIFFS/src/SPIFFS.cpp)

set(ARDUINO_LIBRARY_SPI_SRCS libraries/SPI/src/SPI.cpp)

set(ARDUINO_LIBRARY_Ticker_SRCS libraries/Ticker/src/Ticker.cpp)

set(ARDUINO_LIBRARY_Update_SRCS
  libraries/Update/src/Updater.cpp
  libraries/Update/src/HttpsOTAUpdate.cpp)

set(ARDUINO_LIBRARY_WebServer_SRCS
  libraries/WebServer/src/WebServer.cpp
  libraries/WebServer/src/Parsing.cpp
  libraries/WebServer/src/detail/mimetable.cpp)

set(ARDUINO_LIBRARY_WiFiClientSecure_SRCS
  libraries/WiFiClientSecure/src/ssl_client.cpp
  libraries/WiFiClientSecure/src/WiFiClientSecure.cpp)

set(ARDUINO_LIBRARY_WiFi_SRCS
  libraries/WiFi/src/ETH.cpp
  libraries/WiFi/src/WiFiAP.cpp
  libraries/WiFi/src/WiFiClient.cpp
  libraries/WiFi/src/WiFi.cpp
  libraries/WiFi/src/WiFiGeneric.cpp
  libraries/WiFi/src/WiFiMulti.cpp
  libraries/WiFi/src/WiFiScan.cpp
  libraries/WiFi/src/WiFiServer.cpp
  libraries/WiFi/src/WiFiSTA.cpp
  libraries/WiFi/src/WiFiUdp.cpp)

set(ARDUINO_LIBRARY_WiFiProv_SRCS libraries/WiFiProv/src/WiFiProv.cpp)

set(ARDUINO_LIBRARY_Wire_SRCS libraries/Wire/src/Wire.cpp)

set(ARDUINO_LIBRARY_BLE_SRCS
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


set(includedirs variants/${IDF_TARGET}/ cores/esp32/)
set(srcs ${CORE_SRCS})
set(priv_includes cores/esp32/libb64)
set(requires spi_flash mbedtls mdns esp_adc_cal)
set(priv_requires fatfs nvs_flash app_update spiffs bootloader_support openssl bt main)

foreach(libname IN LISTS ARDUINO_ALL_LIBRARIES)
  if(NOT CONFIG_ARDUINO_SELECTIVE_COMPILATION OR CONFIG_ARDUINO_SELECTIVE_${libname})
    if(ARDUINO_LIBRARY_${libname}_SRCS)
      list(APPEND srcs ${ARDUINO_LIBRARY_${libname}_SRCS})
    endif()
    if(ARDUINO_LIBRARY_${libname}_REQUIRES)
      list(APPEND priv_requires ${ARDUINO_LIBRARY_${libname}_REQUIRES})
    endif()
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libraries/${libname}/src)
      list(APPEND includedirs libraries/${libname}/src)
    endif()
  endif()
endforeach()

idf_component_register(INCLUDE_DIRS ${includedirs} PRIV_INCLUDE_DIRS ${priv_includes} SRCS ${srcs} REQUIRES ${requires} PRIV_REQUIRES ${priv_requires})

if(IDF_TARGET STREQUAL "esp32")
target_compile_options(${COMPONENT_TARGET} PUBLIC -DARDUINO=10812 -DARDUINO_ESP32_DEV -DARDUINO_ARCH_ESP32 -DARDUINO_BOARD="ESP32_DEV" -DARDUINO_VARIANT="esp32" -DESP32)
endif()
if(IDF_TARGET STREQUAL "esp32s2")
target_compile_options(${COMPONENT_TARGET} PUBLIC -DARDUINO=10812 -DARDUINO_ESP32S2_DEV -DARDUINO_ARCH_ESP32 -DARDUINO_BOARD="ESP32S2_DEV" -DARDUINO_VARIANT="esp32s2" -DESP32)
endif()
