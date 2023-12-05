# Check ESP-IDF version and error out if it is not in the supported range.
#
# Note for arduino-esp32 developers: to bypass the version check locally,
# set ARDUINO_SKIP_IDF_VERSION_CHECK environment variable to 1. For example:
#   export ARDUINO_SKIP_IDF_VERSION_CHECK=1
#   idf.py build

set(min_supported_idf_version "5.1.0")
set(max_supported_idf_version "5.1.99")
set(idf_version "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}.${IDF_VERSION_PATCH}")

if ("${idf_version}" AND NOT "$ENV{ARDUINO_SKIP_IDF_VERSION_CHECK}")
  if (idf_version VERSION_LESS min_supported_idf_version)
    message(FATAL_ERROR "Arduino-esp32 can be used with ESP-IDF versions "
                        "between ${min_supported_idf_version} and ${max_supported_idf_version}, "
                        "but an older version is detected: ${idf_version}.")
  endif()
  if (idf_version VERSION_GREATER max_supported_idf_version)
    message(FATAL_ERROR "Arduino-esp32 can be used with ESP-IDF versions "
                        "between ${min_supported_idf_version} and ${max_supported_idf_version}, "
                        "but a newer version is detected: ${idf_version}.")
  endif()
endif()

set(CORE_SRCS
  cores/esp32/base64.cpp
  cores/esp32/cbuf.cpp
  cores/esp32/chip-debug-report.cpp
  cores/esp32/esp32-hal-adc.c
  cores/esp32/esp32-hal-bt.c
  cores/esp32/esp32-hal-cpu.c
  cores/esp32/esp32-hal-dac.c
  cores/esp32/esp32-hal-gpio.c
  cores/esp32/esp32-hal-i2c.c
  cores/esp32/esp32-hal-i2c-slave.c
  cores/esp32/esp32-hal-ledc.c
  cores/esp32/esp32-hal-matrix.c
  cores/esp32/esp32-hal-misc.c
  cores/esp32/esp32-hal-periman.c
  cores/esp32/esp32-hal-psram.c
  cores/esp32/esp32-hal-rgb-led.c
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
  cores/esp32/Tone.cpp
  cores/esp32/HWCDC.cpp
  cores/esp32/USB.cpp
  cores/esp32/USBCDC.cpp
  cores/esp32/USBMSC.cpp
  cores/esp32/FirmwareMSC.cpp
  cores/esp32/firmware_msc_fat.c
  cores/esp32/wiring_pulse.c
  cores/esp32/wiring_shift.c
  cores/esp32/WMath.cpp
  cores/esp32/WString.cpp
  )

set(LIBRARY_SRCS
  libraries/ArduinoOTA/src/ArduinoOTA.cpp
  libraries/AsyncUDP/src/AsyncUDP.cpp
  libraries/BluetoothSerial/src/BluetoothSerial.cpp
  libraries/BluetoothSerial/src/BTAddress.cpp
  libraries/BluetoothSerial/src/BTAdvertisedDeviceSet.cpp
  libraries/BluetoothSerial/src/BTScanResultsSet.cpp
  libraries/DNSServer/src/DNSServer.cpp
  libraries/EEPROM/src/EEPROM.cpp
  libraries/ESP_I2S/src/ESP_I2S.cpp
  libraries/ESP_SR/src/ESP_SR.cpp
  libraries/ESP_SR/src/esp32-hal-sr.c
  libraries/ESPmDNS/src/ESPmDNS.cpp
  libraries/Ethernet/src/ETH.cpp
  libraries/FFat/src/FFat.cpp
  libraries/FS/src/FS.cpp
  libraries/FS/src/vfs_api.cpp
  libraries/HTTPClient/src/HTTPClient.cpp
  libraries/HTTPUpdate/src/HTTPUpdate.cpp
  libraries/LittleFS/src/LittleFS.cpp
  libraries/Insights/src/Insights.cpp
  libraries/NetBIOS/src/NetBIOS.cpp
  libraries/Preferences/src/Preferences.cpp
  libraries/RainMaker/src/RMaker.cpp
  libraries/RainMaker/src/RMakerNode.cpp
  libraries/RainMaker/src/RMakerParam.cpp
  libraries/RainMaker/src/RMakerDevice.cpp
  libraries/RainMaker/src/RMakerType.cpp
  libraries/RainMaker/src/RMakerQR.cpp
  libraries/RainMaker/src/RMakerUtils.cpp
  libraries/RainMaker/src/AppInsights.cpp
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
  libraries/USB/src/USBHID.cpp
  libraries/USB/src/USBMIDI.cpp
  libraries/USB/src/USBHIDMouse.cpp
  libraries/USB/src/USBHIDKeyboard.cpp
  libraries/USB/src/USBHIDGamepad.cpp
  libraries/USB/src/USBHIDConsumerControl.cpp
  libraries/USB/src/USBHIDSystemControl.cpp
  libraries/USB/src/USBHIDVendor.cpp
  libraries/USB/src/USBVendor.cpp
  libraries/WebServer/src/WebServer.cpp
  libraries/WebServer/src/Parsing.cpp
  libraries/WebServer/src/detail/mimetable.cpp
  libraries/WiFiClientSecure/src/ssl_client.cpp
  libraries/WiFiClientSecure/src/WiFiClientSecure.cpp
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
  variants/${CONFIG_ARDUINO_VARIANT}/
  cores/esp32/
  libraries/ArduinoOTA/src
  libraries/AsyncUDP/src
  libraries/BLE/src
  libraries/BluetoothSerial/src
  libraries/DNSServer/src
  libraries/EEPROM/src
  libraries/ESP_I2S/src
  libraries/ESP_SR/src
  libraries/ESP32/src
  libraries/ESPmDNS/src
  libraries/Ethernet/src
  libraries/FFat/src
  libraries/FS/src
  libraries/HTTPClient/src
  libraries/HTTPUpdate/src
  libraries/LittleFS/src
  libraries/Insights/src
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
  libraries/USB/src
  libraries/WebServer/src
  libraries/WiFiClientSecure/src
  libraries/WiFi/src
  libraries/WiFiProv/src
  libraries/Wire/src
  )

