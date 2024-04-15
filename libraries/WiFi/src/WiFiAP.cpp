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
#if SOC_WIFI_SUPPORTED

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

/**
 * Set up an access point
 * @param ssid              Pointer to the SSID (max 63 char).
 * @param passphrase        (for WPA2 min 8 char, for open use NULL)
 * @param channel           WiFi channel number, 1 - 13.
 * @param ssid_hidden       Network cloaking (0 = broadcast SSID, 1 = hide SSID)
 * @param max_connection    Max simultaneous connected clients, 1 - 4.
*/
bool WiFiAPClass::softAP(const char* ssid, const char* passphrase, int channel, int ssid_hidden, int max_connection, bool ftm_responder, wifi_auth_mode_t auth_mode, wifi_cipher_type_t cipher) {
  return AP.begin() && AP.create(ssid, passphrase, channel, ssid_hidden, max_connection, ftm_responder, auth_mode, cipher);
}

/**
 * Return the current SSID associated with the network
 * @return SSID
 */
String WiFiAPClass::softAPSSID() const {
  return AP.SSID();
}

/**
 * Configure access point
 * @param local_ip      access point IP
 * @param gateway       gateway IP
 * @param subnet        subnet mask
 */
bool WiFiAPClass::softAPConfig(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dhcp_lease_start, IPAddress dns) {
  return AP.begin() && AP.config(local_ip, gateway, subnet, dhcp_lease_start, dns);
}

/**
 * Disconnect from the network (close AP)
 * @param wifioff disable mode?
 * @return one value of wl_status_t enum
 */
bool WiFiAPClass::softAPdisconnect(bool wifioff) {
  if (!AP.clear()) {
    return false;
  }
  if (wifioff) {
    return AP.end();
  }
  return true;
}

/**
 * Sets the working bandwidth of the AP mode
 * @param m wifi_bandwidth_t
 */
bool WiFiAPClass::softAPbandwidth(wifi_bandwidth_t bandwidth) {
  return AP.bandwidth(bandwidth);
}

/**
 * Get the count of the Station / client that are connected to the softAP interface
 * @return Stations count
 */
uint8_t WiFiAPClass::softAPgetStationNum() {
  return AP.stationCount();
}

/**
 * Get the softAP interface IP address.
 * @return IPAddress softAP IP
 */
IPAddress WiFiAPClass::softAPIP() {
  return AP.localIP();
}

/**
 * Get the softAP broadcast IP address.
 * @return IPAddress softAP broadcastIP
 */
IPAddress WiFiAPClass::softAPBroadcastIP() {
  return AP.broadcastIP();
}

/**
 * Get the softAP network ID.
 * @return IPAddress softAP networkID
 */
IPAddress WiFiAPClass::softAPNetworkID() {
  return AP.networkID();
}

/**
 * Get the softAP subnet mask.
 * @return IPAddress subnetMask
 */
IPAddress WiFiAPClass::softAPSubnetMask() {
  return AP.subnetMask();
}

/**
 * Get the softAP subnet CIDR.
 * @return uint8_t softAP subnetCIDR
 */
uint8_t WiFiAPClass::softAPSubnetCIDR() {
  return AP.subnetCIDR();
}

/**
 * Get the softAP interface MAC address.
 * @param mac   pointer to uint8_t array with length WL_MAC_ADDR_LENGTH
 * @return      pointer to uint8_t*
 */
uint8_t* WiFiAPClass::softAPmacAddress(uint8_t* mac) {
  return AP.macAddress(mac);
}

/**
 * Get the softAP interface MAC address.
 * @return String mac
 */
String WiFiAPClass::softAPmacAddress(void) {
  return AP.macAddress();
}

/**
 * Get the softAP interface Host name.
 * @return char array hostname
 */
const char* WiFiAPClass::softAPgetHostname() {
  return AP.getHostname();
}

/**
 * Set the softAP    interface Host name.
 * @param  hostname  pointer to const string
 * @return true on   success
 */
bool WiFiAPClass::softAPsetHostname(const char* hostname) {
  return AP.setHostname(hostname);
}

/**
 * Enable IPv6 on the softAP interface.
 * @return true on success
 */
bool WiFiAPClass::softAPenableIPv6(bool enable) {
  return AP.enableIPv6(enable);
}

/**
 * Get the softAP interface IPv6 address.
 * @return IPAddress softAP IPv6
 */

IPAddress WiFiAPClass::softAPlinkLocalIPv6() {
  return AP.linkLocalIPv6();
}

#endif /* SOC_WIFI_SUPPORTED */
