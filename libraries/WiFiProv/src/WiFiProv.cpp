/*
    WiFiProv.cpp - WiFiProv class for provisioning
    All rights reserved.
 
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

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <esp_err.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp32-hal.h>

#include <wifi_provisioning/scheme_ble.h>
#include <wifi_provisioning/scheme_softap.h>
#include <wifi_provisioning/manager.h>
#undef IPADDR_NONE
#include "WiFiProv.h"
#include "SimpleBLE.h"

extern esp_err_t postToSysQueue(system_prov_event_t *);

static wifi_prov_mgr_config_t config;
static scheme_t prov_scheme;
static const uint8_t custom_service_uuid[16] = {  0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
                                                  0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02, };

#define SERV_NAME_PREFIX_PROV "PROV_"

void provSchemeBLE()
{
    prov_scheme = WIFI_PROV_SCHEME_BLE;
    config.scheme = wifi_prov_scheme_ble;    
}

void provSchemeSoftAP()
{
    prov_scheme = WIFI_PROV_SCHEME_SOFTAP;
    config.scheme = wifi_prov_scheme_softap;
}

static void prov_event_handler(void *user_data, wifi_prov_cb_event_t event, void *event_data)
{
    if (!event) {
         return;
     }

     system_prov_event_t *sys_prov = (system_prov_event_t *)malloc(sizeof(system_prov_event_t));
     if(sys_prov == NULL) {
         log_e("Malloc Failed");
         return;
     }

     sys_prov->prov_event = (wifi_prov_event_t *)malloc(sizeof(wifi_prov_event_t));
     if(sys_prov->prov_event == NULL) {
         log_e("Malloc Failed");
         free(sys_prov);
         return;
     }

     sys_prov->sys_event = (system_event_t *)malloc(sizeof(system_event_t));
     if(sys_prov->sys_event == NULL) {
         log_e("Malloc Failed");
         free(sys_prov->prov_event);
         free(sys_prov);
         return;
     }

     sys_prov->prov_event->event = event;
     sys_prov->prov_event->event_data = event_data;
     sys_prov->sys_event->event_id = SYSTEM_EVENT_MAX;
     esp_err_t check = postToSysQueue(sys_prov);
     if(check == ESP_FAIL) {
         log_e("Provisioning event not send to queue");
         free(sys_prov->sys_event);
         free(sys_prov->prov_event);
         free(sys_prov);
     }
}

static void get_device_service_name(char *service_name, size_t max)
{
    uint8_t eth_mac[6];
    WiFi.macAddress(eth_mac);
    snprintf(service_name, max, "%s%02X%02X%02X",SERV_NAME_PREFIX_PROV, eth_mac[3], eth_mac[4], eth_mac[5]);
}

void WiFiProvClass :: beginProvision(void (*scheme_cb)(), wifi_prov_event_handler_t scheme_event_handler, wifi_prov_security_t security, const char * pop, const char *service_name, const char *service_key, uint8_t * uuid)
{
    WiFi.enableProv(true);
    bool provisioned = false;
    scheme_cb();
    config.scheme_event_handler = scheme_event_handler;
    config.app_event_handler = {
            .event_cb = prov_event_handler,
            .user_data = NULL
            };
 
    wifi_prov_mgr_init(config);
    WiFi.mode(WIFI_MODE_AP);
    wifi_prov_mgr_is_provisioned(&provisioned);
    if(provisioned == false) {
        if(prov_scheme == WIFI_PROV_SCHEME_BLE) {
            service_key = NULL;
            if(uuid == NULL) {
                uuid=(uint8_t *)custom_service_uuid;
            }
            wifi_prov_scheme_ble_set_service_uuid(uuid);
        }

        if(service_name == NULL) {
            char service_name_temp[12];
            get_device_service_name(service_name_temp,sizeof(service_name_temp));
            service_name = (const char *)service_name_temp;
        }

        if(prov_scheme == WIFI_PROV_SCHEME_BLE) {
            log_i("Starting AP using BLE\n service_name : %s\n pop : %s",service_name,pop);

        } else {
            if(service_key == NULL) {
               log_i("Starting AP using SOFTAP\n service_name : %s\n pop : %s",service_name,pop); 
            } else { 
               log_i("Starting AP using SOFTAP\n service_name : %s\n password : %s\n pop : %s",service_name,service_key,pop); 
            }
        }
           
        wifi_prov_mgr_start_provisioning(security,pop,service_name,service_key);

    } else {
        wifi_prov_mgr_deinit();
        WiFi.mode(WIFI_MODE_STA);
        log_i("Already Provisioned, starting Wi-Fi STA");
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
        wifi_config_t conf;
        esp_wifi_get_config(WIFI_IF_STA,&conf);
        log_i("SSID : %s\n",conf.sta.ssid);
#endif
        log_i("CONNECTING TO THE ACCESS POINT : ");
        WiFi.begin(); 
    }
}
WiFiProvClass WiFiProv;
