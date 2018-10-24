/*
 ESP8266WiFiSTA.cpp - WiFi library for esp8266

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
#include "WiFiSTA.h"

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
#include <esp32-hal.h>
#include <lwip/ip_addr.h>
#include "lwip/err.h"
#include "lwip/dns.h"
#include <esp_smartconfig.h>
#include <tcpip_adapter.h>
}

// -----------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------- Private functions ------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

static bool sta_config_equal(const wifi_config_t& lhs, const wifi_config_t& rhs);


/**
 * compare two STA configurations
 * @param lhs station_config
 * @param rhs station_config
 * @return equal
 */
static bool sta_config_equal(const wifi_config_t& lhs, const wifi_config_t& rhs)
{
    if(memcmp(&lhs, &rhs, sizeof(wifi_config_t)) != 0) {
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------- STA function -----------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

bool WiFiSTAClass::_autoReconnect = true;
bool WiFiSTAClass::_useStaticIp = false;

static wl_status_t _sta_status = WL_NO_SHIELD;
static EventGroupHandle_t _sta_status_group = NULL;

void WiFiSTAClass::_setStatus(wl_status_t status)
{
    if(!_sta_status_group){
        _sta_status_group = xEventGroupCreate();
        if(!_sta_status_group){
            log_e("STA Status Group Create Failed!");
            _sta_status = status;
            return;
        }
    }
    xEventGroupClearBits(_sta_status_group, 0x00FFFFFF);
    xEventGroupSetBits(_sta_status_group, status);
}

/**
 * Return Connection status.
 * @return one of the value defined in wl_status_t
 *
 */
wl_status_t WiFiSTAClass::status()
{
    if(!_sta_status_group){
        return _sta_status;
    }
    return (wl_status_t)xEventGroupClearBits(_sta_status_group, 0);
}

/**
 * Start Wifi connection
 * if passphrase is set the most secure supported mode will be automatically selected
 * @param ssid const char*          Pointer to the SSID string.
 * @param passphrase const char *   Optional. Passphrase. Valid characters in a passphrase must be between ASCII 32-126 (decimal).
 * @param bssid uint8_t[6]          Optional. BSSID / MAC of AP
 * @param channel                   Optional. Channel of AP
 * @param connect                   Optional. call connect
 * @return
 */
wl_status_t WiFiSTAClass::begin(const char* ssid, const char *passphrase, int32_t channel, const uint8_t* bssid, bool connect)
{

    if(!WiFi.enableSTA(true)) {
        log_e("STA enable failed!");
        return WL_CONNECT_FAILED;
    }

    if(!ssid || *ssid == 0x00 || strlen(ssid) > 31) {
        log_e("SSID too long or missing!");
        return WL_CONNECT_FAILED;
    }

    if(passphrase && strlen(passphrase) > 64) {
        log_e("passphrase too long!");
        return WL_CONNECT_FAILED;
    }

    wifi_config_t conf;
    memset(&conf, 0, sizeof(wifi_config_t));
    strcpy(reinterpret_cast<char*>(conf.sta.ssid), ssid);

    if(passphrase) {
        if (strlen(passphrase) == 64){ // it's not a passphrase, is the PSK
            memcpy(reinterpret_cast<char*>(conf.sta.password), passphrase, 64);
        } else {
            strcpy(reinterpret_cast<char*>(conf.sta.password), passphrase);
        }
    }

    if(bssid) {
        conf.sta.bssid_set = 1;
        memcpy((void *) &conf.sta.bssid[0], (void *) bssid, 6);
    }

    if(channel > 0 && channel <= 13) {
        conf.sta.channel = channel;
    }

    wifi_config_t current_conf;
    esp_wifi_get_config(WIFI_IF_STA, &current_conf);
    if(!sta_config_equal(current_conf, conf)) {
        if(esp_wifi_disconnect()){
            log_e("disconnect failed!");
            return WL_CONNECT_FAILED;
        }

        esp_wifi_set_config(WIFI_IF_STA, &conf);
    } else if(status() == WL_CONNECTED){
        return WL_CONNECTED;
    }

    if(!_useStaticIp) {
        if(tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_STA) == ESP_ERR_TCPIP_ADAPTER_DHCPC_START_FAILED){
            log_e("dhcp client start failed!");
            return WL_CONNECT_FAILED;
        }
    } else {
        tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);
    }

    if(connect && esp_wifi_connect()) {
        log_e("connect failed!");
        return WL_CONNECT_FAILED;
    }

    return status();
}

