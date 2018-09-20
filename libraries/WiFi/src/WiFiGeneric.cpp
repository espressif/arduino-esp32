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
#include <vector>

#include "sdkconfig.h"

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

static xQueueHandle _network_event_queue;
static TaskHandle_t _network_event_task_handle = NULL;
static EventGroupHandle_t _network_event_group = NULL;

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

static bool _start_network_event_task(){
    if(!_network_event_group){
        _network_event_group = xEventGroupCreate();
        if(!_network_event_group){
            log_e("Network Event Group Create Failed!");
            return false;
        }
        xEventGroupSetBits(_network_event_group, WIFI_DNS_IDLE_BIT);
    }
    if(!_network_event_queue){
        _network_event_queue = xQueueCreate(32, sizeof(system_event_t *));
        if(!_network_event_queue){
            log_e("Network Event Queue Create Failed!");
            return false;
        }
    }
    if(!_network_event_task_handle){
        xTaskCreatePinnedToCore(_network_event_task, "network_event", 4096, NULL, 2, &_network_event_task_handle, ARDUINO_RUNNING_CORE);
        if(!_network_event_task_handle){
            log_e("Network Event Task Start Failed!");
            return false;
        }
    }
    return esp_event_loop_init(&_network_event_cb, NULL) == ESP_OK;
}

void tcpipInit(){
    static bool initialized = false;
    if(!initialized && _start_network_event_task()){
        initialized = true;
        tcpip_adapter_init();
    }
}

static bool lowLevelInitDone = false;
static bool wifiLowLevelInit(bool persistent){
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
    _esp_wifi_started = false;
    err = esp_wifi_stop();
    if(err){
        log_e("Could not stop WiFi! %u", err);
        _esp_wifi_started = true;
        return false;
    }
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

int WiFiGenericClass::setStatusBits(int bits){
    if(!_network_event_group){
        return 0;
    }
    return xEventGroupSetBits(_network_event_group, bits);
}

int WiFiGenericClass::clearStatusBits(int bits){
    if(!_network_event_group){
        return 0;
    }
    return xEventGroupClearBits(_network_event_group, bits);
}

int WiFiGenericClass::getStatusBits(){
    if(!_network_event_group){
        return 0;
    }
    return xEventGroupGetBits(_network_event_group);
}

int WiFiGenericClass::waitStatusBits(int bits, uint32_t timeout_ms){
    if(!_network_event_group){
        return 0;
    }
    return xEventGroupWaitBits(
        _network_event_group,    // The event group being tested.
        bits,  // The bits within the event group to wait for.
        pdFALSE,         // BIT_0 and BIT_4 should be cleared before returning.
        pdTRUE,        // Don't wait for both bits, either bit will do.
        timeout_ms / portTICK_PERIOD_MS ) & bits; // Wait a maximum of 100ms for either bit to be set.
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
const char * system_event_names[] = { "WIFI_READY", "SCAN_DONE", "STA_START", "STA_STOP", "STA_CONNECTED", "STA_DISCONNECTED", "STA_AUTHMODE_CHANGE", "STA_GOT_IP", "STA_LOST_IP", "STA_WPS_ER_SUCCESS", "STA_WPS_ER_FAILED", "STA_WPS_ER_TIMEOUT", "STA_WPS_ER_PIN", "AP_START", "AP_STOP", "AP_STACONNECTED", "AP_STADISCONNECTED", "AP_STAIPASSIGNED", "AP_PROBEREQRECVED", "GOT_IP6", "ETH_START", "ETH_STOP", "ETH_CONNECTED", "ETH_DISCONNECTED", "ETH_GOT_IP", "MAX"};
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

    } else if(event->event_id == SYSTEM_EVENT_STA_START) {
        WiFiSTAClass::_setStatus(WL_DISCONNECTED);
        setStatusBits(STA_STARTED_BIT);
    } else if(event->event_id == SYSTEM_EVENT_STA_STOP) {
        WiFiSTAClass::_setStatus(WL_NO_SHIELD);
        clearStatusBits(STA_STARTED_BIT | STA_CONNECTED_BIT | STA_HAS_IP_BIT | STA_HAS_IP6_BIT);
    } else if(event->event_id == SYSTEM_EVENT_STA_CONNECTED) {
        WiFiSTAClass::_setStatus(WL_IDLE_STATUS);
        setStatusBits(STA_CONNECTED_BIT);
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

        } else {
            WiFiSTAClass::_setStatus(WL_DISCONNECTED);
        }
        clearStatusBits(STA_CONNECTED_BIT | STA_HAS_IP_BIT | STA_HAS_IP6_BIT);
        if(((reason == WIFI_REASON_AUTH_EXPIRE) ||
            (reason >= WIFI_REASON_BEACON_TIMEOUT && reason != WIFI_REASON_AUTH_FAIL)) &&
            WiFi.getAutoReconnect())
        {
            WiFi.enableSTA(false);
            WiFi.enableSTA(true);
            WiFi.begin();
        }
    } else if(event->event_id == SYSTEM_EVENT_STA_GOT_IP) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
        uint8_t * ip = (uint8_t *)&(event->event_info.got_ip.ip_info.ip.addr);
        uint8_t * mask = (uint8_t *)&(event->event_info.got_ip.ip_info.netmask.addr);
        uint8_t * gw = (uint8_t *)&(event->event_info.got_ip.ip_info.gw.addr);
        log_d("STA IP: %u.%u.%u.%u, MASK: %u.%u.%u.%u, GW: %u.%u.%u.%u",
            ip[0], ip[1], ip[2], ip[3],
            mask[0], mask[1], mask[2], mask[3],
            gw[0], gw[1], gw[2], gw[3]);
