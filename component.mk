ARDUINO_CORE_LIBS := $(patsubst $(COMPONENT_PATH)/%,%,$(wildcard $(COMPONENT_PATH)/libraries/*/src))
FILTER_LIBS :=

ifndef CONFIG_BT_ENABLED
ifndef CONFIG_BLUEDROID_ENABLED
FILTER_LIBS += libraries/SimpleBLE/src
endif
endif

ifndef CONFIG_WIFI_ENABLED
FILTER_LIBS += libraries/WiFi/src  \
	       libraries/WiFiClientSecure/src
endif

ARDUINO_CORE_LIBS := $(filter-out $(FILTER_LIBS),$(ARDUINO_CORE_LIBS))
COMPONENT_ADD_INCLUDEDIRS := cores/esp32 variants/esp32 $(ARDUINO_CORE_LIBS)
COMPONENT_PRIV_INCLUDEDIRS := cores/esp32/libb64
COMPONENT_SRCDIRS := cores/esp32/libb64 cores/esp32 variants/esp32 $(ARDUINO_CORE_LIBS)
CXXFLAGS += -fno-rtti