wl_status_t WiFiSTAClass::begin(char* ssid, char *passphrase, int32_t channel, const uint8_t* bssid, bool connect)
{
    return begin((const char*) ssid, (const char*) passphrase, channel, bssid, connect);
}

/**
 * Use to connect to SDK config.
 * @return wl_status_t
 */
wl_status_t WiFiSTAClass::begin()
{

    if(!WiFi.enableSTA(true)) {
        log_e("STA enable failed!");
        return WL_CONNECT_FAILED;
    }

    if(!_useStaticIp) {
        if(tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_STA) == ESP_ERR_TCPIP_ADAPTER_DHCPC_START_FAILED){
            log_e("dhcp client start failed!");
            return WL_CONNECT_FAILED;
        }
    } else {
        tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);
    }

    if(status() != WL_CONNECTED && esp_wifi_connect()){
        log_e("connect failed!");
        return WL_CONNECT_FAILED;
    }

    return status();
}

/**
 * will force a disconnect an then start reconnecting to AP
 * @return ok
 */
bool WiFiSTAClass::reconnect()
{
    if(WiFi.getMode() & WIFI_MODE_STA) {
        if(esp_wifi_disconnect() == ESP_OK) {
            return esp_wifi_connect() == ESP_OK;
        }
    }
    return false;
}

/**
 * Disconnect from the network
 * @param wifioff
 * @return  one value of wl_status_t enum
 */
bool WiFiSTAClass::disconnect(bool wifioff, bool eraseap)
{
    wifi_config_t conf;

    if(WiFi.getMode() & WIFI_MODE_STA){
        if(eraseap){
            memset(&conf, 0, sizeof(wifi_config_t));
            if(esp_wifi_set_config(WIFI_IF_STA, &conf)){
                log_e("clear config failed!");
            }
        }
        if(esp_wifi_disconnect()){
            log_e("disconnect failed!");
            return false;
        }
        if(wifioff) {
             return WiFi.enableSTA(false);
        }
        return true;
    }

    return false;
}

/**
 * Change IP configuration settings disabling the dhcp client
 * @param local_ip   Static ip configuration
 * @param gateway    Static gateway configuration
 * @param subnet     Static Subnet mask
 * @param dns1       Static DNS server 1
 * @param dns2       Static DNS server 2
 */
bool WiFiSTAClass::config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1, IPAddress dns2)
{
    esp_err_t err = ESP_OK;

    if(!WiFi.enableSTA(true)) {
        return false;
    }

    tcpip_adapter_ip_info_t info;

    if(local_ip != (uint32_t)0x00000000){
        info.ip.addr = static_cast<uint32_t>(local_ip);
        info.gw.addr = static_cast<uint32_t>(gateway);
        info.netmask.addr = static_cast<uint32_t>(subnet);
    } else {
        info.ip.addr = 0;
        info.gw.addr = 0;
        info.netmask.addr = 0;
    }

    err = tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);
    if(err != ESP_OK && err != ESP_ERR_TCPIP_ADAPTER_DHCP_ALREADY_STOPPED){
        log_e("DHCP could not be stopped! Error: %d", err);
        return false;
    }

    err = tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &info);
    if(err != ERR_OK){
        log_e("STA IP could not be configured! Error: %d", err);
        return false;
    }

    if(info.ip.addr){
        _useStaticIp = true;
    } else {
        err = tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_STA);
        if(err == ESP_ERR_TCPIP_ADAPTER_DHCPC_START_FAILED){
            log_e("dhcp client start failed!");
            return false;
        }
        _useStaticIp = false;
    }

    ip_addr_t d;
    d.type = IPADDR_TYPE_V4;

    if(dns1 != (uint32_t)0x00000000) {
        // Set DNS1-Server
        d.u_addr.ip4.addr = static_cast<uint32_t>(dns1);
        dns_setserver(0, &d);
    }

    if(dns2 != (uint32_t)0x00000000) {
        // Set DNS2-Server
        d.u_addr.ip4.addr = static_cast<uint32_t>(dns2);
        dns_setserver(1, &d);
    }

    return true;
}

/**
 * is STA interface connected?
 * @return true if STA is connected to an AD
 */
bool WiFiSTAClass::isConnected()
{
    return (status() == WL_CONNECTED);
}


/**
 * Setting the ESP32 station to connect to the AP (which is recorded)
 * automatically or not when powered on. Enable auto-connect by default.
 * @param autoConnect bool
 * @return if saved
 */
