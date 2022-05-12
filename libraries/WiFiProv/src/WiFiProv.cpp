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
#include <esp_event.h>
#include <esp32-hal.h>

#include <nvs_flash.h>
#if CONFIG_BLUEDROID_ENABLED
#include "wifi_provisioning/scheme_ble.h"
#endif
#include <wifi_provisioning/scheme_softap.h>
#include <wifi_provisioning/manager.h>
#undef IPADDR_NONE
#include "WiFiProv.h"
#if CONFIG_IDF_TARGET_ESP32
#include "SimpleBLE.h"
#endif

bool wifiLowLevelInit(bool persistent);

#if CONFIG_BLUEDROID_ENABLED
static const uint8_t custom_service_uuid[16] = {  0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
                                                  0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02, };
#endif

#define SERV_NAME_PREFIX_PROV "PROV_"

static void get_device_service_name(prov_scheme_t prov_scheme, char *service_name, size_t max)
{
    uint8_t eth_mac[6] = {0,0,0,0,0,0};
    if(esp_wifi_get_mac((wifi_interface_t)WIFI_IF_STA, eth_mac) != ESP_OK){
    	log_e("esp_wifi_get_mac failed!");
    	return;
    }
#if CONFIG_IDF_TARGET_ESP32 && defined(CONFIG_BLUEDROID_ENABLED)
    if(prov_scheme == WIFI_PROV_SCHEME_BLE) {
        snprintf(service_name, max, "%s%02X%02X%02X",SERV_NAME_PREFIX_PROV, eth_mac[3], eth_mac[4], eth_mac[5]);
    } else {
#endif
         snprintf(service_name, max, "%s%02X%02X%02X",SERV_NAME_PREFIX_PROV, eth_mac[3], eth_mac[4], eth_mac[5]);
#if CONFIG_IDF_TARGET_ESP32 && defined(CONFIG_BLUEDROID_ENABLED)
    }
#endif
}

void WiFiProvClass :: beginProvision(prov_scheme_t prov_scheme, scheme_handler_t scheme_handler, wifi_prov_security_t security, const char * pop, const char *service_name, const char *service_key, uint8_t * uuid)
{
    bool provisioned = false;
    static char service_name_temp[32];

    wifi_prov_mgr_config_t config;
#if CONFIG_BLUEDROID_ENABLED
    if(prov_scheme == WIFI_PROV_SCHEME_BLE) {
        config.scheme = wifi_prov_scheme_ble;
    } else {
#endif
    	config.scheme = wifi_prov_scheme_softap;
#if CONFIG_BLUEDROID_ENABLED
    }

    if(scheme_handler == WIFI_PROV_SCHEME_HANDLER_NONE){
#endif
    	wifi_prov_event_handler_t scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE;
    	memcpy(&config.scheme_event_handler, &scheme_event_handler, sizeof(wifi_prov_event_handler_t));
#if CONFIG_BLUEDROID_ENABLED
    } else if(scheme_handler == WIFI_PROV_SCHEME_HANDLER_FREE_BTDM){
    	wifi_prov_event_handler_t scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM;
    	memcpy(&config.scheme_event_handler, &scheme_event_handler, sizeof(wifi_prov_event_handler_t));
    } else if(scheme_handler == WIFI_PROV_SCHEME_HANDLER_FREE_BT){
    	wifi_prov_event_handler_t scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BT;
    	memcpy(&config.scheme_event_handler, &scheme_event_handler, sizeof(wifi_prov_event_handler_t));
    } else if(scheme_handler == WIFI_PROV_SCHEME_HANDLER_FREE_BLE){
    	wifi_prov_event_handler_t scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BLE;
    	memcpy(&config.scheme_event_handler, &scheme_event_handler, sizeof(wifi_prov_event_handler_t));
    } else {
    	log_e("Unknown scheme handler!");
    	return;
    }
#endif
    config.app_event_handler.event_cb = NULL;
    config.app_event_handler.user_data = NULL;
    wifiLowLevelInit(true);
    if(wifi_prov_mgr_init(config) != ESP_OK){
    	log_e("wifi_prov_mgr_init failed!");
    	return;
    }
    if(wifi_prov_mgr_is_provisioned(&provisioned) != ESP_OK){
    	log_e("wifi_prov_mgr_is_provisioned failed!");
    	wifi_prov_mgr_deinit();
    	return;
    }
    if(provisioned == false) {
#if CONFIG_BLUEDROID_ENABLED
        if(prov_scheme == WIFI_PROV_SCHEME_BLE) {
            service_key = NULL;
            if(uuid == NULL) {
                uuid=(uint8_t *)custom_service_uuid;
            }
            wifi_prov_scheme_ble_set_service_uuid(uuid);
        }
#endif

        if(service_name == NULL) {
            get_device_service_name(prov_scheme, service_name_temp, 32);
            service_name = (const char *)service_name_temp;
        }

#if CONFIG_BLUEDROID_ENABLED
        if(prov_scheme == WIFI_PROV_SCHEME_BLE) {
            log_i("Starting AP using BLE. service_name : %s, pop : %s",service_name,pop);
        } else {
#endif
            if(service_key == NULL) {
               log_i("Starting provisioning AP using SOFTAP. service_name : %s, pop : %s",service_name,pop);
            } else {
               log_i("Starting provisioning AP using SOFTAP. service_name : %s, password : %s, pop : %s",service_name,service_key,pop);
            }
#if CONFIG_BLUEDROID_ENABLED
        }
#endif
        if(wifi_prov_mgr_start_provisioning(security, pop, service_name, service_key) != ESP_OK){
        	log_e("wifi_prov_mgr_start_provisioning failed!");
        	return;
        }
    } else {
        log_i("Already Provisioned");
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
        static wifi_config_t conf;
        esp_wifi_get_config((wifi_interface_t)WIFI_IF_STA,&conf);
        log_i("Attempting connect to AP: %s\n",conf.sta.ssid);
#endif
        esp_wifi_start();        
        wifi_prov_mgr_deinit();
        WiFi.begin();
    }
}

WiFiProvClass WiFiProv;