set(srcs ${CORE_SRCS} ${LIBRARY_SRCS} ${BLE_SRCS})
set(priv_includes cores/esp32/libb64)
set(requires spi_flash esp_partition mbedtls wifi_provisioning wpa_supplicant esp_adc esp_eth http_parser)
set(priv_requires fatfs nvs_flash app_update spiffs bootloader_support bt esp_hid)

idf_component_register(INCLUDE_DIRS ${includedirs} PRIV_INCLUDE_DIRS ${priv_includes} SRCS ${srcs} REQUIRES ${requires} PRIV_REQUIRES ${priv_requires})

if(NOT CONFIG_FREERTOS_HZ EQUAL 1000 AND NOT "$ENV{ARDUINO_SKIP_TICK_CHECK}")
    # See delay() in cores/esp32/esp32-hal-misc.c.
    message(FATAL_ERROR "esp32-arduino requires CONFIG_FREERTOS_HZ=1000 "
                        "(currently ${CONFIG_FREERTOS_HZ})")
endif()

string(TOUPPER ${CONFIG_ARDUINO_VARIANT} idf_target_caps)
string(REPLACE "-" "_" idf_target_for_macro "${idf_target_caps}")
target_compile_options(${COMPONENT_TARGET} PUBLIC
    -DARDUINO=10812
    -DARDUINO_${idf_target_for_macro}_DEV
    -DARDUINO_ARCH_ESP32
    -DARDUINO_BOARD="${idf_target_caps}_DEV"
    -DARDUINO_VARIANT="${CONFIG_ARDUINO_VARIANT}"
    -DESP32)

if(CONFIG_AUTOSTART_ARDUINO)
    # in autostart mode, arduino-esp32 contains app_main() function and needs to
    # reference setup() and loop() in the main component. If we add main
    # component to priv_requires then we create a large circular dependency
    # (arduino-esp32 -> main -> arduino-esp32) and can get linker errors, so
    # instead we add setup() and loop() to the undefined symbols list so the
    # linker will always include them.
    #
    # (As they are C++ symbol, we need to add the C++ mangled names.)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u _Z5setupv -u _Z4loopv")
endif()

# This function adds a dependency on the given component if the component is included into the build.
function(maybe_add_component component_name)
    idf_build_get_property(components BUILD_COMPONENTS)
    if (${component_name} IN_LIST components)
        idf_component_get_property(lib_name ${component_name} COMPONENT_LIB)
        target_link_libraries(${COMPONENT_LIB} PUBLIC ${lib_name})
    endif()
endfunction()

if(IDF_TARGET MATCHES "esp32s2|esp32s3" AND CONFIG_TINYUSB_ENABLED)
    maybe_add_component(arduino_tinyusb)
endif()
if(NOT CONFIG_ARDUINO_SELECTIVE_COMPILATION OR CONFIG_ARDUINO_SELECTIVE_ArduinoOTA)
    maybe_add_component(esp_https_ota)
endif()