bool WiFiSTAClass::setAutoConnect(bool autoConnect)
{
    /*bool ret;
    ret = esp_wifi_set_auto_connect(autoConnect);
    return ret;*/
    return false;//now deprecated
}

/**
 * Checks if ESP32 station mode will connect to AP
 * automatically or not when it is powered on.
 * @return auto connect
 */
bool WiFiSTAClass::getAutoConnect()
{
    /*bool autoConnect;
    esp_wifi_get_auto_connect(&autoConnect);
    return autoConnect;*/
    return false;//now deprecated
}

bool WiFiSTAClass::setAutoReconnect(bool autoReconnect)
{
    _autoReconnect = autoReconnect;
    return true;
}

bool WiFiSTAClass::getAutoReconnect()
{
    return _autoReconnect;
}

/**
 * Wait for WiFi connection to reach a result
 * returns the status reached or disconnect if STA is off
 * @return wl_status_t
 */
uint8_t WiFiSTAClass::waitForConnectResult()
{
    //1 and 3 have STA enabled
    if((WiFiGenericClass::getMode() & WIFI_MODE_STA) == 0) {
        return WL_DISCONNECTED;
    }
    int i = 0;
    while((!status() || status() >= WL_DISCONNECTED) && i++ < 100) {
        delay(100);
    }
    return status();
}

/**
 * Get the station interface IP address.
 * @return IPAddress station IP
 */
IPAddress WiFiSTAClass::localIP()
{
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return IPAddress();
    }
    tcpip_adapter_ip_info_t ip;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip);
    return IPAddress(ip.ip.addr);
}


/**
 * Get the station interface MAC address.
 * @param mac   pointer to uint8_t array with length WL_MAC_ADDR_LENGTH
 * @return      pointer to uint8_t *
 */
uint8_t* WiFiSTAClass::macAddress(uint8_t* mac)
{
    if(WiFiGenericClass::getMode() != WIFI_MODE_NULL){
        esp_wifi_get_mac(WIFI_IF_STA, mac);
    }
    return mac;
}

/**
 * Get the station interface MAC address.
 * @return String mac
 */
String WiFiSTAClass::macAddress(void)
{
    uint8_t mac[6];
    char macStr[18] = { 0 };
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return String();
    }
    esp_wifi_get_mac(WIFI_IF_STA, mac);

    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
}

/**
 * Get the interface subnet mask address.
 * @return IPAddress subnetMask
 */
IPAddress WiFiSTAClass::subnetMask()
{
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return IPAddress();
    }
    tcpip_adapter_ip_info_t ip;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip);
    return IPAddress(ip.netmask.addr);
}

/**
 * Get the gateway ip address.
 * @return IPAddress gatewayIP
 */
IPAddress WiFiSTAClass::gatewayIP()
{
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return IPAddress();
    }
    tcpip_adapter_ip_info_t ip;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip);
    return IPAddress(ip.gw.addr);
}

/**
 * Get the DNS ip address.
 * @param dns_no
 * @return IPAddress DNS Server IP
 */
IPAddress WiFiSTAClass::dnsIP(uint8_t dns_no)
{
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return IPAddress();
    }
    ip_addr_t dns_ip = dns_getserver(dns_no);
    return IPAddress(dns_ip.u_addr.ip4.addr);
}

/**
 * Return the current SSID associated with the network
 * @return SSID
 */
String WiFiSTAClass::SSID() const
{
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return String();
    }
    wifi_ap_record_t info;
    if(!esp_wifi_sta_get_ap_info(&info)) {
        return String(reinterpret_cast<char*>(info.ssid));
    }
    return String();
}

/**
 * Return the current pre shared key associated with the network
 * @return  psk string
 */
String WiFiSTAClass::psk() const
{
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return String();
    }
    wifi_config_t conf;
    esp_wifi_get_config(WIFI_IF_STA, &conf);
    return String(reinterpret_cast<char*>(conf.sta.password));
}

/**
 * Return the current bssid / mac associated with the network if configured
 * @return bssid uint8_t *
 */
uint8_t* WiFiSTAClass::BSSID(void)
{
    static uint8_t bssid[6];
    wifi_ap_record_t info;
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return NULL;
    }
    if(!esp_wifi_sta_get_ap_info(&info)) {
        memcpy(bssid, info.bssid, 6);
        return reinterpret_cast<uint8_t*>(bssid);
    }
    return NULL;
}

