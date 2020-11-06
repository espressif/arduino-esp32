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
#include "WiFiAP.h"

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
#include <lwip/ip_addr.h>
#include "dhcpserver/dhcpserver_options.h"
}



// -----------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------- Private functions ------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

esp_netif_t* get_esp_interface_netif(esp_interface_t interface);
esp_err_t set_esp_interface_ip(esp_interface_t interface, IPAddress local_ip=IPAddress(), IPAddress gateway=IPAddress(), IPAddress subnet=IPAddress());
static bool softap_config_equal(const wifi_config_t& lhs, const wifi_config_t& rhs);



/**
 * compare two AP configurations
 * @param lhs softap_config
 * @param rhs softap_config
 * @return equal
 */
static bool softap_config_equal(const wifi_config_t& lhs, const wifi_config_t& rhs)
{
    if(strcmp(reinterpret_cast<const char*>(lhs.ap.ssid), reinterpret_cast<const char*>(rhs.ap.ssid)) != 0) {
        return false;
    }
    if(strcmp(reinterpret_cast<const char*>(lhs.ap.password), reinterpret_cast<const char*>(rhs.ap.password)) != 0) {
        return false;
    }
    if(lhs.ap.channel != rhs.ap.channel) {
        return false;
    }
    if(lhs.ap.ssid_hidden != rhs.ap.ssid_hidden) {
        return false;
    }
    if(lhs.ap.max_connection != rhs.ap.max_connection) {
        return false;
    }
    return true;
}

void wifi_softap_config(wifi_config_t *wifi_config, const char * ssid=NULL, const char * password=NULL, uint8_t channel=6, wifi_auth_mode_t authmode=WIFI_AUTH_WPA_WPA2_PSK, uint8_t ssid_hidden=0, uint8_t max_connections=4, uint16_t beacon_interval=100){
	wifi_config->ap.channel = channel;
	wifi_config->ap.max_connection = max_connections;
	wifi_config->ap.beacon_interval = beacon_interval;
	wifi_config->ap.ssid_hidden = ssid_hidden;
    wifi_config->ap.authmode = WIFI_AUTH_OPEN;
	wifi_config->ap.ssid_len = 0;
    wifi_config->ap.ssid[0] = 0;
    wifi_config->ap.password[0] = 0;
    if(ssid != NULL && ssid[0] != 0){
    	snprintf((char*)wifi_config->ap.ssid, 32, ssid);
    	wifi_config->ap.ssid_len = strlen(ssid);
    	if(password != NULL && password[0] != 0){
    		wifi_config->ap.authmode = authmode;
    	    snprintf((char*)wifi_config->ap.password, 64, password);
    	}
    }
}

// -----------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------------- AP function -----------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------


/**
 * Set up an access point
 * @param ssid              Pointer to the SSID (max 63 char).
 * @param passphrase        (for WPA2 min 8 char, for open use NULL)
 * @param channel           WiFi channel number, 1 - 13.
 * @param ssid_hidden       Network cloaking (0 = broadcast SSID, 1 = hide SSID)
 * @param max_connection    Max simultaneous connected clients, 1 - 4.
*/
bool WiFiAPClass::softAP(const char* ssid, const char* passphrase, int channel, int ssid_hidden, int max_connection)
{

    if(!WiFi.enableAP(true)) {
        // enable AP failed
        log_e("enable AP first!");
        return false;
    }

    if(!ssid || *ssid == 0) {
        // fail SSID missing
        log_e("SSID missing!");
        return false;
    }

    if(passphrase && (strlen(passphrase) > 0 && strlen(passphrase) < 8)) {
        // fail passphrase too short
        log_e("passphrase too short!");
        return false;
    }

    wifi_config_t conf;
    wifi_config_t conf_current;
    wifi_softap_config(&conf, ssid, passphrase, channel, WIFI_AUTH_WPA_WPA2_PSK, ssid_hidden, max_connection);
    esp_err_t err = esp_wifi_get_config(WIFI_IF_AP, &conf_current);
    if(err){
    	log_e("get AP config failed");
        return false;
    }
    if(!softap_config_equal(conf, conf_current)) {
    	err = esp_wifi_set_config(WIFI_IF_AP, &conf);
        if(err){
        	log_e("set AP config failed");
            return false;
        }
    }

    return true;
}


/**
 * Configure access point
 * @param local_ip      access point IP
 * @param gateway       gateway IP
 * @param subnet        subnet mask
 */
bool WiFiAPClass::softAPConfig(IPAddress local_ip, IPAddress gateway, IPAddress subnet)
{
    esp_err_t err = ESP_OK;

    if(!WiFi.enableAP(true)) {
        // enable AP failed
        return false;
    }

    err = set_esp_interface_ip(ESP_IF_WIFI_AP, local_ip, gateway, subnet);
    return err == ESP_OK;
}



/**
 * Disconnect from the network (close AP)
 * @param wifioff disable mode?
 * @return one value of wl_status_t enum
 */
bool WiFiAPClass::softAPdisconnect(bool wifioff)
{
    bool ret;
    wifi_config_t conf;
    wifi_softap_config(&conf);

    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return false;
    }

    ret = esp_wifi_set_config(WIFI_IF_AP, &conf) == ESP_OK;

    if(ret && wifioff) {
        ret = WiFi.enableAP(false) == ESP_OK;
    }

    return ret;
}


