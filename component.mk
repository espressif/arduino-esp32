ARDUINO_CORE_LIBS := libraries/ArduinoOTA/src  \
    libraries/ESP32/src \
    libraries/ESPmDNS/src \
    libraries/FS/src  \
    libraries/HTTPClient/src \
    libraries/Preferences/src \
    libraries/SD/src \
    libraries/SD_MMC/src \
    libraries/SPI/src \
    libraries/Update/src \
    libraries/Wire/src

ifdef CONFIG_BT_ENABLED
ifdef CONFIG_BLUEDROID_ENABLED
ARDUINO_CORE_LIBS += libraries/SimpleBLE/src
endif
endif

ifdef CONFIG_WIFI_ENABLED
ARDUINO_CORE_LIBS += libraries/WiFi/src \
		     libraries/WiFiClientSecure/src
endif

COMPONENT_ADD_INCLUDEDIRS := cores/esp32 variants/esp32 $(ARDUINO_CORE_LIBS)
COMPONENT_PRIV_INCLUDEDIRS := cores/esp32/libb64
COMPONENT_SRCDIRS := cores/esp32/libb64 cores/esp32 variants/esp32 $(ARDUINO_CORE_LIBS)
CXXFLAGS += -fno-rtti
