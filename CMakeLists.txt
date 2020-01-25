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
  cores/esp32/wiring_pulse.c
  cores/esp32/wiring_shift.c
  cores/esp32/WMath.cpp
  cores/esp32/WString.cpp
  )

set(AZURE_SRCS
  libraries/AzureIoT/src/az_iot/azureiotcerts.c
  libraries/AzureIoT/src/az_iot/c-utility/pal/agenttime.c
  libraries/AzureIoT/src/az_iot/c-utility/pal/dns_async.c
  libraries/AzureIoT/src/az_iot/c-utility/pal/freertos/lock.c
  libraries/AzureIoT/src/az_iot/c-utility/pal/freertos/threadapi.c
  libraries/AzureIoT/src/az_iot/c-utility/pal/freertos/tickcounter.c
  libraries/AzureIoT/src/az_iot/c-utility/pal/lwip/sntp_lwip.c
  libraries/AzureIoT/src/az_iot/c-utility/pal/socket_async.c
  libraries/AzureIoT/src/az_iot/c-utility/pal/src/platform_openssl_compact.c
  libraries/AzureIoT/src/az_iot/c-utility/pal/src/tlsio_openssl_compact.c
  libraries/AzureIoT/src/az_iot/c-utility/pal/tlsio_options.c
  libraries/AzureIoT/src/az_iot/c-utility/src/base64.c
  libraries/AzureIoT/src/az_iot/c-utility/src/buffer.c
  libraries/AzureIoT/src/az_iot/c-utility/src/connection_string_parser.c
  libraries/AzureIoT/src/az_iot/c-utility/src/consolelogger.c
  libraries/AzureIoT/src/az_iot/c-utility/src/constbuffer.c
  libraries/AzureIoT/src/az_iot/c-utility/src/constmap.c
  libraries/AzureIoT/src/az_iot/c-utility/src/crt_abstractions.c
  libraries/AzureIoT/src/az_iot/c-utility/src/doublylinkedlist.c
  libraries/AzureIoT/src/az_iot/c-utility/src/gballoc.c
  libraries/AzureIoT/src/az_iot/c-utility/src/gb_stdio.c
  libraries/AzureIoT/src/az_iot/c-utility/src/gb_time.c
  libraries/AzureIoT/src/az_iot/c-utility/src/hmac.c
  libraries/AzureIoT/src/az_iot/c-utility/src/hmacsha256.c
  libraries/AzureIoT/src/az_iot/c-utility/src/httpapiex.c
  libraries/AzureIoT/src/az_iot/c-utility/src/httpapiexsas.c
  libraries/AzureIoT/src/az_iot/c-utility/src/httpheaders.c
  libraries/AzureIoT/src/az_iot/c-utility/src/http_proxy_io.c
  libraries/AzureIoT/src/az_iot/c-utility/src/map.c
  libraries/AzureIoT/src/az_iot/c-utility/src/optionhandler.c
  libraries/AzureIoT/src/az_iot/c-utility/src/sastoken.c
  libraries/AzureIoT/src/az_iot/c-utility/src/sha1.c
  libraries/AzureIoT/src/az_iot/c-utility/src/sha224.c
  libraries/AzureIoT/src/az_iot/c-utility/src/sha384-512.c
  libraries/AzureIoT/src/az_iot/c-utility/src/singlylinkedlist.c
  libraries/AzureIoT/src/az_iot/c-utility/src/strings.c
  libraries/AzureIoT/src/az_iot/c-utility/src/string_tokenizer.c
  libraries/AzureIoT/src/az_iot/c-utility/src/urlencode.c
  libraries/AzureIoT/src/az_iot/c-utility/src/usha.c
  libraries/AzureIoT/src/az_iot/c-utility/src/vector.c
  libraries/AzureIoT/src/az_iot/c-utility/src/xio.c
  libraries/AzureIoT/src/az_iot/c-utility/src/xlogging.c
  libraries/AzureIoT/src/az_iot/iothub_client/src/blob.c
  libraries/AzureIoT/src/az_iot/iothub_client/src/iothub_client_authorization.c
  libraries/AzureIoT/src/az_iot/iothub_client/src/iothub_client.c
  libraries/AzureIoT/src/az_iot/iothub_client/src/iothub_client_ll.c
  libraries/AzureIoT/src/az_iot/iothub_client/src/iothub_client_retry_control.c
  libraries/AzureIoT/src/az_iot/iothub_client/src/iothub_message.c
  libraries/AzureIoT/src/az_iot/iothub_client/src/iothubtransport.c
  libraries/AzureIoT/src/az_iot/iothub_client/src/iothubtransportmqtt.c
  libraries/AzureIoT/src/az_iot/iothub_client/src/iothubtransport_mqtt_common.c
  libraries/AzureIoT/src/az_iot/iothub_client/src/version.c
  libraries/AzureIoT/src/az_iot/umqtt/src/mqtt_client.c
  libraries/AzureIoT/src/az_iot/umqtt/src/mqtt_codec.c
  libraries/AzureIoT/src/az_iot/umqtt/src/mqtt_message.c
  libraries/AzureIoT/src/AzureIotHub.cpp
  libraries/AzureIoT/src/Esp32MQTTClient.cpp
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

set(LIBRARY_SRCS)
set(LIBRARY_INCLUDEDIRS)

# Honor sdkconfig selections
if(CONFIG_ARDUINO_SELECTIVE_ArduinoOTA)
    list(APPEND LIBRARY_SRCS libraries/ArduinoOTA/src/ArduinoOTA.cpp)
    list(APPEND LIBRARY_INCLUDEDIRS libraries/ArduinoOTA/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_AsyncUDP)
    list(APPEND LIBRARY_SRCS libraries/AsyncUDP/src/AsyncUDP.cpp)
    list(APPEND LIBRARY_INCLUDEDIRS libraries/AsyncUDP/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_AzureIoT)
    list(APPEND LIBRARY_SRCS ${AZURE_SRCS})
    list(APPEND LIBRARY_INCLUDEDIRS libraries/AzureIoT/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_BLE)
    list(APPEND LIBRARY_SRCS ${BLE_SRCS})
    list(APPEND LIBRARY_INCLUDEDIRS libraries/BLE/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_BluetoothSerial)
    list(APPEND LIBRARY_SRCS libraries/BluetoothSerial/src/BluetoothSerial.cpp)
    list(APPEND LIBRARY_INCLUDEDIRS libraries/BluetoothSerial/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_DNSServer)
    list(APPEND LIBRARY_SRCS libraries/DNSServer/src/DNSServer.cpp)
    list(APPEND LIBRARY_INCLUDEDIRS libraries/DNSServer/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_EEPROM)
    list(APPEND LIBRARY_SRCS libraries/EEPROM/src/EEPROM.cpp)
    list(APPEND LIBRARY_INCLUDEDIRS libraries/EEPROM/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_ESP32)
    #list(APPEND LIBRARY_SRCS )
    list(APPEND LIBRARY_INCLUDEDIRS libraries/ESP32/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_ESPmDNS)
    list(APPEND LIBRARY_SRCS libraries/ESPmDNS/src/ESPmDNS.cpp)
    list(APPEND LIBRARY_INCLUDEDIRS libraries/ESPmDNS/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_FFat)
    list(APPEND LIBRARY_SRCS libraries/FFat/src/FFat.cpp)
    list(APPEND LIBRARY_INCLUDEDIRS libraries/FFat/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_FS)
    list(APPEND LIBRARY_SRCS
            libraries/FS/src/FS.cpp
            libraries/FS/src/vfs_api.cpp)
    list(APPEND LIBRARY_INCLUDEDIRS libraries/FS/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_HTTPClient)
    list(APPEND LIBRARY_SRCS libraries/HTTPClient/src/HTTPClient.cpp)
    list(APPEND LIBRARY_INCLUDEDIRS libraries/HTTPClient/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_HTTPUpdate)
    list(APPEND LIBRARY_SRCS libraries/HTTPUpdate/src/HTTPUpdate.cpp)
    list(APPEND LIBRARY_INCLUDEDIRS libraries/HTTPUpdate/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_NetBIOS)
    list(APPEND LIBRARY_SRCS libraries/NetBIOS/src/NetBIOS.cpp)
    list(APPEND LIBRARY_INCLUDEDIRS libraries/NetBIOS/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_Preferences)
    list(APPEND LIBRARY_SRCS libraries/Preferences/src/Preferences.cpp)
    list(APPEND LIBRARY_INCLUDEDIRS libraries/Preferences/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_SD_MMC)
    list(APPEND LIBRARY_SRCS libraries/SD_MMC/src/SD_MMC.cpp)
    list(APPEND LIBRARY_INCLUDEDIRS libraries/SD_MMC/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_SD)
    list(APPEND LIBRARY_SRCS
            libraries/SD/src/SD.cpp
            libraries/SD/src/sd_diskio.cpp
            libraries/SD/src/sd_diskio_crc.c)
    list(APPEND LIBRARY_INCLUDEDIRS libraries/SD/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_SimpleBLE)
    list(APPEND LIBRARY_SRCS libraries/SimpleBLE/src/SimpleBLE.cpp)
    list(APPEND LIBRARY_INCLUDEDIRS libraries/SimpleBLE/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_SPIFFS)
    list(APPEND LIBRARY_SRCS libraries/SPIFFS/src/SPIFFS.cpp)
    list(APPEND LIBRARY_INCLUDEDIRS libraries/SPIFFS/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_SPI)
    list(APPEND LIBRARY_SRCS libraries/SPI/src/SPI.cpp)
    list(APPEND LIBRARY_INCLUDEDIRS libraries/SPI/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_Ticker)
    list(APPEND LIBRARY_SRCS libraries/Ticker/src/Ticker.cpp)
    list(APPEND LIBRARY_INCLUDEDIRS libraries/Ticker/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_Update)
    list(APPEND LIBRARY_SRCS libraries/Update/src/Updater.cpp)
    list(APPEND LIBRARY_INCLUDEDIRS libraries/Update/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_WebServer)
    list(APPEND LIBRARY_SRCS
            libraries/WebServer/src/WebServer.cpp
            libraries/WebServer/src/Parsing.cpp
            libraries/WebServer/src/detail/mimetable.cpp)
    list(APPEND LIBRARY_INCLUDEDIRS libraries/WebServer/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_WiFiClientSecure)
    list(APPEND LIBRARY_SRCS
            libraries/WiFiClientSecure/src/ssl_client.cpp
            libraries/WiFiClientSecure/src/WiFiClientSecure.cpp)
    list(APPEND LIBRARY_INCLUDEDIRS libraries/WiFiClientSecure/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_WiFi)
    list(APPEND LIBRARY_SRCS
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
    list(APPEND LIBRARY_INCLUDEDIRS libraries/WiFi/src)
endif()

if(CONFIG_ARDUINO_SELECTIVE_Wire)
    list(APPEND LIBRARY_SRCS libraries/Wire/src/Wire.cpp)
    list(APPEND LIBRARY_INCLUDEDIRS libraries/Wire/src)
endif()


set(COMPONENT_SRCS ${CORE_SRCS} ${LIBRARY_SRCS})

set(COMPONENT_ADD_INCLUDEDIRS
  variants/esp32/
  cores/esp32/
  ${LIBRARY_INCLUDEDIRS}
  )

set(COMPONENT_PRIV_INCLUDEDIRS cores/esp32/libb64)

set(COMPONENT_REQUIRES spi_flash mbedtls mdns ethernet esp_adc_cal)
set(COMPONENT_PRIV_REQUIRES fatfs nvs_flash app_update spiffs bootloader_support openssl bt)

register_component()

# Honor sdkconfig selections
if(CONFIG_ARDUINO_SELECTIVE_AzureIoT)
    set_source_files_properties(libraries/AzureIoT/src/az_iot/iothub_client/src/iothubtransport_mqtt_common.c
        PROPERTIES COMPILE_FLAGS
        -Wno-maybe-uninitialized
    )
endif()
