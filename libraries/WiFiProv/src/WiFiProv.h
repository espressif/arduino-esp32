// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "WiFi.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"
//Select the scheme using which you want to provision
typedef enum 
{
    WIFI_PROV_SCHEME_BLE,
    WIFI_PROV_SCHEME_SOFTAP
}scheme_t;

extern void provSchemeSoftAP();
extern void provSchemeBLE();

//Provisioning class 
class WiFiProvClass
{
    public:

    void beginProvision(void (*scheme_cb)() = provSchemeSoftAP, wifi_prov_event_handler_t scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE, wifi_prov_security_t security = WIFI_PROV_SECURITY_1, const char * pop = "abcd1234", const char * service_name = NULL, const char * service_key = NULL, uint8_t *uuid = NULL);
};
extern WiFiProvClass WiFiProv;
/*
 Event Handler for BLE
      - WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
      - WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BLE
      - WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BT
Event Handler for SOFTAP
      - WIFI_PROV_EVENT_HANDLER_NONE
*/
