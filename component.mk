ARDUINO_CORE_LIBS := $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/*/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/*/*/*/*/*/*/))))

SPECIFIC_LIBS :=

ifneq "y" "$(CONFIG_ARDUINO_SELECTIVE_ArduinoOTA)"
MODULE := ArduinoOTA
SPECIFIC_LIBS += $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
endif

ifneq "y" "$(CONFIG_ARDUINO_SELECTIVE_AsyncUDP)"
MODULE := AsyncUDP
SPECIFIC_LIBS += $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
endif

ifneq "y" "$(CONFIG_ARDUINO_SELECTIVE_AzureIoT)"
MODULE := AzureIoT
SPECIFIC_LIBS += $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
endif

ifneq "y" "$(CONFIG_ARDUINO_SELECTIVE_BLE)"
MODULE := BLE
SPECIFIC_LIBS += $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
endif

ifneq "y" "$(CONFIG_ARDUINO_SELECTIVE_BluetoothSerial)"
MODULE := BluetoothSerial
SPECIFIC_LIBS += $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
endif

ifneq "y" "$(CONFIG_ARDUINO_SELECTIVE_DNSServer)"
MODULE := DNSServer
SPECIFIC_LIBS += $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
endif

ifneq "y" "$(CONFIG_ARDUINO_SELECTIVE_EEPROM)"
MODULE := EEPROM
SPECIFIC_LIBS += $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
endif

ifneq "y" "$(CONFIG_ARDUINO_SELECTIVE_ESP32)"
MODULE := ESP32
SPECIFIC_LIBS += $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
endif

ifneq "y" "$(CONFIG_ARDUINO_SELECTIVE_ESPmDNS)"
MODULE := ESPmDNS
SPECIFIC_LIBS += $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
endif

ifneq "y" "$(CONFIG_ARDUINO_SELECTIVE_FS)"
MODULE := FS
SPECIFIC_LIBS += $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
endif

ifneq "y" "$(CONFIG_ARDUINO_SELECTIVE_HTTPClient)"
MODULE := HTTPClient
SPECIFIC_LIBS += $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
endif

ifneq "y" "$(CONFIG_ARDUINO_SELECTIVE_NetBIOS)"
MODULE := NetBIOS
SPECIFIC_LIBS += $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
endif

ifneq "y" "$(CONFIG_ARDUINO_SELECTIVE_Preferences)"
MODULE := Preferences
SPECIFIC_LIBS += $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
endif

ifneq "y" "$(CONFIG_ARDUINO_SELECTIVE_SD)"
MODULE := SD
SPECIFIC_LIBS += $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
endif

ifneq "y" "$(CONFIG_ARDUINO_SELECTIVE_SD_MMC)"
MODULE := SD_MMC
SPECIFIC_LIBS += $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
endif

ifneq "y" "$(CONFIG_ARDUINO_SELECTIVE_SimpleBLE)"
MODULE := SimpleBLE
SPECIFIC_LIBS += $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
endif

ifneq "y" "$(CONFIG_ARDUINO_SELECTIVE_SPI)"
MODULE := SPI
SPECIFIC_LIBS += $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
endif

ifneq "y" "$(CONFIG_ARDUINO_SELECTIVE_SPIFFS)"
MODULE := SPIFFS
SPECIFIC_LIBS += $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
endif

ifneq "y" "$(CONFIG_ARDUINO_SELECTIVE_Ticker)"
MODULE := Ticker
SPECIFIC_LIBS += $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
endif

ifneq "y" "$(CONFIG_ARDUINO_SELECTIVE_Update)"
MODULE := Update
SPECIFIC_LIBS += $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
endif

ifneq "y" "$(CONFIG_ARDUINO_SELECTIVE_WebServer)"
MODULE := WebServer
SPECIFIC_LIBS += $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
endif

ifneq "y" "$(CONFIG_ARDUINO_SELECTIVE_WiFi)"
MODULE := WiFi
SPECIFIC_LIBS += $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
endif

ifneq "y" "$(CONFIG_ARDUINO_SELECTIVE_WiFiClientSecure)"
MODULE := WiFiClientSecure
SPECIFIC_LIBS += $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
endif

ifneq "y" "$(CONFIG_ARDUINO_SELECTIVE_Wire)"
MODULE := Wire
SPECIFIC_LIBS += $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
endif

ARDUINO_CORE_LIBS := $(filter-out $(SPECIFIC_LIBS),$(ARDUINO_CORE_LIBS))

COMPONENT_ADD_INCLUDEDIRS := cores/esp32 variants/esp32 $(ARDUINO_CORE_LIBS)
COMPONENT_PRIV_INCLUDEDIRS := cores/esp32/libb64
COMPONENT_SRCDIRS := cores/esp32/libb64 cores/esp32 variants/esp32 $(ARDUINO_CORE_LIBS)
CXXFLAGS += -fno-rtti