#endif
        WiFiSTAClass::_setStatus(WL_CONNECTED);
        setStatusBits(STA_HAS_IP_BIT | STA_CONNECTED_BIT);
    } else if(event->event_id == SYSTEM_EVENT_STA_LOST_IP) {
        WiFiSTAClass::_setStatus(WL_IDLE_STATUS);
        clearStatusBits(STA_HAS_IP_BIT);

    } else if(event->event_id == SYSTEM_EVENT_AP_START) {
        setStatusBits(AP_STARTED_BIT);
    } else if(event->event_id == SYSTEM_EVENT_AP_STOP) {
        clearStatusBits(AP_STARTED_BIT | AP_HAS_CLIENT_BIT);
    } else if(event->event_id == SYSTEM_EVENT_AP_STACONNECTED) {
        setStatusBits(AP_HAS_CLIENT_BIT);
    } else if(event->event_id == SYSTEM_EVENT_AP_STADISCONNECTED) {
        wifi_sta_list_t clients;
        if(esp_wifi_ap_get_sta_list(&clients) != ESP_OK || !clients.num){
            clearStatusBits(AP_HAS_CLIENT_BIT);
        }

    } else if(event->event_id == SYSTEM_EVENT_ETH_START) {
        setStatusBits(ETH_STARTED_BIT);
    } else if(event->event_id == SYSTEM_EVENT_ETH_STOP) {
        clearStatusBits(ETH_STARTED_BIT | ETH_CONNECTED_BIT | ETH_HAS_IP_BIT | ETH_HAS_IP6_BIT);
    } else if(event->event_id == SYSTEM_EVENT_ETH_CONNECTED) {
        setStatusBits(ETH_CONNECTED_BIT);
    } else if(event->event_id == SYSTEM_EVENT_ETH_DISCONNECTED) {
        clearStatusBits(ETH_CONNECTED_BIT | ETH_HAS_IP_BIT | ETH_HAS_IP6_BIT);
    } else if(event->event_id == SYSTEM_EVENT_ETH_GOT_IP) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
        uint8_t * ip = (uint8_t *)&(event->event_info.got_ip.ip_info.ip.addr);
        uint8_t * mask = (uint8_t *)&(event->event_info.got_ip.ip_info.netmask.addr);
        uint8_t * gw = (uint8_t *)&(event->event_info.got_ip.ip_info.gw.addr);
        log_d("ETH IP: %u.%u.%u.%u, MASK: %u.%u.%u.%u, GW: %u.%u.%u.%u",
            ip[0], ip[1], ip[2], ip[3],
            mask[0], mask[1], mask[2], mask[3],
            gw[0], gw[1], gw[2], gw[3]);
