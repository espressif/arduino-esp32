ARDUINO_LIBRARIES_LIST := $(patsubst $(COMPONENT_PATH)/libraries/%,%,$(wildcard $(COMPONENT_PATH)/libraries/*))
ARDUINO_SINGLE_LIBRARY_FILES = $(patsubst $(COMPONENT_PATH)/%,%,$(sort $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/)) $(dir $(wildcard $(COMPONENT_PATH)/libraries/$(MODULE)/*/*/*/*/*/))))
ARDUINO_CORE_LIBS := $(foreach MODULE,$(ARDUINO_LIBRARIES_LIST),$(if $(CONFIG_ARDUINO_SELECTIVE_COMPILATION),$(if $(CONFIG_ARDUINO_SELECTIVE_$(MODULE)),$(ARDUINO_SINGLE_LIBRARY_FILES)),$(ARDUINO_SINGLE_LIBRARY_FILES)))

COMPONENT_ADD_INCLUDEDIRS := cores/esp32 variants/esp32 $(ARDUINO_CORE_LIBS)
COMPONENT_PRIV_INCLUDEDIRS := cores/esp32/libb64
COMPONENT_SRCDIRS := cores/esp32/libb64 cores/esp32 variants/esp32 $(ARDUINO_CORE_LIBS)
CXXFLAGS += -fno-rtti
