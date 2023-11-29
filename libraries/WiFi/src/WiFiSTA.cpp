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
#include "esp_mac.h"

#if __has_include ("esp_eap_client.h")
#include "esp_eap_client.h"
#else
#include "esp_wpa2.h"
#endif

// -----------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------- Private functions ------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

esp_netif_t* get_esp_interface_netif(esp_interface_t interface);
esp_err_t set_esp_interface_dns(esp_interface_t interface, IPAddress main_dns=IPAddress(), IPAddress backup_dns=IPAddress(), IPAddress fallback_dns=IPAddress());
esp_err_t set_esp_interface_ip(esp_interface_t interface, IPAddress local_ip=INADDR_NONE, IPAddress gateway=INADDR_NONE, IPAddress subnet=INADDR_NONE, IPAddress dhcp_lease_start=INADDR_NONE);
static bool sta_config_equal(const wifi_config_t& lhs, const wifi_config_t& rhs);

static size_t _wifi_strncpy(char * dst, const char * src, size_t dst_len){
    if(!dst || !src || !dst_len){
        return 0;
    }
    size_t src_len = strlen(src);
    if(src_len >= dst_len){
        src_len = dst_len;
    } else {
        src_len += 1;
    }
    memcpy(dst, src, src_len);
    return src_len;
}


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