/**
 * Return the current bssid / mac associated with the network if configured
 * @return String bssid mac
 */
String WiFiSTAClass::BSSIDstr(void)
{
    uint8_t* bssid = BSSID();
    if(!bssid){
        return String();
    }
    char mac[18] = { 0 };
    sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
    return String(mac);
}

/**
 * Return the current network RSSI.
 * @return  RSSI value
 */
int8_t WiFiSTAClass::RSSI(void)
{
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return 0;
    }
    wifi_ap_record_t info;
    if(!esp_wifi_sta_get_ap_info(&info)) {
        return info.rssi;
    }
    return 0;
}

/**
 * Get the station interface Host name.
 * @return char array hostname
 */
const char * WiFiSTAClass::getHostname()
{
    const char * hostname = NULL;
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return hostname;
    }
    if(tcpip_adapter_get_hostname(TCPIP_ADAPTER_IF_STA, &hostname)){
        return NULL;
    }
    return hostname;
}

/**
 * Set the station interface Host name.
 * @param  hostname  pointer to const string
 * @return true on   success
 */
bool WiFiSTAClass::setHostname(const char * hostname)
{
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return false;
    }
    return tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, hostname) == 0;
}

/**
 * Enable IPv6 on the station interface.
 * @return true on success
 */
bool WiFiSTAClass::enableIpV6()
{
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return false;
    }
    return tcpip_adapter_create_ip6_linklocal(TCPIP_ADAPTER_IF_STA) == 0;
}

/**
 * Get the station interface IPv6 address.
 * @return IPv6Address
 */
IPv6Address WiFiSTAClass::localIPv6()
{
    static ip6_addr_t addr;
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return IPv6Address();
    }
    if(tcpip_adapter_get_ip6_linklocal(TCPIP_ADAPTER_IF_STA, &addr)){
        return IPv6Address();
    }
    return IPv6Address(addr.addr);
}


bool WiFiSTAClass::_smartConfigStarted = false;
bool WiFiSTAClass::_smartConfigDone = false;


bool WiFiSTAClass::beginSmartConfig() {
    if (_smartConfigStarted) {
        return false;
    }

    if (!WiFi.mode(WIFI_STA)) {
        return false;
    }

    esp_wifi_disconnect();

    esp_err_t err;
    err = esp_smartconfig_start(reinterpret_cast<sc_callback_t>(&WiFiSTAClass::_smartConfigCallback), 1);
    if (err == ESP_OK) {
        _smartConfigStarted = true;
        _smartConfigDone = false;
        return true;
    }
    return false;
}

bool WiFiSTAClass::stopSmartConfig() {
    if (!_smartConfigStarted) {
        return true;
    }

    if (esp_smartconfig_stop() == ESP_OK) {
        _smartConfigStarted = false;
        return true;
    }

    return false;
}

bool WiFiSTAClass::smartConfigDone() {
    if (!_smartConfigStarted) {
        return false;
    }

    return _smartConfigDone;
}

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
const char * sc_status_strings[] = {
    "WAIT",
    "FIND_CHANNEL",
    "GETTING_SSID_PSWD",
    "LINK",
    "LINK_OVER"
};

const char * sc_type_strings[] = {
    "ESPTOUCH",
    "AIRKISS",
    "ESPTOUCH_AIRKISS"
};
#endif

void WiFiSTAClass::_smartConfigCallback(uint32_t st, void* result) {
    smartconfig_status_t status = (smartconfig_status_t) st;
    log_d("Status: %s", sc_status_strings[st % 5]);
    if (status == SC_STATUS_GETTING_SSID_PSWD) {
        smartconfig_type_t * type = (smartconfig_type_t *)result;
        log_d("Type: %s", sc_type_strings[*type % 3]);
    } else if (status == SC_STATUS_LINK) {
        wifi_sta_config_t *sta_conf = reinterpret_cast<wifi_sta_config_t *>(result);
        log_d("SSID: %s", (char *)(sta_conf->ssid));
        sta_conf->bssid_set = 0;
        esp_wifi_set_config(WIFI_IF_STA, (wifi_config_t *)sta_conf);
        esp_wifi_connect();
        _smartConfigDone = true;
    } else if (status == SC_STATUS_LINK_OVER) {
        if(result){
            ip4_addr_t * ip = (ip4_addr_t *)result;
            log_d("Sender IP: " IPSTR, IP2STR(ip));
        }
        WiFi.stopSmartConfig();
    }
}
