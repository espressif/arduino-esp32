ARDUINO_CORE_LIBS := $(patsubst $(COMPONENT_PATH)/%,%,$(wildcard $(COMPONENT_PATH)/libraries/*/src))

COMPONENT_ADD_INCLUDEDIRS := cores/esp32 variants/esp32 $(ARDUINO_CORE_LIBS)
COMPONENT_PRIV_INCLUDEDIRS := cores/esp32/libb64
COMPONENT_SRCDIRS := cores/esp32/libb64 cores/esp32 variants/esp32 $(ARDUINO_CORE_LIBS)
CXXFLAGS += -fno-rtti
