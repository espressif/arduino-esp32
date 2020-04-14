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

#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "SimpleBLE.h"
//Select the scheme using which you want to provision
enum scheme
{
    WIFI_PROV_SCHEME_BLE,
    WIFI_PROV_SCHEME_SOFTAP,
    WIFI_PROV_SCHEME_CONSOLE
};
//Provisioning class 
class WiFiProvClass
{
    protected:
        static bool prov_enable;
  
    public:
        WiFiProvClass() {
            prov_enable = false;
        }

        bool isProvEnabled();
    
        void beginProvision(scheme prov_scheme = WIFI_PROV_SCHEME_SOFTAP, wifi_prov_event_handler_t scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE, wifi_prov_security_t security = WIFI_PROV_SECURITY_1, const char * pop = "abcd1234", const char * service_name = NULL, const char * service_key = NULL, uint8_t *uuid = NULL);
};

/*
 Event Handler for BLE
      - WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
      - WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BLE
      - WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BT
Event Handler for SOFTAP
      - WIFI_PROV_EVENT_HANDLER_NONE
*/
