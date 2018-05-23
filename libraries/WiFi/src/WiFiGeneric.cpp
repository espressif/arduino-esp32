/*
 ESP8266WiFiGeneric.cpp - WiFi library for esp8266

 Copyright (c) 2014 Ivan Grokhotkov. All rights reserved.
 This file is part of the esp8266 core for Arduino environment.

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

 Reworked on 28 Dec 2015 by Markus Sattler

 */

#include "WiFi.h"
#include "WiFiGeneric.h"

extern "C" {
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#include <esp_err.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include "lwip/ip_addr.h"
#include "lwip/opt.h"
#include "lwip/err.h"
#include "lwip/dns.h"
#include "esp_ipc.h"


} //extern "C"

#include "esp32-hal-log.h"

#undef min
#undef max
#include <vector>

#include "sdkconfig.h"

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

static xQueueHandle _network_event_queue;
static TaskHandle_t _network_event_task_handle = NULL;

static void _network_event_task(void * arg){
    system_event_t *event = NULL;
    for (;;) {
        if(xQueueReceive(_network_event_queue, &event, portMAX_DELAY) == pdTRUE){
            WiFiGenericClass::_eventCallback(arg, event);
        }
    }
    vTaskDelete(NULL);
    _network_event_task_handle = NULL;
}

