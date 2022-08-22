#ifndef UPDATE_NETWORK_INTERFACE
#define UPDATE_NETWORK_INTERFACE

#include <stdint.h>

enum class UpdateNetworkInterface : const uint8_t {
    DEFAULT_IFC,
    WIFI_STA_IFC,
    WIFI_AP_IFC,
    ETHERNET_IFC,
    POINT_TO_POINT_IFC,
    SERIAL_LINE_IFC
};

#endif // UPDATE_NETWORK_INTERFACE