/**
 * Get the count of the Station / client that are connected to the softAP interface
 * @return Stations count
 */
uint8_t WiFiAPClass::softAPgetStationNum()
{
    wifi_sta_list_t clients;
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return 0;
    }
    if(esp_wifi_ap_get_sta_list(&clients) == ESP_OK) {
        return clients.num;
    }
    return 0;
}

/**
 * Get the softAP interface IP address.
 * @return IPAddress softAP IP
 */
IPAddress WiFiAPClass::softAPIP()
{
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return IPAddress();
    }
	esp_netif_ip_info_t ip;
    if(esp_netif_get_ip_info(get_esp_interface_netif(ESP_IF_WIFI_AP), &ip) != ESP_OK){
    	log_e("Netif Get IP Failed!");
    	return IPAddress();
    }
    return IPAddress(ip.ip.addr);
}

/**
 * Get the softAP broadcast IP address.
 * @return IPAddress softAP broadcastIP
 */
IPAddress WiFiAPClass::softAPBroadcastIP()
{
	esp_netif_ip_info_t ip;
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return IPAddress();
    }
    if(esp_netif_get_ip_info(get_esp_interface_netif(ESP_IF_WIFI_AP), &ip) != ESP_OK){
    	log_e("Netif Get IP Failed!");
    	return IPAddress();
    }
    return WiFiGenericClass::calculateBroadcast(IPAddress(ip.gw.addr), IPAddress(ip.netmask.addr));
}

/**
 * Get the softAP network ID.
 * @return IPAddress softAP networkID
 */
IPAddress WiFiAPClass::softAPNetworkID()
{
	esp_netif_ip_info_t ip;
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return IPAddress();
    }
    if(esp_netif_get_ip_info(get_esp_interface_netif(ESP_IF_WIFI_AP), &ip) != ESP_OK){
    	log_e("Netif Get IP Failed!");
    	return IPAddress();
    }
    return WiFiGenericClass::calculateNetworkID(IPAddress(ip.gw.addr), IPAddress(ip.netmask.addr));
}

/**
 * Get the softAP subnet CIDR.
 * @return uint8_t softAP subnetCIDR
 */
uint8_t WiFiAPClass::softAPSubnetCIDR()
{
	esp_netif_ip_info_t ip;
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return IPAddress();
    }
    if(esp_netif_get_ip_info(get_esp_interface_netif(ESP_IF_WIFI_AP), &ip) != ESP_OK){
    	log_e("Netif Get IP Failed!");
    	return IPAddress();
    }
    return WiFiGenericClass::calculateSubnetCIDR(IPAddress(ip.netmask.addr));
}

/**
 * Get the softAP interface MAC address.
 * @param mac   pointer to uint8_t array with length WL_MAC_ADDR_LENGTH
 * @return      pointer to uint8_t*
 */
uint8_t* WiFiAPClass::softAPmacAddress(uint8_t* mac)
{
    if(WiFiGenericClass::getMode() != WIFI_MODE_NULL){
        esp_wifi_get_mac(WIFI_IF_AP, mac);
    }
    return mac;
}

/**
 * Get the softAP interface MAC address.
 * @return String mac
 */
String WiFiAPClass::softAPmacAddress(void)
{
    uint8_t mac[6];
    char macStr[18] = { 0 };
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return String();
    }
    esp_wifi_get_mac(WIFI_IF_AP, mac);

    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
}

/**
 * Get the softAP interface Host name.
 * @return char array hostname
 */
const char * WiFiAPClass::softAPgetHostname()
{
    const char * hostname = NULL;
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return hostname;
    }
    if(esp_netif_get_hostname(get_esp_interface_netif(ESP_IF_WIFI_AP), &hostname) != ESP_OK){
    	log_e("Netif Get Hostname Failed!");
    }
    return hostname;
}

/**
 * Set the softAP    interface Host name.
 * @param  hostname  pointer to const string
 * @return true on   success
 */
bool WiFiAPClass::softAPsetHostname(const char * hostname)
{
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return false;
    }
    return esp_netif_set_hostname(get_esp_interface_netif(ESP_IF_WIFI_AP), hostname) == ESP_OK;
}

/**
 * Enable IPv6 on the softAP interface.
 * @return true on success
 */
bool WiFiAPClass::softAPenableIpV6()
{
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return false;
    }
    return esp_netif_create_ip6_linklocal(get_esp_interface_netif(ESP_IF_WIFI_AP)) == ESP_OK;
}

/**
 * Get the softAP interface IPv6 address.
 * @return IPv6Address softAP IPv6
 */
IPv6Address WiFiAPClass::softAPIPv6()
{
	esp_ip6_addr_t addr;
    if(WiFiGenericClass::getMode() == WIFI_MODE_NULL){
        return IPv6Address();
    }
    if(esp_netif_get_ip6_linklocal(get_esp_interface_netif(ESP_IF_WIFI_AP), &addr)) {
        return IPv6Address();
    }
    return IPv6Address(addr.addr);
}