#endif
        setStatusBits(ETH_CONNECTED_BIT | ETH_HAS_IP_BIT);

    } else if(event->event_id == SYSTEM_EVENT_GOT_IP6) {
        if(event->event_info.got_ip6.if_index == TCPIP_ADAPTER_IF_AP){
            setStatusBits(AP_HAS_IP6_BIT);
        } else if(event->event_info.got_ip6.if_index == TCPIP_ADAPTER_IF_STA){
            setStatusBits(STA_CONNECTED_BIT | STA_HAS_IP6_BIT);
        } else if(event->event_info.got_ip6.if_index == TCPIP_ADAPTER_IF_ETH){
            setStatusBits(ETH_CONNECTED_BIT | ETH_HAS_IP6_BIT);
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
    uint8_t primaryChan = 0;
    wifi_second_chan_t secondChan = WIFI_SECOND_CHAN_NONE;
    if(!lowLevelInitDone){
        return primaryChan;
    }
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
    wifi_mode_t cm = getMode();
    if(cm == m) {
        return true;
    }
    if(!cm && m){
        if(!espWiFiStart(_persistent)){
            return false;
        }
    } else if(cm && !m){
        return espWiFiStop();
    }

    esp_err_t err;
    err = esp_wifi_set_mode(m);
    if(err){
        log_e("Could not set mode! %u", err);
        return false;
    }
    return true;
}

/**
 * get WiFi mode
 * @return WiFiMode
 */
wifi_mode_t WiFiGenericClass::getMode()
{
    if(!_esp_wifi_started){
        return WIFI_MODE_NULL;
    }
    wifi_mode_t mode;
    if(esp_wifi_get_mode(&mode) == ESP_ERR_WIFI_NOT_INIT){
        log_w("WiFi not started");
        return WIFI_MODE_NULL;
    }
    return mode;
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
        }
        return mode((wifi_mode_t)(currentMode & (~WIFI_MODE_STA)));
    }
    return true;
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
        }
        return mode((wifi_mode_t)(currentMode & (~WIFI_MODE_AP)));
    }
    return true;
}

/**
 * control modem sleep when only in STA mode
 * @param enable bool
 * @return ok
 */
bool WiFiGenericClass::setSleep(bool enable)
{
    if((getMode() & WIFI_MODE_STA) == 0){
        log_w("STA has not been started");
        return false;
    }
    return esp_wifi_set_ps(enable?WIFI_PS_MIN_MODEM:WIFI_PS_NONE) == ESP_OK;
}

/**
 * get modem sleep enabled
 * @return true if modem sleep is enabled
 */
bool WiFiGenericClass::getSleep()
{
    wifi_ps_type_t ps;
    if((getMode() & WIFI_MODE_STA) == 0){
        log_w("STA has not been started");
        return false;
    }
    if(esp_wifi_get_ps(&ps) == ESP_OK){
        return ps == WIFI_PS_MIN_MODEM;
    }
    return false;
}

/**
 * control wifi tx power
 * @param power enum maximum wifi tx power
 * @return ok
 */
bool WiFiGenericClass::setTxPower(wifi_power_t power){
    if((getStatusBits() & (STA_STARTED_BIT | AP_STARTED_BIT)) == 0){
        log_w("Neither AP or STA has been started");
        return false;
    }
    return esp_wifi_set_max_tx_power(power) == ESP_OK;
}

wifi_power_t WiFiGenericClass::getTxPower(){
    int8_t power;
    if((getStatusBits() & (STA_STARTED_BIT | AP_STARTED_BIT)) == 0){
        log_w("Neither AP or STA has been started");
        return WIFI_POWER_19_5dBm;
    }
    if(esp_wifi_get_max_tx_power(&power)){
        return WIFI_POWER_19_5dBm;
    }
    return (wifi_power_t)power;
}

// -----------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------ Generic Network function ---------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

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
    xEventGroupSetBits(_network_event_group, WIFI_DNS_DONE_BIT);
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
    waitStatusBits(WIFI_DNS_IDLE_BIT, 5000);
    clearStatusBits(WIFI_DNS_IDLE_BIT);
    err_t err = dns_gethostbyname(aHostname, &addr, &wifi_dns_found_callback, &aResult);
    if(err == ERR_OK && addr.u_addr.ip4.addr) {
        aResult = addr.u_addr.ip4.addr;
    } else if(err == ERR_INPROGRESS) {
        waitStatusBits(WIFI_DNS_DONE_BIT, 4000);
        clearStatusBits(WIFI_DNS_DONE_BIT);
    }
    setStatusBits(WIFI_DNS_IDLE_BIT);
    if((uint32_t)aResult == 0){
        log_e("DNS Failed for %s", aHostname);
    }
    return (uint32_t)aResult != 0;
}