static void wifi_sta_config(wifi_config_t * wifi_config, const char * ssid=NULL, const char * password=NULL, const uint8_t * bssid=NULL, uint8_t channel=0, wifi_auth_mode_t min_security=WIFI_AUTH_WPA2_PSK, wifi_scan_method_t scan_method=WIFI_ALL_CHANNEL_SCAN, wifi_sort_method_t sort_method=WIFI_CONNECT_AP_BY_SIGNAL, uint16_t listen_interval=0, bool pmf_required=false){
    wifi_config->sta.channel = channel;
    wifi_config->sta.listen_interval = listen_interval;
    wifi_config->sta.scan_method = scan_method;//WIFI_ALL_CHANNEL_SCAN or WIFI_FAST_SCAN
    wifi_config->sta.sort_method = sort_method;//WIFI_CONNECT_AP_BY_SIGNAL or WIFI_CONNECT_AP_BY_SECURITY
    wifi_config->sta.threshold.rssi = -127;
    wifi_config->sta.pmf_cfg.capable = true;
    wifi_config->sta.pmf_cfg.required = pmf_required;
    wifi_config->sta.bssid_set = 0;
    memset(wifi_config->sta.bssid, 0, 6);
    wifi_config->sta.threshold.authmode = WIFI_AUTH_OPEN;
    wifi_config->sta.ssid[0] = 0;
    wifi_config->sta.password[0] = 0;
    if(ssid != NULL && ssid[0] != 0){
        _wifi_strncpy((char*)wifi_config->sta.ssid, ssid, 32);
    	if(password != NULL && password[0] != 0){
    		wifi_config->sta.threshold.authmode = min_security;
    		_wifi_strncpy((char*)wifi_config->sta.password, password, 64);
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
wifi_auth_mode_t WiFiSTAClass::_minSecurity = WIFI_AUTH_WPA2_PSK;
wifi_scan_method_t WiFiSTAClass::_scanMethod = WIFI_FAST_SCAN;
wifi_sort_method_t WiFiSTAClass::_sortMethod = WIFI_CONNECT_AP_BY_SIGNAL;

static wl_status_t _sta_status = WL_STOPPED;
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
 * Start Wifi connection with a WPA2 Enterprise AP
 * if passphrase is set the most secure supported mode will be automatically selected
 * @param ssid const char*          Pointer to the SSID string.
 * @param method wpa2_method_t      The authentication method of WPA2 (WPA2_AUTH_TLS, WPA2_AUTH_PEAP, WPA2_AUTH_TTLS)
 * @param wpa2_identity  const char*          Pointer to the entity
 * @param wpa2_username  const char*          Pointer to the username
 * @param password const char *     Pointer to the password.
 * @param ca_pem const char*        Pointer to a string with the contents of a  .pem  file with CA cert
 * @param client_crt const char*        Pointer to a string with the contents of a .crt file with client cert
 * @param client_key const char*        Pointer to a string with the contants of a .key file with client key
 * @param bssid uint8_t[6]          Optional. BSSID / MAC of AP
 * @param channel                   Optional. Channel of AP
 * @param connect                   Optional. call connect
 * @return
 */
wl_status_t WiFiSTAClass::begin(const char* wpa2_ssid, wpa2_auth_method_t method, const char* wpa2_identity, const char* wpa2_username, const char *wpa2_password, const char* ca_pem, const char* client_crt, const char* client_key, int32_t channel, const uint8_t* bssid, bool connect)
{
    if(!WiFi.enableSTA(true)) {
        log_e("STA enable failed!");
        return WL_CONNECT_FAILED;
    }

    if(!wpa2_ssid || *wpa2_ssid == 0x00 || strlen(wpa2_ssid) > 32) {
        log_e("SSID too long or missing!");
        return WL_CONNECT_FAILED;
    }

    if(wpa2_identity && strlen(wpa2_identity) > 64) {
        log_e("identity too long!");
        return WL_CONNECT_FAILED;
    }

    if(wpa2_username && strlen(wpa2_username) > 64) {
        log_e("username too long!");
        return WL_CONNECT_FAILED;
    }

    if(wpa2_password && strlen(wpa2_password) > 64) {
        log_e("password too long!");
    }

    if(ca_pem) {
#if __has_include ("esp_eap_client.h")
        esp_eap_client_set_ca_cert((uint8_t *)ca_pem, strlen(ca_pem));
#else
        esp_wifi_sta_wpa2_ent_set_ca_cert((uint8_t *)ca_pem, strlen(ca_pem));
#endif
    }

    if(client_crt) {
#if __has_include ("esp_eap_client.h")
        esp_eap_client_set_certificate_and_key((uint8_t *)client_crt, strlen(client_crt), (uint8_t *)client_key, strlen(client_key), NULL, 0);
#else
        esp_wifi_sta_wpa2_ent_set_cert_key((uint8_t *)client_crt, strlen(client_crt), (uint8_t *)client_key, strlen(client_key), NULL, 0);
#endif
    }

#if __has_include ("esp_eap_client.h")
    esp_eap_client_set_identity((uint8_t *)wpa2_identity, strlen(wpa2_identity));
#else
    esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)wpa2_identity, strlen(wpa2_identity));
#endif
    if(method == WPA2_AUTH_PEAP || method == WPA2_AUTH_TTLS) {
#if __has_include ("esp_eap_client.h")
        esp_eap_client_set_username((uint8_t *)wpa2_username, strlen(wpa2_username));
        esp_eap_client_set_password((uint8_t *)wpa2_password, strlen(wpa2_password));
#else
        esp_wifi_sta_wpa2_ent_set_username((uint8_t *)wpa2_username, strlen(wpa2_username));
        esp_wifi_sta_wpa2_ent_set_password((uint8_t *)wpa2_password, strlen(wpa2_password));
#endif
    }
#if __has_include ("esp_eap_client.h")
    esp_wifi_sta_enterprise_enable(); //set config settings to enable function
#else
    esp_wifi_sta_wpa2_ent_enable(); //set config settings to enable function
#endif
    WiFi.begin(wpa2_ssid); //connect to wifi

    return status();
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

    if(!ssid || *ssid == 0x00 || strlen(ssid) > 32) {
        log_e("SSID too long or missing!");
        return WL_CONNECT_FAILED;
    }

    if(passphrase && strlen(passphrase) > 64) {
        log_e("passphrase too long!");
        return WL_CONNECT_FAILED;
    }

    wifi_config_t conf;
    memset(&conf, 0, sizeof(wifi_config_t));

    wifi_sta_config(&conf, ssid, passphrase, bssid, channel, _minSecurity, _scanMethod, _sortMethod);

    wifi_config_t current_conf;
    if(esp_wifi_get_config((wifi_interface_t)ESP_IF_WIFI_STA, &current_conf) != ESP_OK){
        log_e("get current config failed!");
        return WL_CONNECT_FAILED;
    }
    if(!sta_config_equal(current_conf, conf)) {
        if(esp_wifi_disconnect()){
            log_e("disconnect failed!");
            return WL_CONNECT_FAILED;
        }

        if(esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_STA, &conf) != ESP_OK){
            log_e("set config failed!");
            return WL_CONNECT_FAILED;
        }
    } else if(status() == WL_CONNECTED){
        return WL_CONNECTED;
    } else {
        if(esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_STA, &conf) != ESP_OK){
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
    if(esp_wifi_get_config((wifi_interface_t)ESP_IF_WIFI_STA, &current_conf) != ESP_OK || esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_STA, &current_conf) != ESP_OK) {
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
 * will force a disconnect and then start reconnecting to AP
 * @return true when successful
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
 * Disconnect from the network.
 * @param wifioff `true` to turn the Wi-Fi radio off.
 * @param eraseap `true` to erase the AP configuration from the NVS memory.
 * @return `true` when successful.
 */
bool WiFiSTAClass::disconnect(bool wifioff, bool eraseap)
{
    wifi_config_t conf;
    wifi_sta_config(&conf);

    if(WiFi.getMode() & WIFI_MODE_STA){
        _useStaticIp = false;
        if(eraseap){
            if(esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_STA, &conf)){
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
 * @brief  Reset WiFi settings in NVS to default values.
 * @return true if erase succeeded
 * @note: Resets SSID, password, protocol, mode, etc.
 * These settings are maintained by WiFi driver in IDF.
 * WiFi driver must be initialized.
 */
bool WiFiSTAClass::eraseAP(void) {
    if(WiFi.getMode()==WIFI_MODE_NULL) {
        if(!WiFi.enableSTA(true))
            return false;
    }

    return esp_wifi_restore()==ESP_OK;
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
 * Sets the working bandwidth of the STA mode
 * @param m wifi_bandwidth_t
 */
bool WiFiSTAClass::bandwidth(wifi_bandwidth_t bandwidth) {
    if(!WiFi.enableSTA(true)) {
        log_e("STA enable failed!");
        return false;
    }

    esp_err_t err;
    err = esp_wifi_set_bandwidth((wifi_interface_t)ESP_IF_WIFI_STA, bandwidth);
    if(err){
        log_e("Could not set STA bandwidth!");
        return false;
    }

    return true;
}

/**
 * Change DNS server for static IP configuration
 * @param dns1       Static DNS server 1
 * @param dns2       Static DNS server 2 (optional)
 */
bool WiFiSTAClass::setDNS(IPAddress dns1, IPAddress dns2)
{
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL)
        return false;
    esp_err_t err = set_esp_interface_dns(ESP_IF_WIFI_STA, dns1, dns2);
    return err == ESP_OK;
}

/**
 * is STA interface connected?
 * @return true if STA is connected to an AP
 */
bool WiFiSTAClass::isConnected()
{
    return (status() == WL_CONNECTED);
}

/**
 * Set the minimum security for AP to be considered connectable.
 * Must be called before WiFi.begin().
 * @param minSecurity wifi_auth_mode_t
 */
void WiFiSTAClass::setMinSecurity(wifi_auth_mode_t minSecurity)
{
    _minSecurity = minSecurity;
}

/**
 * Set the way that AP is chosen. 
 * First SSID match[WIFI_FAST_SCAN] or Sorted[WIFI_ALL_CHANNEL_SCAN] (RSSI or Security)
 * Must be called before WiFi.begin()
 * @param scanMethod wifi_scan_method_t
 */
void WiFiSTAClass::setScanMethod(wifi_scan_method_t scanMethod)
{
    _scanMethod = scanMethod;
}

/**
 * Set the way that AP is sorted. (requires scanMethod WIFI_ALL_CHANNEL_SCAN) 
 * By SSID[WIFI_CONNECT_AP_BY_SIGNAL] or Security[WIFI_CONNECT_AP_BY_SECURITY]
 * Must be called before WiFi.begin()
 * @param sortMethod wifi_sort_method_t
 */
void WiFiSTAClass::setSortMethod(wifi_sort_method_t sortMethod)
{
    _sortMethod = sortMethod;
}

/**
 * Deprecated. Setting the ESP32 station to connect to the AP (which is recorded)
 * automatically or not when powered on. Enable auto-connect by default.
 * @deprecated use `setAutoReconnect`
 * @param autoConnect bool
 * @return if saved
 */
bool WiFiSTAClass::setAutoConnect(bool autoConnect)
{
    return false;//now deprecated
}

/**
 * Deprecated. Checks if ESP32 station mode will connect to AP
 * automatically or not when it is powered on.
 * @deprecated use `getAutoReconnect`
 * @return auto connect
 */
bool WiFiSTAClass::getAutoConnect()
{
    return false;//now deprecated
}

/**
 * Function used to set the automatic reconnection if the connection is lost. 
 * @param autoReconnect `true` to enable this option.
 * @return true 
 */
bool WiFiSTAClass::setAutoReconnect(bool autoReconnect)
{
    _autoReconnect = autoReconnect;
    return true;
}
/**
 * Function used to get the automatic reconnection if the connection is lost.
 * @return The function will return `true` if this setting is enabled.
 */
bool WiFiSTAClass::getAutoReconnect()
{
    return _autoReconnect;
}

/**
 * Wait for WiFi connection to reach a result
 * returns the status reached or disconnect if STA is off
 * @return wl_status_t
 */
uint8_t WiFiSTAClass::waitForConnectResult(unsigned long timeoutLength)
{
    //1 and 3 have STA enabled
    if((WiFiGenericClass::getMode() & WIFI_MODE_STA) == 0) {
        return WL_DISCONNECTED;
    }
    unsigned long start = millis();
    while((!status() || status() >= WL_DISCONNECTED) && (millis() - start) < timeoutLength) {
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
        esp_wifi_get_mac((wifi_interface_t)ESP_IF_WIFI_STA, mac);
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
        esp_wifi_get_mac((wifi_interface_t)ESP_IF_WIFI_STA, mac);
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
    esp_wifi_get_config((wifi_interface_t)ESP_IF_WIFI_STA, &conf);
    return String(reinterpret_cast<char*>(conf.sta.password));
}

/**
 * Return the current bssid / mac associated with the network if configured
 * @return bssid uint8_t *
 */
uint8_t* WiFiSTAClass::BSSID(uint8_t* buff)
{
    static uint8_t bssid[6];
    wifi_ap_record_t info;
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return NULL;
    }
    esp_err_t err = esp_wifi_sta_get_ap_info(&info);
    if (buff != NULL) {
        if(err) {
          memset(buff, 0, 6);
        } else {
          memcpy(buff, info.bssid, 6);
        }
        return  buff;
    }
    if(!err) {
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

/**
 * @brief 
 * 
 * @param type Select type of SmartConfig. Default type is SC_TYPE_ESPTOUCH
 * @param crypt_key When using type SC_TYPE_ESPTOUTCH_V2 crypt key needed, else ignored. Lenght should be 16 chars.
 * @return true if configuration is successful.
 * @return false if configuration fails.
 */
bool WiFiSTAClass::beginSmartConfig(smartconfig_type_t type, char* crypt_key) {
    esp_err_t err;
    if (_smartConfigStarted) {
        return false;
    }

    if (!WiFi.mode(WIFI_STA)) {
        return false;
    }
    esp_wifi_disconnect();

    smartconfig_start_config_t conf = SMARTCONFIG_START_CONFIG_DEFAULT();

    if (type == SC_TYPE_ESPTOUCH_V2){
        conf.esp_touch_v2_enable_crypt = true;
        conf.esp_touch_v2_key = crypt_key;
    }

    err = esp_smartconfig_set_type(type);
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
