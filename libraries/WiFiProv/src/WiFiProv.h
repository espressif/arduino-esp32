/*
    WiFiProv.h - Base class for provisioning support
    All right reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#pragma once

#include "soc/soc_caps.h"
#if SOC_WIFI_SUPPORTED

#include "WiFi.h"
#include "HardwareSerial.h"
#include "network_provisioning/manager.h"
//Select the scheme using which you want to provision
typedef enum {
  NETWORK_PROV_SCHEME_SOFTAP,
#if CONFIG_BLUEDROID_ENABLED
  NETWORK_PROV_SCHEME_BLE,
#endif
  NETWORK_PROV_SCHEME_MAX
} prov_scheme_t;

typedef enum {
  NETWORK_PROV_SCHEME_HANDLER_NONE,
#if CONFIG_BLUEDROID_ENABLED
  NETWORK_PROV_SCHEME_HANDLER_FREE_BTDM,
  NETWORK_PROV_SCHEME_HANDLER_FREE_BLE,
  NETWORK_PROV_SCHEME_HANDLER_FREE_BT,
#endif
  NETWORK_PROV_SCHEME_HANDLER_MAX
} scheme_handler_t;

//Provisioning class
class WiFiProvClass {
public:
  void beginProvision(
    prov_scheme_t prov_scheme = NETWORK_PROV_SCHEME_SOFTAP, scheme_handler_t scheme_handler = NETWORK_PROV_SCHEME_HANDLER_NONE,
    network_prov_security_t security = NETWORK_PROV_SECURITY_1, const char *pop = "abcd1234", const char *service_name = NULL, const char *service_key = NULL,
    uint8_t *uuid = NULL, bool reset_provisioned = false
  );
  void endProvision();
  bool disableAutoStop(uint32_t cleanup_delay);
  void printQR(const char *name, const char *pop, const char *transport, Print &out = Serial);
};

extern WiFiProvClass WiFiProv;

#endif /* SOC_WIFI_SUPPORTED */
