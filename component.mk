COMPONENT_ADD_INCLUDEDIRS := cores/esp32 variants/esp32 libraries/WiFi/src libraries/SPI/src libraries/Wire/src
COMPONENT_PRIV_INCLUDEDIRS := cores/esp32/libb64
COMPONENT_SRCDIRS := cores/esp32/libb64 cores/esp32 variants/esp32 libraries/WiFi/src libraries/SPI/src libraries/Wire/src
CXXFLAGS += -fno-rtti
