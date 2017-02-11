ARDUINO_LIB_DIRS := libraries/WiFi/src libraries/SPI/src libraries/Wire/src libraries/Preferences/src

COMPONENT_ADD_INCLUDEDIRS := cores/esp32 variants/esp32 $(ARDUINO_LIB_DIRS)
COMPONENT_PRIV_INCLUDEDIRS := cores/esp32/libb64
COMPONENT_SRCDIRS := cores/esp32/libb64 cores/esp32 variants/esp32 $(ARDUINO_LIB_DIRS)
CXXFLAGS += -fno-rtti
