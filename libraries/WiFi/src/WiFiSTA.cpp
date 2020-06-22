/*
 WiFiSTA.cpp - WiFi library for esp32

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
#include <esp_event.h>
#include <esp32-hal.h>
#include <lwip/ip_addr.h>
#include "lwip/err.h"
#include "lwip/dns.h"
#include <esp_smartconfig.h>
#include <esp_netif.h>
}

// -----------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------- Private functions ------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

esp_netif_t* get_esp_interface_netif(esp_interface_t interface);
esp_err_t set_esp_interface_dns(esp_interface_t interface, IPAddress main_dns=IPAddress(), IPAddress backup_dns=IPAddress(), IPAddress fallback_dns=IPAddress());
esp_err_t set_esp_interface_ip(esp_interface_t interface, IPAddress local_ip=IPAddress(), IPAddress gateway=IPAddress(), IPAddress subnet=IPAddress());
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

static void wifi_sta_config(wifi_config_t * wifi_config, const char * ssid=NULL, const char * password=NULL, const uint8_t * bssid=NULL, uint8_t channel=0, wifi_scan_method_t scan_method=WIFI_ALL_CHANNEL_SCAN, wifi_sort_method_t sort_method=WIFI_CONNECT_AP_BY_SIGNAL, uint16_t listen_interval=0, bool pmf_required=false){
    wifi_config->sta.channel = channel;
    wifi_config->sta.listen_interval = listen_interval;
    wifi_config->sta.scan_method = scan_method;//WIFI_ALL_CHANNEL_SCAN or WIFI_FAST_SCAN
    wifi_config->sta.sort_method = sort_method;//WIFI_CONNECT_AP_BY_SIGNAL or WIFI_CONNECT_AP_BY_SECURITY
    wifi_config->sta.threshold.rssi = -75;
    wifi_config->sta.pmf_cfg.capable = true;
    wifi_config->sta.pmf_cfg.required = pmf_required;
    wifi_config->sta.bssid_set = 0;
    memset(wifi_config->sta.bssid, 0, 6);
    wifi_config->sta.threshold.authmode = WIFI_AUTH_OPEN;
    wifi_config->sta.ssid[0] = 0;
    wifi_config->sta.password[0] = 0;
    if(ssid != NULL && ssid[0] != 0){
    	snprintf((char*)wifi_config->sta.ssid, 32, ssid);
    	if(password != NULL && password[0] != 0){
    		wifi_config->sta.threshold.authmode = WIFI_AUTH_WEP;
    		if(strlen(password) == 64){
    			memcpy((char*)wifi_config->sta.password, password, 64);
    		} else {
    			snprintf((char*)wifi_config->sta.password, 64, password);
    		}
    	}
        if(bssid != NULL){
            wifi_config->sta.bssid_set = 1;
            memcpy(wifi_config->sta.bssid, bssid, 6);
        }
    }
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
    wifi_config_t current_conf;

    wifi_sta_config(&conf, ssid, passphrase, bssid, channel);

    if(esp_wifi_get_config(ESP_IF_WIFI_STA, &current_conf) != ESP_OK){
        log_e("get current config failed!");
        return WL_CONNECT_FAILED;
    }
    if(!sta_config_equal(current_conf, conf)) {
        if(esp_wifi_disconnect()){
            log_e("disconnect failed!");
            return WL_CONNECT_FAILED;
        }

        if(esp_wifi_set_config(ESP_IF_WIFI_STA, &conf) != ESP_OK){
            log_e("set config failed!");
            return WL_CONNECT_FAILED;
        }
    } else if(status() == WL_CONNECTED){
        return WL_CONNECTED;
    } else {
        if(esp_wifi_set_config(ESP_IF_WIFI_STA, &conf) != ESP_OK){
            log_e("set config failed!");
            return WL_CONNECT_FAILED;
        }
    }

    if(!_useStaticIp){
    	if(set_esp_interface_ip(ESP_IF_WIFI_STA) != ESP_OK) {
            return WL_CONNECT_FAILED;
        }
    }

    if(connect){
    	if(esp_wifi_connect() != ESP_OK) {
			log_e("connect failed!");
			return WL_CONNECT_FAILED;
		}
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

    wifi_config_t current_conf;
    if(esp_wifi_get_config(ESP_IF_WIFI_STA, &current_conf) != ESP_OK || esp_wifi_set_config(ESP_IF_WIFI_STA, &current_conf) != ESP_OK) {
        log_e("config failed");
        return WL_CONNECT_FAILED;
    }

    if(!_useStaticIp && set_esp_interface_ip(ESP_IF_WIFI_STA) != ESP_OK) {
        log_e("set ip failed!");
        return WL_CONNECT_FAILED;
    }

    if(status() != WL_CONNECTED){
    	esp_err_t err = esp_wifi_connect();
    	if(err){
            log_e("connect failed! 0x%x", err);
            return WL_CONNECT_FAILED;
    	}
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
    wifi_sta_config(&conf);

    if(WiFi.getMode() & WIFI_MODE_STA){
        if(eraseap){
            if(esp_wifi_set_config(ESP_IF_WIFI_STA, &conf)){
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
    err = set_esp_interface_ip(ESP_IF_WIFI_STA, local_ip, gateway, subnet);
    if(err == ESP_OK){
    	err = set_esp_interface_dns(ESP_IF_WIFI_STA, dns1, dns2);
    }
    _useStaticIp = err == ESP_OK;
    return err == ESP_OK;
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
    return false;//now deprecated
}

/**
 * Checks if ESP32 station mode will connect to AP
 * automatically or not when it is powered on.
 * @return auto connect
 */