static esp_err_t _network_event_cb(void *arg, system_event_t *event){
    if (xQueueSend(_network_event_queue, &event, portMAX_DELAY) != pdPASS) {
        log_w("Network Event Queue Send Failed!");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void _start_network_event_task(){
    if(!_network_event_queue){
        _network_event_queue = xQueueCreate(32, sizeof(system_event_t *));
        if(!_network_event_queue){
            log_e("Network Event Queue Create Failed!");
            return;
        }
    }
    if(!_network_event_task_handle){
        xTaskCreatePinnedToCore(_network_event_task, "network_event", 4096, NULL, 2, &_network_event_task_handle, ARDUINO_RUNNING_CORE);
        if(!_network_event_task_handle){
            log_e("Network Event Task Start Failed!");
            return;
        }
    }
    esp_event_loop_init(&_network_event_cb, NULL);
}

void tcpipInit(){
    static bool initialized = false;
    if(!initialized){
        initialized = true;
        _start_network_event_task();
        tcpip_adapter_init();
    }
}

static bool wifiLowLevelInit(bool persistent){
    static bool lowLevelInitDone = false;
    if(!lowLevelInitDone){
        tcpipInit();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_err_t err = esp_wifi_init(&cfg);
        if(err){
            log_e("esp_wifi_init %d", err);
            return false;
        }
        if(!persistent){
          esp_wifi_set_storage(WIFI_STORAGE_RAM);
        }
        esp_wifi_set_mode(WIFI_MODE_NULL);
        lowLevelInitDone = true;
    }
    return true;
}

static bool wifiLowLevelDeinit(){
    //deinit not working yet!
    //esp_wifi_deinit();
    return true;
}

static bool _esp_wifi_started = false;

static bool espWiFiStart(bool persistent){
    if(_esp_wifi_started){
        return true;
    }
    if(!wifiLowLevelInit(persistent)){
        return false;
    }
    esp_err_t err = esp_wifi_start();
    if (err != ESP_OK) {
        log_e("esp_wifi_start %d", err);
        wifiLowLevelDeinit();
        return false;
    }
    _esp_wifi_started = true;
    system_event_t event; 
    event.event_id = SYSTEM_EVENT_WIFI_READY; 
    WiFiGenericClass::_eventCallback(nullptr, &event);

    return true;
}

static bool espWiFiStop(){
    esp_err_t err;
    if(!_esp_wifi_started){
        return true;
    }
    err = esp_wifi_stop();
    if(err){
        log_e("Could not stop WiFi! %u", err);
        return false;
    }
    _esp_wifi_started = false;
    return wifiLowLevelDeinit();
}

// -----------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------- Generic WiFi function -----------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

typedef struct WiFiEventCbList {
    static wifi_event_id_t current_id;
    wifi_event_id_t id;
    WiFiEventCb cb;
    WiFiEventFuncCb fcb;
    WiFiEventSysCb scb;
    system_event_id_t event;

    WiFiEventCbList() : id(current_id++) {}
} WiFiEventCbList_t;
wifi_event_id_t WiFiEventCbList::current_id = 1;


// arduino dont like std::vectors move static here
static std::vector<WiFiEventCbList_t> cbEventList;

bool WiFiGenericClass::_persistent = true;
wifi_mode_t WiFiGenericClass::_forceSleepLastMode = WIFI_MODE_NULL;

WiFiGenericClass::WiFiGenericClass()
{

}

/**
 * set callback function
 * @param cbEvent WiFiEventCb
 * @param event optional filter (WIFI_EVENT_MAX is all events)
 */
wifi_event_id_t WiFiGenericClass::onEvent(WiFiEventCb cbEvent, system_event_id_t event)
{
    if(!cbEvent) {
        return 0;
    }
    WiFiEventCbList_t newEventHandler;
    newEventHandler.cb = cbEvent;
    newEventHandler.fcb = NULL;
    newEventHandler.scb = NULL;
    newEventHandler.event = event;
    cbEventList.push_back(newEventHandler);
    return newEventHandler.id;
}

wifi_event_id_t WiFiGenericClass::onEvent(WiFiEventFuncCb cbEvent, system_event_id_t event)
{
    if(!cbEvent) {
        return 0;
    }
    WiFiEventCbList_t newEventHandler;
    newEventHandler.cb = NULL;
    newEventHandler.fcb = cbEvent;
    newEventHandler.scb = NULL;
    newEventHandler.event = event;
    cbEventList.push_back(newEventHandler);
    return newEventHandler.id;
}

wifi_event_id_t WiFiGenericClass::onEvent(WiFiEventSysCb cbEvent, system_event_id_t event)
{
    if(!cbEvent) {
        return 0;
    }
    WiFiEventCbList_t newEventHandler;
    newEventHandler.cb = NULL;
    newEventHandler.fcb = NULL;
    newEventHandler.scb = cbEvent;
    newEventHandler.event = event;
    cbEventList.push_back(newEventHandler);
    return newEventHandler.id;
}

/**
 * removes a callback form event handler
 * @param cbEvent WiFiEventCb
 * @param event optional filter (WIFI_EVENT_MAX is all events)
 */
void WiFiGenericClass::removeEvent(WiFiEventCb cbEvent, system_event_id_t event)
{
    if(!cbEvent) {
        return;
    }

    for(uint32_t i = 0; i < cbEventList.size(); i++) {
        WiFiEventCbList_t entry = cbEventList[i];
        if(entry.cb == cbEvent && entry.event == event) {
            cbEventList.erase(cbEventList.begin() + i);
        }
    }
}

void WiFiGenericClass::removeEvent(WiFiEventSysCb cbEvent, system_event_id_t event)
{
    if(!cbEvent) {
        return;
    }

    for(uint32_t i = 0; i < cbEventList.size(); i++) {
        WiFiEventCbList_t entry = cbEventList[i];
        if(entry.scb == cbEvent && entry.event == event) {
            cbEventList.erase(cbEventList.begin() + i);
        }
    }
}

void WiFiGenericClass::removeEvent(wifi_event_id_t id)
{
    for(uint32_t i = 0; i < cbEventList.size(); i++) {
        WiFiEventCbList_t entry = cbEventList[i];
        if(entry.id == id) {
            cbEventList.erase(cbEventList.begin() + i);
        }
    }
}

/**
 * callback for WiFi events
 * @param arg
 */
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
const char * system_event_names[] = { "WIFI_READY", "SCAN_DONE", "STA_START", "STA_STOP", "STA_CONNECTED", "STA_DISCONNECTED", "STA_AUTHMODE_CHANGE", "STA_GOT_IP", "STA_LOST_IP", "STA_WPS_ER_SUCCESS", "STA_WPS_ER_FAILED", "STA_WPS_ER_TIMEOUT", "STA_WPS_ER_PIN", "AP_START", "AP_STOP", "AP_STACONNECTED", "AP_STADISCONNECTED", "AP_PROBEREQRECVED", "GOT_IP6", "ETH_START", "ETH_STOP", "ETH_CONNECTED", "ETH_DISCONNECTED", "ETH_GOT_IP", "MAX"};
#endif
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_WARN
const char * system_event_reasons[] = { "UNSPECIFIED", "AUTH_EXPIRE", "AUTH_LEAVE", "ASSOC_EXPIRE", "ASSOC_TOOMANY", "NOT_AUTHED", "NOT_ASSOCED", "ASSOC_LEAVE", "ASSOC_NOT_AUTHED", "DISASSOC_PWRCAP_BAD", "DISASSOC_SUPCHAN_BAD", "UNSPECIFIED", "IE_INVALID", "MIC_FAILURE", "4WAY_HANDSHAKE_TIMEOUT", "GROUP_KEY_UPDATE_TIMEOUT", "IE_IN_4WAY_DIFFERS", "GROUP_CIPHER_INVALID", "PAIRWISE_CIPHER_INVALID", "AKMP_INVALID", "UNSUPP_RSN_IE_VERSION", "INVALID_RSN_IE_CAP", "802_1X_AUTH_FAILED", "CIPHER_SUITE_REJECTED", "BEACON_TIMEOUT", "NO_AP_FOUND", "AUTH_FAIL", "ASSOC_FAIL", "HANDSHAKE_TIMEOUT" };
#define reason2str(r) ((r>176)?system_event_reasons[r-176]:system_event_reasons[r-1])
#endif
esp_err_t WiFiGenericClass::_eventCallback(void *arg, system_event_t *event)
{
    log_d("Event: %d - %s", event->event_id, system_event_names[event->event_id]);
    if(event->event_id == SYSTEM_EVENT_SCAN_DONE) {
        WiFiScanClass::_scanDone();
    } else if(event->event_id == SYSTEM_EVENT_STA_DISCONNECTED) {
        uint8_t reason = event->event_info.disconnected.reason;
        log_w("Reason: %u - %s", reason, reason2str(reason));
        if(reason == WIFI_REASON_NO_AP_FOUND) {
            WiFiSTAClass::_setStatus(WL_NO_SSID_AVAIL);
        } else if(reason == WIFI_REASON_AUTH_FAIL || reason == WIFI_REASON_ASSOC_FAIL) {
            WiFiSTAClass::_setStatus(WL_CONNECT_FAILED);
        } else if(reason == WIFI_REASON_BEACON_TIMEOUT || reason == WIFI_REASON_HANDSHAKE_TIMEOUT) {
            WiFiSTAClass::_setStatus(WL_CONNECTION_LOST);
        } else if(reason == WIFI_REASON_AUTH_EXPIRE) {
            if(WiFi.getAutoReconnect()){
                WiFi.begin();
            }
        } else {
            WiFiSTAClass::_setStatus(WL_DISCONNECTED);
        }
    } else if(event->event_id == SYSTEM_EVENT_STA_START) {
        WiFiSTAClass::_setStatus(WL_DISCONNECTED);
    } else if(event->event_id == SYSTEM_EVENT_STA_STOP) {
        WiFiSTAClass::_setStatus(WL_NO_SHIELD);
    } else if(event->event_id == SYSTEM_EVENT_STA_CONNECTED) {
        WiFiSTAClass::_setStatus(WL_IDLE_STATUS);
    } else if(event->event_id == SYSTEM_EVENT_STA_GOT_IP) {
//#1081 https://github.com/espressif/arduino-esp32/issues/1081		
//        if(WiFiSTAClass::status() == WL_IDLE_STATUS) 
		{        
            WiFiSTAClass::_setStatus(WL_CONNECTED);
        }
    }

    for(uint32_t i = 0; i < cbEventList.size(); i++) {
        WiFiEventCbList_t entry = cbEventList[i];
        if(entry.cb || entry.fcb || entry.scb) {
            if(entry.event == (system_event_id_t) event->event_id || entry.event == SYSTEM_EVENT_MAX) {
                if(entry.cb) {
                    entry.cb((system_event_id_t) event->event_id);
                } else if(entry.fcb) {
                    entry.fcb((system_event_id_t) event->event_id, (system_event_info_t) event->event_info);
                } else {
                    entry.scb(event);
                }
            }
        }
    }
    return ESP_OK;
}

/**
 * Return the current channel associated with the network
 * @return channel (1-13)
 */
int32_t WiFiGenericClass::channel(void)
{
    uint8_t primaryChan;
    wifi_second_chan_t secondChan;
    esp_wifi_get_channel(&primaryChan, &secondChan);
    return primaryChan;
}


/**
 * store WiFi config in SDK flash area
 * @param persistent
 */
void WiFiGenericClass::persistent(bool persistent)
{
    _persistent = persistent;
}


/**
 * set new mode
 * @param m WiFiMode_t
 */
bool WiFiGenericClass::mode(wifi_mode_t m)
{
    if (!_esp_wifi_started) {
         wifiLowLevelInit(_persistent); 
    }
    wifi_mode_t cm = getMode();
    if(cm == WIFI_MODE_MAX){
        return false;
    }
    if(cm == m) {
        return true;
    }
    if(m){
        espWiFiStart(_persistent);
    } else {
        return espWiFiStop();
    }
    
    esp_err_t err;
    err = esp_wifi_set_mode(m);
    if(err){
        log_e("Could not set mode! %u", err);
        return false;
    }
    if(m){
        return espWiFiStart(_persistent);
    }
    return espWiFiStop();
}

/**
 * get WiFi mode
 * @return WiFiMode
 */
wifi_mode_t WiFiGenericClass::getMode()
{
    if(!wifiLowLevelInit(_persistent)){
        return WIFI_MODE_MAX;

    }
    uint8_t mode;
    esp_wifi_get_mode((wifi_mode_t*)&mode);
    return (wifi_mode_t)mode;
}

/**
 * control STA mode
 * @param enable bool
 * @return ok
 */
bool WiFiGenericClass::enableSTA(bool enable)
{

    wifi_mode_t currentMode = getMode();
    bool isEnabled = ((currentMode & WIFI_MODE_STA) != 0);

    if(isEnabled != enable) {
        if(enable) {
            return mode((wifi_mode_t)(currentMode | WIFI_MODE_STA));
        } else {
            return mode((wifi_mode_t)(currentMode & (~WIFI_MODE_STA)));
        }
    } else {
        return true;
    }
}

/**
 * control AP mode
 * @param enable bool
 * @return ok
 */
bool WiFiGenericClass::enableAP(bool enable)
{

    wifi_mode_t currentMode = getMode();
    bool isEnabled = ((currentMode & WIFI_MODE_AP) != 0);

    if(isEnabled != enable) {
        if(enable) {
            return mode((wifi_mode_t)(currentMode | WIFI_MODE_AP));
        } else {
            return mode((wifi_mode_t)(currentMode & (~WIFI_MODE_AP)));
        }
    } else {
        return true;
    }
}


// -----------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------ Generic Network function ---------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

static bool _dns_busy = false;

/**
 * DNS callback
 * @param name
 * @param ipaddr
 * @param callback_arg
 */
static void wifi_dns_found_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    if(ipaddr) {
        (*reinterpret_cast<IPAddress*>(callback_arg)) = ipaddr->u_addr.ip4.addr;
    }
    _dns_busy = false;
}

/**
 * Resolve the given hostname to an IP address.
 * @param aHostname     Name to be resolved
 * @param aResult       IPAddress structure to store the returned IP address
 * @return 1 if aIPAddrString was successfully converted to an IP address,
 *          else error code
 */
int WiFiGenericClass::hostByName(const char* aHostname, IPAddress& aResult)
{
    ip_addr_t addr;
    aResult = static_cast<uint32_t>(0);

    _dns_busy = true;
    err_t err = dns_gethostbyname(aHostname, &addr, &wifi_dns_found_callback, &aResult);
    if(err == ERR_OK && addr.u_addr.ip4.addr) {
        aResult = addr.u_addr.ip4.addr;
        _dns_busy = false;
    } else if(err == ERR_INPROGRESS) {
        while(_dns_busy){
            delay(1);
        }
    } else {
        _dns_busy = false;
        return 0;
    }
    return 1;
}