bool WiFiSTAClass::getAutoConnect()
{
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
	esp_netif_ip_info_t ip;
    if(esp_netif_get_ip_info(get_esp_interface_netif(ESP_IF_WIFI_STA), &ip) != ESP_OK){
    	log_e("Netif Get IP Failed!");
    	return IPAddress();
    }
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
        esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
    }
    else{
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
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
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
    }
    else{
        esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
    }
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
	esp_netif_ip_info_t ip;
    if(esp_netif_get_ip_info(get_esp_interface_netif(ESP_IF_WIFI_STA), &ip) != ESP_OK){
    	log_e("Netif Get IP Failed!");
    	return IPAddress();
    }
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
	esp_netif_ip_info_t ip;
    if(esp_netif_get_ip_info(get_esp_interface_netif(ESP_IF_WIFI_STA), &ip) != ESP_OK){
    	log_e("Netif Get IP Failed!");
    	return IPAddress();
    }
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
    const ip_addr_t * dns_ip = dns_getserver(dns_no);
    return IPAddress(dns_ip->u_addr.ip4.addr);
}

/**
 * Get the broadcast ip address.
 * @return IPAddress broadcastIP
 */
IPAddress WiFiSTAClass::broadcastIP()
{
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return IPAddress();
    }
	esp_netif_ip_info_t ip;
    if(esp_netif_get_ip_info(get_esp_interface_netif(ESP_IF_WIFI_STA), &ip) != ESP_OK){
    	log_e("Netif Get IP Failed!");
    	return IPAddress();
    }
    return WiFiGenericClass::calculateBroadcast(IPAddress(ip.gw.addr), IPAddress(ip.netmask.addr));
}

/**
 * Get the network id.
 * @return IPAddress networkID
 */
IPAddress WiFiSTAClass::networkID()
{
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return IPAddress();
    }
	esp_netif_ip_info_t ip;
    if(esp_netif_get_ip_info(get_esp_interface_netif(ESP_IF_WIFI_STA), &ip) != ESP_OK){
    	log_e("Netif Get IP Failed!");
    	return IPAddress();
    }
    return WiFiGenericClass::calculateNetworkID(IPAddress(ip.gw.addr), IPAddress(ip.netmask.addr));
}

/**
 * Get the subnet CIDR.
 * @return uint8_t subnetCIDR
 */
uint8_t WiFiSTAClass::subnetCIDR()
{
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return (uint8_t)0;
    }
	esp_netif_ip_info_t ip;
    if(esp_netif_get_ip_info(get_esp_interface_netif(ESP_IF_WIFI_STA), &ip) != ESP_OK){
    	log_e("Netif Get IP Failed!");
    	return IPAddress();
    }
    return WiFiGenericClass::calculateSubnetCIDR(IPAddress(ip.netmask.addr));
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
    esp_wifi_get_config(ESP_IF_WIFI_STA, &conf);
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
 * Enable IPv6 on the station interface.
 * @return true on success
 */
bool WiFiSTAClass::enableIpV6()
{
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return false;
    }
    return esp_netif_create_ip6_linklocal(get_esp_interface_netif(ESP_IF_WIFI_STA)) == ESP_OK;
}

/**
 * Get the station interface IPv6 address.
 * @return IPv6Address
 */
IPv6Address WiFiSTAClass::localIPv6()
{
	esp_ip6_addr_t addr;
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return IPv6Address();
    }
    if(esp_netif_get_ip6_linklocal(get_esp_interface_netif(ESP_IF_WIFI_STA), &addr)) {
        return IPv6Address();
    }
    return IPv6Address(addr.addr);
}


bool WiFiSTAClass::_smartConfigStarted = false;
bool WiFiSTAClass::_smartConfigDone = false;


bool WiFiSTAClass::beginSmartConfig() {
    esp_err_t err;
    if (_smartConfigStarted) {
        return false;
    }

    if (!WiFi.mode(WIFI_STA)) {
        return false;
    }
    esp_wifi_disconnect();

    smartconfig_start_config_t conf = SMARTCONFIG_START_CONFIG_DEFAULT();
    err = esp_smartconfig_set_type(SC_TYPE_ESPTOUCH);
    if (err != ESP_OK) {
    	log_e("SmartConfig Set Type Failed!");
        return false;
    }
    err = esp_smartconfig_start(&conf);
    if (err != ESP_OK) {
    	log_e("SmartConfig Start Failed!");
        return false;
    }
    _smartConfigStarted = true;
    _smartConfigDone = false;
    return true;
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
