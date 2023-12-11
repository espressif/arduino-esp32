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
#include <esp_event.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include "lwip/ip_addr.h"
#include "lwip/opt.h"
#include "lwip/err.h"
#include "lwip/dns.h"
#include "dhcpserver/dhcpserver.h"
#include "dhcpserver/dhcpserver_options.h"

} //extern "C"

#include "esp32-hal.h"
#include <vector>
#include "sdkconfig.h"

#define _byte_swap32(num) (((num>>24)&0xff) | ((num<<8)&0xff0000) | ((num>>8)&0xff00) | ((num<<24)&0xff000000))
ESP_EVENT_DEFINE_BASE(ARDUINO_EVENTS);
/*
 * Private (exposable) methods
 * */
static esp_netif_t* esp_netifs[ESP_IF_MAX] = {NULL, NULL, NULL};
esp_interface_t get_esp_netif_interface(esp_netif_t* esp_netif){
	for(int i=0; i<ESP_IF_MAX; i++){
		if(esp_netifs[i] != NULL && esp_netifs[i] == esp_netif){
			return (esp_interface_t)i;
		}
	}
	return ESP_IF_MAX;
}

void add_esp_interface_netif(esp_interface_t interface, esp_netif_t* esp_netif){
	if(interface < ESP_IF_MAX){
		esp_netifs[interface] = esp_netif;
	}
}

esp_netif_t* get_esp_interface_netif(esp_interface_t interface){
	if(interface < ESP_IF_MAX){
		return esp_netifs[interface];
	}
	return NULL;
}

esp_err_t set_esp_interface_hostname(esp_interface_t interface, const char * hostname){
	if(interface < ESP_IF_MAX){
		return esp_netif_set_hostname(esp_netifs[interface], hostname);
	}
	return ESP_FAIL;
}

esp_err_t set_esp_interface_ip(esp_interface_t interface, IPAddress local_ip=IPAddress(), IPAddress gateway=IPAddress(), IPAddress subnet=IPAddress(), IPAddress dhcp_lease_start=INADDR_NONE){
	esp_netif_t *esp_netif = esp_netifs[interface];
	esp_netif_dhcp_status_t status = ESP_NETIF_DHCP_INIT;
	esp_netif_ip_info_t info;
    info.ip.addr = static_cast<uint32_t>(local_ip);
    info.gw.addr = static_cast<uint32_t>(gateway);
    info.netmask.addr = static_cast<uint32_t>(subnet);

    log_v("Configuring %s static IP: " IPSTR ", MASK: " IPSTR ", GW: " IPSTR,
          interface == ESP_IF_WIFI_STA ? "Station" :
          interface == ESP_IF_WIFI_AP ? "SoftAP" : "Ethernet",
          IP2STR(&info.ip), IP2STR(&info.netmask), IP2STR(&info.gw));

    esp_err_t err = ESP_OK;
    if(interface != ESP_IF_WIFI_AP){
    	err = esp_netif_dhcpc_get_status(esp_netif, &status);
        if(err){
        	log_e("DHCPC Get Status Failed! 0x%04x", err);
        	return err;
        }
		err = esp_netif_dhcpc_stop(esp_netif);
		if(err && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED){
			log_e("DHCPC Stop Failed! 0x%04x", err);
			return err;
		}
        err = esp_netif_set_ip_info(esp_netif, &info);
        if(err){
        	log_e("Netif Set IP Failed! 0x%04x", err);
        	return err;
        }
    	if(info.ip.addr == 0){
    		err = esp_netif_dhcpc_start(esp_netif);
    		if(err){
            	log_e("DHCPC Start Failed! 0x%04x", err);
            	return err;
            }
    	}
    } else {
    	err = esp_netif_dhcps_get_status(esp_netif, &status);
        if(err){
        	log_e("DHCPS Get Status Failed! 0x%04x", err);
        	return err;
        }
		err = esp_netif_dhcps_stop(esp_netif);
		if(err && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED){
			log_e("DHCPS Stop Failed! 0x%04x", err);
			return err;
		}
        err = esp_netif_set_ip_info(esp_netif, &info);
        if(err){
        	log_e("Netif Set IP Failed! 0x%04x", err);
        	return err;
        }

        dhcps_lease_t lease;
        lease.enable = true;
        uint8_t CIDR = WiFiGenericClass::calculateSubnetCIDR(subnet);
        log_v("SoftAP: %s | Gateway: %s | DHCP Start: %s | Netmask: %s", local_ip.toString().c_str(), gateway.toString().c_str(), dhcp_lease_start.toString().c_str(), subnet.toString().c_str());
        // netmask must have room for at least 12 IP addresses (AP + GW + 10 DHCP Leasing addresses)
        // netmask also must be limited to the last 8 bits of IPv4, otherwise this function won't work
        // IDF NETIF checks netmask for the 3rd byte: https://github.com/espressif/esp-idf/blob/master/components/esp_netif/lwip/esp_netif_lwip.c#L1857-L1862
        if (CIDR > 28 || CIDR < 24) {
            log_e("Bad netmask. It must be from /24 to /28 (255.255.255. 0<->240)");
            return ESP_FAIL; //  ESP_FAIL if initializing failed
        }
        // The code below is ready for any netmask, not limited to 255.255.255.0
        uint32_t netmask = _byte_swap32(info.netmask.addr);
        uint32_t ap_ipaddr = _byte_swap32(info.ip.addr);
        uint32_t dhcp_ipaddr = _byte_swap32(static_cast<uint32_t>(dhcp_lease_start));
        dhcp_ipaddr = dhcp_ipaddr == 0 ? ap_ipaddr + 1 : dhcp_ipaddr;
        uint32_t leaseStartMax = ~netmask - 10;
        // there will be 10 addresses for DHCP to lease
        lease.start_ip.addr = dhcp_ipaddr;
        lease.end_ip.addr = lease.start_ip.addr + 10;
        // Check if local_ip is in the same subnet as the dhcp leasing range initial address
        if ((ap_ipaddr & netmask) != (dhcp_ipaddr & netmask)) {
            log_e("The AP IP address (%s) and the DHCP start address (%s) must be in the same subnet", 
                local_ip.toString().c_str(), IPAddress(_byte_swap32(dhcp_ipaddr)).toString().c_str());
            return ESP_FAIL; //  ESP_FAIL if initializing failed
        }
        // prevents DHCP lease range to overflow subnet range
        if ((dhcp_ipaddr & ~netmask) >= leaseStartMax) {
            // make first DHCP lease addr stay in the begining of the netmask range
            lease.start_ip.addr = (dhcp_ipaddr & netmask) + 1;
            lease.end_ip.addr = lease.start_ip.addr + 10;
            log_w("DHCP Lease out of range - Changing DHCP leasing start to %s", IPAddress(_byte_swap32(lease.start_ip.addr)).toString().c_str());
        }
        // Check if local_ip is within DHCP range
        if (ap_ipaddr >= lease.start_ip.addr && ap_ipaddr <= lease.end_ip.addr) {
            log_e("The AP IP address (%s) can't be within the DHCP range (%s -- %s)", 
                local_ip.toString().c_str(), IPAddress(_byte_swap32(lease.start_ip.addr)).toString().c_str(), IPAddress(_byte_swap32(lease.end_ip.addr)).toString().c_str());
            return ESP_FAIL; //  ESP_FAIL if initializing failed
        }
        // Check if gateway is within DHCP range
        uint32_t gw_ipaddr = _byte_swap32(info.gw.addr);
        bool gw_in_same_subnet = (gw_ipaddr & netmask) == (ap_ipaddr & netmask);
        if (gw_in_same_subnet && gw_ipaddr >= lease.start_ip.addr && gw_ipaddr <= lease.end_ip.addr) {
            log_e("The GatewayP address (%s) can't be within the DHCP range (%s -- %s)", 
                gateway.toString().c_str(), IPAddress(_byte_swap32(lease.start_ip.addr)).toString().c_str(), IPAddress(_byte_swap32(lease.end_ip.addr)).toString().c_str());
            return ESP_FAIL; //  ESP_FAIL if initializing failed
        }
        // all done, just revert back byte order of DHCP lease range
        lease.start_ip.addr = _byte_swap32(lease.start_ip.addr);
        lease.end_ip.addr = _byte_swap32(lease.end_ip.addr);
        log_v("DHCP Server Range: %s to %s", IPAddress(lease.start_ip.addr).toString().c_str(), IPAddress(lease.end_ip.addr).toString().c_str());
        err = esp_netif_dhcps_option(
            esp_netif,
            ESP_NETIF_OP_SET,
            ESP_NETIF_SUBNET_MASK,
            (void*)&info.netmask.addr, sizeof(info.netmask.addr)
        );
		if(err){
        	log_e("DHCPS Set Netmask Failed! 0x%04x", err);
        	return err;
        }
        err = esp_netif_dhcps_option(
            esp_netif,
            ESP_NETIF_OP_SET,
            ESP_NETIF_REQUESTED_IP_ADDRESS,
            (void*)&lease, sizeof(dhcps_lease_t)
        );
		if(err){
        	log_e("DHCPS Set Lease Failed! 0x%04x", err);
        	return err;
        }
		err = esp_netif_dhcps_start(esp_netif);
		if(err){
        	log_e("DHCPS Start Failed! 0x%04x", err);
        	return err;
        }
    }
	return err;
}

esp_err_t set_esp_interface_dns(esp_interface_t interface, IPAddress main_dns=IPAddress(), IPAddress backup_dns=IPAddress(), IPAddress fallback_dns=IPAddress()){
	esp_netif_t *esp_netif = esp_netifs[interface];
	esp_netif_dns_info_t dns;
	dns.ip.type = ESP_IPADDR_TYPE_V4;
	dns.ip.u_addr.ip4.addr = static_cast<uint32_t>(main_dns);
	if(dns.ip.u_addr.ip4.addr && esp_netif_set_dns_info(esp_netif, ESP_NETIF_DNS_MAIN, &dns) != ESP_OK){
    	log_e("Set Main DNS Failed!");
    	return ESP_FAIL;
    }
	if(interface != ESP_IF_WIFI_AP){
		dns.ip.u_addr.ip4.addr = static_cast<uint32_t>(backup_dns);
		if(dns.ip.u_addr.ip4.addr && esp_netif_set_dns_info(esp_netif, ESP_NETIF_DNS_BACKUP, &dns) != ESP_OK){
	    	log_e("Set Backup DNS Failed!");
	    	return ESP_FAIL;
	    }
		dns.ip.u_addr.ip4.addr = static_cast<uint32_t>(fallback_dns);
		if(dns.ip.u_addr.ip4.addr && esp_netif_set_dns_info(esp_netif, ESP_NETIF_DNS_FALLBACK, &dns) != ESP_OK){
	    	log_e("Set Fallback DNS Failed!");
	    	return ESP_FAIL;
	    }
	}
	return ESP_OK;
}

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
static const char * auth_mode_str(int authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:
    	return ("OPEN");
        break;
    case WIFI_AUTH_WEP:
    	return ("WEP");
        break;
    case WIFI_AUTH_WPA_PSK:
    	return ("WPA_PSK");
        break;
    case WIFI_AUTH_WPA2_PSK:
    	return ("WPA2_PSK");
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
    	return ("WPA_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
    	return ("WPA2_ENTERPRISE");
        break;
    case WIFI_AUTH_WPA3_PSK:
    	return ("WPA3_PSK");
        break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
    	return ("WPA2_WPA3_PSK");
        break;
    case WIFI_AUTH_WAPI_PSK:
    	return ("WPAPI_PSK");
        break;
    default:
        break;
    }
	return ("UNKNOWN");
}
#endif

static char default_hostname[32] = {0,};
static const char * get_esp_netif_hostname(){
	if(default_hostname[0] == 0){
	    uint8_t eth_mac[6];
	    esp_wifi_get_mac((wifi_interface_t)WIFI_IF_STA, eth_mac);
	    snprintf(default_hostname, 32, "%s%02X%02X%02X", CONFIG_IDF_TARGET "-", eth_mac[3], eth_mac[4], eth_mac[5]);
	}
	return (const char *)default_hostname;
}
static void set_esp_netif_hostname(const char * name){
	if(name){
		snprintf(default_hostname, 32, "%s", name);
	}
}

static QueueHandle_t _arduino_event_queue;
static TaskHandle_t _arduino_event_task_handle = NULL;
static EventGroupHandle_t _arduino_event_group = NULL;

static void _arduino_event_task(void * arg){
	arduino_event_t *data = NULL;
    for (;;) {
        if(xQueueReceive(_arduino_event_queue, &data, portMAX_DELAY) == pdTRUE){
            WiFiGenericClass::_eventCallback(data);
            free(data);
            data = NULL;
        }
    }
    vTaskDelete(NULL);
    _arduino_event_task_handle = NULL;
}

esp_err_t postArduinoEvent(arduino_event_t *data)
{
	if(data == NULL){
        return ESP_FAIL;
	}
	arduino_event_t * event = (arduino_event_t*)malloc(sizeof(arduino_event_t));
	if(event == NULL){
        log_e("Arduino Event Malloc Failed!");
        return ESP_FAIL;
	}
	memcpy(event, data, sizeof(arduino_event_t));
    if (xQueueSend(_arduino_event_queue, &event, portMAX_DELAY) != pdPASS) {
        log_e("Arduino Event Send Failed!");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void _arduino_event_cb(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	arduino_event_t arduino_event;
	arduino_event.event_id = ARDUINO_EVENT_MAX;

	/*
	 * STA
	 * */
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    	log_v("STA Started");
    	arduino_event.event_id = ARDUINO_EVENT_WIFI_STA_START;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_STOP) {
    	log_v("STA Stopped");
    	arduino_event.event_id = ARDUINO_EVENT_WIFI_STA_STOP;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_AUTHMODE_CHANGE) {
    	#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
            wifi_event_sta_authmode_change_t * event = (wifi_event_sta_authmode_change_t*)event_data;
    	    log_v("STA Auth Mode Changed: From: %s, To: %s", auth_mode_str(event->old_mode), auth_mode_str(event->new_mode));
        #endif
    	arduino_event.event_id = ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE;
    	memcpy(&arduino_event.event_info.wifi_sta_authmode_change, event_data, sizeof(wifi_event_sta_authmode_change_t));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
    	#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
            wifi_event_sta_connected_t * event = (wifi_event_sta_connected_t*)event_data;
    	    log_v("STA Connected: SSID: %s, BSSID: " MACSTR ", Channel: %u, Auth: %s", event->ssid, MAC2STR(event->bssid), event->channel, auth_mode_str(event->authmode));
        #endif
    	arduino_event.event_id = ARDUINO_EVENT_WIFI_STA_CONNECTED;
    	memcpy(&arduino_event.event_info.wifi_sta_connected, event_data, sizeof(wifi_event_sta_connected_t));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    	#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
            wifi_event_sta_disconnected_t * event = (wifi_event_sta_disconnected_t*)event_data;
    	    log_v("STA Disconnected: SSID: %s, BSSID: " MACSTR ", Reason: %u", event->ssid, MAC2STR(event->bssid), event->reason);
        #endif
    	arduino_event.event_id = ARDUINO_EVENT_WIFI_STA_DISCONNECTED;
    	memcpy(&arduino_event.event_info.wifi_sta_disconnected, event_data, sizeof(wifi_event_sta_disconnected_t));
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        #if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            log_v("STA Got %sIP:" IPSTR, event->ip_changed?"New ":"Same ", IP2STR(&event->ip_info.ip));
    	#endif
        arduino_event.event_id = ARDUINO_EVENT_WIFI_STA_GOT_IP;
    	memcpy(&arduino_event.event_info.got_ip, event_data, sizeof(ip_event_got_ip_t));
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP) {
    	log_v("STA IP Lost");
    	arduino_event.event_id = ARDUINO_EVENT_WIFI_STA_LOST_IP;

	/*
	 * SCAN
	 * */
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        #if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
    	   wifi_event_sta_scan_done_t * event = (wifi_event_sta_scan_done_t*)event_data;
    	   log_v("SCAN Done: ID: %u, Status: %u, Results: %u", event->scan_id, event->status, event->number);
        #endif
    	arduino_event.event_id = ARDUINO_EVENT_WIFI_SCAN_DONE;
    	memcpy(&arduino_event.event_info.wifi_scan_done, event_data, sizeof(wifi_event_sta_scan_done_t));

	/*
	 * AP
	 * */
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
		log_v("AP Started");
    	arduino_event.event_id = ARDUINO_EVENT_WIFI_AP_START;
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STOP) {
		log_v("AP Stopped");
    	arduino_event.event_id = ARDUINO_EVENT_WIFI_AP_STOP;
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_PROBEREQRECVED) {
		#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
            wifi_event_ap_probe_req_rx_t * event = (wifi_event_ap_probe_req_rx_t*)event_data;
		    log_v("AP Probe Request: RSSI: %d, MAC: " MACSTR, event->rssi, MAC2STR(event->mac));
    	#endif
        arduino_event.event_id = ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED;
    	memcpy(&arduino_event.event_info.wifi_ap_probereqrecved, event_data, sizeof(wifi_event_ap_probe_req_rx_t));
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
		#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
            wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
		    log_v("AP Station Connected: MAC: " MACSTR ", AID: %d", MAC2STR(event->mac), event->aid);
    	#endif
        arduino_event.event_id = ARDUINO_EVENT_WIFI_AP_STACONNECTED;
    	memcpy(&arduino_event.event_info.wifi_ap_staconnected, event_data, sizeof(wifi_event_ap_staconnected_t));
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
		#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
            wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
		    log_v("AP Station Disconnected: MAC: " MACSTR ", AID: %d", MAC2STR(event->mac), event->aid);
        #endif
    	arduino_event.event_id = ARDUINO_EVENT_WIFI_AP_STADISCONNECTED;
    	memcpy(&arduino_event.event_info.wifi_ap_stadisconnected, event_data, sizeof(wifi_event_ap_stadisconnected_t));
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_AP_STAIPASSIGNED) {
    	#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
           ip_event_ap_staipassigned_t * event = (ip_event_ap_staipassigned_t*)event_data;
    	   log_v("AP Station IP Assigned:" IPSTR, IP2STR(&event->ip));
    	#endif
        arduino_event.event_id = ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED;
    	memcpy(&arduino_event.event_info.wifi_ap_staipassigned, event_data, sizeof(ip_event_ap_staipassigned_t));

	/*
	 * ETH
	 * */
	} else if (event_base == ETH_EVENT && event_id == ETHERNET_EVENT_CONNECTED) {
		log_v("Ethernet Link Up");
    	arduino_event.event_id = ARDUINO_EVENT_ETH_CONNECTED;
    	memcpy(&arduino_event.event_info.eth_connected, event_data, sizeof(esp_eth_handle_t));
	} else if (event_base == ETH_EVENT && event_id == ETHERNET_EVENT_DISCONNECTED) {
		log_v("Ethernet Link Down");
    	arduino_event.event_id = ARDUINO_EVENT_ETH_DISCONNECTED;
	} else if (event_base == ETH_EVENT && event_id == ETHERNET_EVENT_START) {
		log_v("Ethernet Started");
    	arduino_event.event_id = ARDUINO_EVENT_ETH_START;
	} else if (event_base == ETH_EVENT && event_id == ETHERNET_EVENT_STOP) {
		log_v("Ethernet Stopped");
    	arduino_event.event_id = ARDUINO_EVENT_ETH_STOP;
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_GOT_IP) {
        #if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            log_v("Ethernet got %sip:" IPSTR, event->ip_changed?"new ":"", IP2STR(&event->ip_info.ip));
    	#endif
        arduino_event.event_id = ARDUINO_EVENT_ETH_GOT_IP;
    	memcpy(&arduino_event.event_info.got_ip, event_data, sizeof(ip_event_got_ip_t));
    } else if (event_base == ETH_EVENT && event_id == IP_EVENT_ETH_LOST_IP) {
        log_v("Ethernet Lost IP");
        arduino_event.event_id = ARDUINO_EVENT_ETH_LOST_IP;

	/*
	 * IPv6
	 * */
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_GOT_IP6) {
    	ip_event_got_ip6_t * event = (ip_event_got_ip6_t*)event_data;
    	esp_interface_t iface = get_esp_netif_interface(event->esp_netif);
    	log_v("IF[%d] Got IPv6: IP Index: %d, Zone: %d, " IPV6STR, iface, event->ip_index, event->ip6_info.ip.zone, IPV62STR(event->ip6_info.ip));
    	memcpy(&arduino_event.event_info.got_ip6, event_data, sizeof(ip_event_got_ip6_t));
    	if(iface == ESP_IF_WIFI_STA){
        	arduino_event.event_id = ARDUINO_EVENT_WIFI_STA_GOT_IP6;
    	} else if(iface == ESP_IF_WIFI_AP){
        	arduino_event.event_id = ARDUINO_EVENT_WIFI_AP_GOT_IP6;
    	} else if(iface == ESP_IF_ETH){
        	arduino_event.event_id = ARDUINO_EVENT_ETH_GOT_IP6;
    	}

	/*
	 * WPS
	 * */
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_WPS_ER_SUCCESS) {
    	arduino_event.event_id = ARDUINO_EVENT_WPS_ER_SUCCESS;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_WPS_ER_FAILED) {
    	arduino_event.event_id = ARDUINO_EVENT_WPS_ER_FAILED;
    	memcpy(&arduino_event.event_info.wps_fail_reason, event_data, sizeof(wifi_event_sta_wps_fail_reason_t));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_WPS_ER_TIMEOUT) {
    	arduino_event.event_id = ARDUINO_EVENT_WPS_ER_TIMEOUT;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_WPS_ER_PIN) {
    	arduino_event.event_id = ARDUINO_EVENT_WPS_ER_PIN;
    	memcpy(&arduino_event.event_info.wps_er_pin, event_data, sizeof(wifi_event_sta_wps_er_pin_t));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP) {
    	arduino_event.event_id = ARDUINO_EVENT_WPS_ER_PBC_OVERLAP;

	/*
	 * FTM
	 * */
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_FTM_REPORT) {
    	arduino_event.event_id = ARDUINO_EVENT_WIFI_FTM_REPORT;
    	memcpy(&arduino_event.event_info.wifi_ftm_report, event_data, sizeof(wifi_event_ftm_report_t));


	/*
	 * SMART CONFIG
	 * */
	} else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
		log_v("SC Scan Done");
    	arduino_event.event_id = ARDUINO_EVENT_SC_SCAN_DONE;
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
    	log_v("SC Found Channel");
    	arduino_event.event_id = ARDUINO_EVENT_SC_FOUND_CHANNEL;
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        #if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
            smartconfig_event_got_ssid_pswd_t *event = (smartconfig_event_got_ssid_pswd_t *)event_data;
            log_v("SC: SSID: %s, Password: %s", (const char *)event->ssid, (const char *)event->password);
        #endif
    	arduino_event.event_id = ARDUINO_EVENT_SC_GOT_SSID_PSWD;
    	memcpy(&arduino_event.event_info.sc_got_ssid_pswd, event_data, sizeof(smartconfig_event_got_ssid_pswd_t));

    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
    	log_v("SC Send Ack Done");
    	arduino_event.event_id = ARDUINO_EVENT_SC_SEND_ACK_DONE;

	/*
	 * Provisioning
	 * */
	} else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_INIT) {
		log_v("Provisioning Initialized!");
    	arduino_event.event_id = ARDUINO_EVENT_PROV_INIT;
	} else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_DEINIT) {
		log_v("Provisioning Uninitialized!");
    	arduino_event.event_id = ARDUINO_EVENT_PROV_DEINIT;
	} else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_START) {
		log_v("Provisioning Start!");
    	arduino_event.event_id = ARDUINO_EVENT_PROV_START;
	} else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_END) {
		log_v("Provisioning End!");
		wifi_prov_mgr_deinit();
    	arduino_event.event_id = ARDUINO_EVENT_PROV_END;
	} else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_CRED_RECV) {
        #if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
            wifi_sta_config_t *event = (wifi_sta_config_t *)event_data;
            log_v("Provisioned Credentials: SSID: %s, Password: %s", (const char *) event->ssid, (const char *) event->password);
        #endif
    	arduino_event.event_id = ARDUINO_EVENT_PROV_CRED_RECV;
    	memcpy(&arduino_event.event_info.prov_cred_recv, event_data, sizeof(wifi_sta_config_t));
	} else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_CRED_FAIL) {
        #if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_ERROR
            wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
            log_e("Provisioning Failed: Reason : %s", (*reason == WIFI_PROV_STA_AUTH_ERROR)?"Authentication Failed":"AP Not Found");
        #endif
    	arduino_event.event_id = ARDUINO_EVENT_PROV_CRED_FAIL;
    	memcpy(&arduino_event.event_info.prov_fail_reason, event_data, sizeof(wifi_prov_sta_fail_reason_t));
	} else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_CRED_SUCCESS) {
		log_v("Provisioning Success!");
    	arduino_event.event_id = ARDUINO_EVENT_PROV_CRED_SUCCESS;
    }
    
	if(arduino_event.event_id < ARDUINO_EVENT_MAX){
		postArduinoEvent(&arduino_event);
	}
}

static bool _start_network_event_task(){
    if(!_arduino_event_group){
        _arduino_event_group = xEventGroupCreate();
        if(!_arduino_event_group){
            log_e("Network Event Group Create Failed!");
            return false;
        }
        xEventGroupSetBits(_arduino_event_group, WIFI_DNS_IDLE_BIT);
    }
    if(!_arduino_event_queue){
    	_arduino_event_queue = xQueueCreate(32, sizeof(arduino_event_t*));
        if(!_arduino_event_queue){
            log_e("Network Event Queue Create Failed!");
            return false;
        }
    }

    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    	log_e("esp_event_loop_create_default failed!");
        return err;
    }

    if(!_arduino_event_task_handle){
        xTaskCreateUniversal(_arduino_event_task, "arduino_events", 4096, NULL, ESP_TASKD_EVENT_PRIO - 1, &_arduino_event_task_handle, ARDUINO_EVENT_RUNNING_CORE);
        if(!_arduino_event_task_handle){
            log_e("Network Event Task Start Failed!");
            return false;
        }
    }

    if(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &_arduino_event_cb, NULL, NULL)){
        log_e("event_handler_instance_register for WIFI_EVENT Failed!");
        return false;
    }

    if(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &_arduino_event_cb, NULL, NULL)){
        log_e("event_handler_instance_register for IP_EVENT Failed!");
        return false;
    }

    if(esp_event_handler_instance_register(SC_EVENT, ESP_EVENT_ANY_ID, &_arduino_event_cb, NULL, NULL)){
        log_e("event_handler_instance_register for SC_EVENT Failed!");
        return false;
    }

    if(esp_event_handler_instance_register(ETH_EVENT, ESP_EVENT_ANY_ID, &_arduino_event_cb, NULL, NULL)){
        log_e("event_handler_instance_register for ETH_EVENT Failed!");
        return false;
    }

    if(esp_event_handler_instance_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &_arduino_event_cb, NULL, NULL)){
        log_e("event_handler_instance_register for WIFI_PROV_EVENT Failed!");
        return false;
    }

    return true;
}

bool tcpipInit(){
    static bool initialized = false;
    if(!initialized){
        initialized = true;
#if CONFIG_IDF_TARGET_ESP32
        uint8_t mac[8];
        if(esp_efuse_mac_get_default(mac) == ESP_OK){
            esp_base_mac_addr_set(mac);
        }
#endif
        initialized = esp_netif_init() == ESP_OK;
        if(initialized){
        	initialized = _start_network_event_task();
        } else {
        	log_e("esp_netif_init failed!");
        }
    }
    return initialized;
}

/*
 * WiFi INIT
 * */

static bool lowLevelInitDone = false;
bool WiFiGenericClass::_wifiUseStaticBuffers = false;

bool WiFiGenericClass::useStaticBuffers(){
    return _wifiUseStaticBuffers;
}

void WiFiGenericClass::useStaticBuffers(bool bufferMode){
    if (lowLevelInitDone) {
        log_w("WiFi already started. Call WiFi.mode(WIFI_MODE_NULL) before setting Static Buffer Mode.");
    } 
    _wifiUseStaticBuffers = bufferMode;
}

// Temporary fix to ensure that CDC+JTAG stay on on ESP32-C3
#if CONFIG_IDF_TARGET_ESP32C3
extern "C" void phy_bbpll_en_usb(bool en);
#endif

bool wifiLowLevelInit(bool persistent){
    if(!lowLevelInitDone){
        lowLevelInitDone = true;
        if(!tcpipInit()){
        	lowLevelInitDone = false;
        	return lowLevelInitDone;
        }
        if(esp_netifs[ESP_IF_WIFI_AP] == NULL){
            esp_netifs[ESP_IF_WIFI_AP] = esp_netif_create_default_wifi_ap();
        }
        if(esp_netifs[ESP_IF_WIFI_STA] == NULL){
            esp_netifs[ESP_IF_WIFI_STA] = esp_netif_create_default_wifi_sta();
        }

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

	if(!WiFiGenericClass::useStaticBuffers()) {
	    cfg.static_tx_buf_num = 0;
            cfg.dynamic_tx_buf_num = 32;
	    cfg.tx_buf_type = 1;
            cfg.cache_tx_buf_num = 4;  // can't be zero!
	    cfg.static_rx_buf_num = 4;
            cfg.dynamic_rx_buf_num = 32;
        }

        esp_err_t err = esp_wifi_init(&cfg);
        if(err){
            log_e("esp_wifi_init %d", err);
        	lowLevelInitDone = false;
        	return lowLevelInitDone;
        }
// Temporary fix to ensure that CDC+JTAG stay on on ESP32-C3
#if CONFIG_IDF_TARGET_ESP32C3
	phy_bbpll_en_usb(true);
#endif
        if(!persistent){
        	lowLevelInitDone = esp_wifi_set_storage(WIFI_STORAGE_RAM) == ESP_OK;
        }
        if(lowLevelInitDone){
			arduino_event_t arduino_event;
			arduino_event.event_id = ARDUINO_EVENT_WIFI_READY;
			postArduinoEvent(&arduino_event);
        }
    }
    return lowLevelInitDone;
}

static bool wifiLowLevelDeinit(){
    if(lowLevelInitDone){
    	lowLevelInitDone = !(esp_wifi_deinit() == ESP_OK);
    }
    return !lowLevelInitDone;
}

static bool _esp_wifi_started = false;

static bool espWiFiStart(){
    if(_esp_wifi_started){
        return true;
    }
    _esp_wifi_started = true;
    esp_err_t err = esp_wifi_start();
    if (err != ESP_OK) {
        _esp_wifi_started = false;
        log_e("esp_wifi_start %d", err);
        return _esp_wifi_started;
    }
    return _esp_wifi_started;
}

static bool espWiFiStop(){
    esp_err_t err;
    if(!_esp_wifi_started){
        return true;
    }
    _esp_wifi_started = false;
    err = esp_wifi_stop();
    if(err){
        log_e("Could not stop WiFi! %d", err);
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
    arduino_event_id_t event;

    WiFiEventCbList() : id(current_id++), cb(NULL), fcb(NULL), scb(NULL), event(ARDUINO_EVENT_WIFI_READY) {}
} WiFiEventCbList_t;
wifi_event_id_t WiFiEventCbList::current_id = 1;


// arduino dont like std::vectors move static here
static std::vector<WiFiEventCbList_t> cbEventList;

bool WiFiGenericClass::_persistent = true;
bool WiFiGenericClass::_long_range = false;
wifi_mode_t WiFiGenericClass::_forceSleepLastMode = WIFI_MODE_NULL;
#if CONFIG_IDF_TARGET_ESP32S2
wifi_ps_type_t WiFiGenericClass::_sleepEnabled = WIFI_PS_NONE;
#else
wifi_ps_type_t WiFiGenericClass::_sleepEnabled = WIFI_PS_MIN_MODEM;
#endif

WiFiGenericClass::WiFiGenericClass() 
{
}

/**
 * @brief Convert wifi_err_reason_t to a string.
 * @param [in] reason The reason to be converted.
 * @return A string representation of the error code.
 * @note: wifi_err_reason_t values as of Mar 2023 (arduino-esp32 r2.0.7) are: (1-39, 46-51, 67-68, 200-208) and are defined in /tools/sdk/esp32/include/esp_wifi/include/esp_wifi_types.h.
 */
const char * WiFiGenericClass::disconnectReasonName(wifi_err_reason_t reason) {
    switch(reason) {
        //ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(2,0,7)
        case WIFI_REASON_UNSPECIFIED: return "UNSPECIFIED";
        case WIFI_REASON_AUTH_EXPIRE: return "AUTH_EXPIRE";
        case WIFI_REASON_AUTH_LEAVE: return "AUTH_LEAVE";
        case WIFI_REASON_ASSOC_EXPIRE: return "ASSOC_EXPIRE";
        case WIFI_REASON_ASSOC_TOOMANY: return "ASSOC_TOOMANY";
        case WIFI_REASON_NOT_AUTHED: return "NOT_AUTHED";
        case WIFI_REASON_NOT_ASSOCED: return "NOT_ASSOCED";
        case WIFI_REASON_ASSOC_LEAVE: return "ASSOC_LEAVE";
        case WIFI_REASON_ASSOC_NOT_AUTHED: return "ASSOC_NOT_AUTHED";
        case WIFI_REASON_DISASSOC_PWRCAP_BAD: return "DISASSOC_PWRCAP_BAD";
        case WIFI_REASON_DISASSOC_SUPCHAN_BAD: return "DISASSOC_SUPCHAN_BAD";
        case WIFI_REASON_BSS_TRANSITION_DISASSOC: return "BSS_TRANSITION_DISASSOC";
        case WIFI_REASON_IE_INVALID: return "IE_INVALID";
        case WIFI_REASON_MIC_FAILURE: return "MIC_FAILURE";
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: return "4WAY_HANDSHAKE_TIMEOUT";
        case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT: return "GROUP_KEY_UPDATE_TIMEOUT";
        case WIFI_REASON_IE_IN_4WAY_DIFFERS: return "IE_IN_4WAY_DIFFERS";
        case WIFI_REASON_GROUP_CIPHER_INVALID: return "GROUP_CIPHER_INVALID";
        case WIFI_REASON_PAIRWISE_CIPHER_INVALID: return "PAIRWISE_CIPHER_INVALID";
        case WIFI_REASON_AKMP_INVALID: return "AKMP_INVALID";
        case WIFI_REASON_UNSUPP_RSN_IE_VERSION: return "UNSUPP_RSN_IE_VERSION";
        case WIFI_REASON_INVALID_RSN_IE_CAP: return "INVALID_RSN_IE_CAP";
        case WIFI_REASON_802_1X_AUTH_FAILED: return "802_1X_AUTH_FAILED";
        case WIFI_REASON_CIPHER_SUITE_REJECTED: return "CIPHER_SUITE_REJECTED";
        case WIFI_REASON_TDLS_PEER_UNREACHABLE: return "TDLS_PEER_UNREACHABLE";
        case WIFI_REASON_TDLS_UNSPECIFIED: return "TDLS_UNSPECIFIED";
        case WIFI_REASON_SSP_REQUESTED_DISASSOC: return "SSP_REQUESTED_DISASSOC";
        case WIFI_REASON_NO_SSP_ROAMING_AGREEMENT: return "NO_SSP_ROAMING_AGREEMENT";
        case WIFI_REASON_BAD_CIPHER_OR_AKM: return "BAD_CIPHER_OR_AKM";
        case WIFI_REASON_NOT_AUTHORIZED_THIS_LOCATION: return "NOT_AUTHORIZED_THIS_LOCATION";
        case WIFI_REASON_SERVICE_CHANGE_PERCLUDES_TS: return "SERVICE_CHANGE_PERCLUDES_TS";
        case WIFI_REASON_UNSPECIFIED_QOS: return "UNSPECIFIED_QOS";
        case WIFI_REASON_NOT_ENOUGH_BANDWIDTH: return "NOT_ENOUGH_BANDWIDTH";
        case WIFI_REASON_MISSING_ACKS: return "MISSING_ACKS";
        case WIFI_REASON_EXCEEDED_TXOP: return "EXCEEDED_TXOP";
        case WIFI_REASON_STA_LEAVING: return "STA_LEAVING";
        case WIFI_REASON_END_BA: return "END_BA";
        case WIFI_REASON_UNKNOWN_BA: return "UNKNOWN_BA";
        case WIFI_REASON_TIMEOUT: return "TIMEOUT";
        case WIFI_REASON_PEER_INITIATED: return "PEER_INITIATED";
        case WIFI_REASON_AP_INITIATED: return "AP_INITIATED";
        case WIFI_REASON_INVALID_FT_ACTION_FRAME_COUNT: return "INVALID_FT_ACTION_FRAME_COUNT";
        case WIFI_REASON_INVALID_PMKID: return "INVALID_PMKID";
        case WIFI_REASON_INVALID_MDE: return "INVALID_MDE";
        case WIFI_REASON_INVALID_FTE: return "INVALID_FTE";
        case WIFI_REASON_TRANSMISSION_LINK_ESTABLISH_FAILED: return "TRANSMISSION_LINK_ESTABLISH_FAILED";
        case WIFI_REASON_ALTERATIVE_CHANNEL_OCCUPIED: return "ALTERATIVE_CHANNEL_OCCUPIED";
        case WIFI_REASON_BEACON_TIMEOUT: return "BEACON_TIMEOUT";
        case WIFI_REASON_NO_AP_FOUND: return "NO_AP_FOUND";
        case WIFI_REASON_AUTH_FAIL: return "AUTH_FAIL";
        case WIFI_REASON_ASSOC_FAIL: return "ASSOC_FAIL";
        case WIFI_REASON_HANDSHAKE_TIMEOUT: return "HANDSHAKE_TIMEOUT";
        case WIFI_REASON_CONNECTION_FAIL: return "CONNECTION_FAIL";
        case WIFI_REASON_AP_TSF_RESET: return "AP_TSF_RESET";
        case WIFI_REASON_ROAMING: return "ROAMING";
        case WIFI_REASON_ASSOC_COMEBACK_TIME_TOO_LONG: return "ASSOC_COMEBACK_TIME_TOO_LONG";
        default: return "";
    }
}

/**
 * @brief Convert arduino_event_id_t to a C string.
 * @param [in] id The event id to be converted.
 * @return A string representation of the event id.
 * @note: arduino_event_id_t values as of Mar 2023 (arduino-esp32 r2.0.7) are: 0-39 (ARDUINO_EVENT_MAX=40) and are defined in WiFiGeneric.h.
 */
const char * WiFiGenericClass::eventName(arduino_event_id_t id) {
    switch(id) {
        case ARDUINO_EVENT_WIFI_READY: return "WIFI_READY";
        case ARDUINO_EVENT_WIFI_SCAN_DONE: return "SCAN_DONE";
        case ARDUINO_EVENT_WIFI_STA_START: return "STA_START";
        case ARDUINO_EVENT_WIFI_STA_STOP: return "STA_STOP";
        case ARDUINO_EVENT_WIFI_STA_CONNECTED: return "STA_CONNECTED";
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: return "STA_DISCONNECTED";
        case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE: return "STA_AUTHMODE_CHANGE";
        case ARDUINO_EVENT_WIFI_STA_GOT_IP: return "STA_GOT_IP";
        case ARDUINO_EVENT_WIFI_STA_GOT_IP6: return "STA_GOT_IP6";
        case ARDUINO_EVENT_WIFI_STA_LOST_IP: return "STA_LOST_IP";
        case ARDUINO_EVENT_WIFI_AP_START: return "AP_START";
        case ARDUINO_EVENT_WIFI_AP_STOP: return "AP_STOP";
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED: return "AP_STACONNECTED";
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED: return "AP_STADISCONNECTED";
        case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED: return "AP_STAIPASSIGNED";
        case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED: return "AP_PROBEREQRECVED";
        case ARDUINO_EVENT_WIFI_AP_GOT_IP6: return "AP_GOT_IP6";
        case ARDUINO_EVENT_WIFI_FTM_REPORT: return "FTM_REPORT";
        case ARDUINO_EVENT_ETH_START: return "ETH_START";
        case ARDUINO_EVENT_ETH_STOP: return "ETH_STOP";
        case ARDUINO_EVENT_ETH_CONNECTED: return "ETH_CONNECTED";
        case ARDUINO_EVENT_ETH_DISCONNECTED: return "ETH_DISCONNECTED";
        case ARDUINO_EVENT_ETH_GOT_IP: return "ETH_GOT_IP";
        case ARDUINO_EVENT_ETH_LOST_IP: return "ETH_LOST_IP";
        case ARDUINO_EVENT_ETH_GOT_IP6: return "ETH_GOT_IP6";
        case ARDUINO_EVENT_WPS_ER_SUCCESS: return "WPS_ER_SUCCESS";
        case ARDUINO_EVENT_WPS_ER_FAILED: return "WPS_ER_FAILED";
        case ARDUINO_EVENT_WPS_ER_TIMEOUT: return "WPS_ER_TIMEOUT";
        case ARDUINO_EVENT_WPS_ER_PIN: return "WPS_ER_PIN";
        case ARDUINO_EVENT_WPS_ER_PBC_OVERLAP: return "WPS_ER_PBC_OVERLAP";
        case ARDUINO_EVENT_SC_SCAN_DONE: return "SC_SCAN_DONE";
        case ARDUINO_EVENT_SC_FOUND_CHANNEL: return "SC_FOUND_CHANNEL";
        case ARDUINO_EVENT_SC_GOT_SSID_PSWD: return "SC_GOT_SSID_PSWD";
        case ARDUINO_EVENT_SC_SEND_ACK_DONE: return "SC_SEND_ACK_DONE";
        case ARDUINO_EVENT_PROV_INIT: return "PROV_INIT";
        case ARDUINO_EVENT_PROV_DEINIT: return "PROV_DEINIT";
        case ARDUINO_EVENT_PROV_START: return "PROV_START";
        case ARDUINO_EVENT_PROV_END: return "PROV_END";
        case ARDUINO_EVENT_PROV_CRED_RECV: return "PROV_CRED_RECV";
        case ARDUINO_EVENT_PROV_CRED_FAIL: return "PROV_CRED_FAIL";
        case ARDUINO_EVENT_PROV_CRED_SUCCESS: return "PROV_CRED_SUCCESS";
        default: return "";
    }
}

const char * WiFiGenericClass::getHostname()
{
    return get_esp_netif_hostname();
}

bool WiFiGenericClass::setHostname(const char * hostname)
{
    set_esp_netif_hostname(hostname);
    return true;
}

int WiFiGenericClass::setStatusBits(int bits){
    if(!_arduino_event_group){
        return 0;
    }
    return xEventGroupSetBits(_arduino_event_group, bits);
}

int WiFiGenericClass::clearStatusBits(int bits){
    if(!_arduino_event_group){
        return 0;
    }
    return xEventGroupClearBits(_arduino_event_group, bits);
}

int WiFiGenericClass::getStatusBits(){
    if(!_arduino_event_group){
        return 0;
    }
    return xEventGroupGetBits(_arduino_event_group);
}

int WiFiGenericClass::waitStatusBits(int bits, uint32_t timeout_ms){
    if(!_arduino_event_group){
        return 0;
    }
    return xEventGroupWaitBits(
        _arduino_event_group,    // The event group being tested.
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
wifi_event_id_t WiFiGenericClass::onEvent(WiFiEventCb cbEvent, arduino_event_id_t event)
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

wifi_event_id_t WiFiGenericClass::onEvent(WiFiEventFuncCb cbEvent, arduino_event_id_t event)
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

wifi_event_id_t WiFiGenericClass::onEvent(WiFiEventSysCb cbEvent, arduino_event_id_t event)
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
void WiFiGenericClass::removeEvent(WiFiEventCb cbEvent, arduino_event_id_t event)
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

void WiFiGenericClass::removeEvent(WiFiEventSysCb cbEvent, arduino_event_id_t event)
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
esp_err_t WiFiGenericClass::_eventCallback(arduino_event_t *event)
{
    static bool first_connect = true;

    if(!event) return ESP_OK;                                                       //Null would crash this function

    log_d("Arduino Event: %d - %s", event->event_id, WiFi.eventName(event->event_id));
    if(event->event_id == ARDUINO_EVENT_WIFI_SCAN_DONE) {
        WiFiScanClass::_scanDone();
    } else if(event->event_id == ARDUINO_EVENT_WIFI_STA_START) {
        WiFiSTAClass::_setStatus(WL_DISCONNECTED);
        setStatusBits(STA_STARTED_BIT);
        if(esp_wifi_set_ps(_sleepEnabled) != ESP_OK){
            log_e("esp_wifi_set_ps failed");
        }
    } else if(event->event_id == ARDUINO_EVENT_WIFI_STA_STOP) {
        WiFiSTAClass::_setStatus(WL_STOPPED);
        clearStatusBits(STA_STARTED_BIT | STA_CONNECTED_BIT | STA_HAS_IP_BIT | STA_HAS_IP6_BIT);
    } else if(event->event_id == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
        WiFiSTAClass::_setStatus(WL_IDLE_STATUS);
        setStatusBits(STA_CONNECTED_BIT);

        //esp_netif_create_ip6_linklocal(esp_netifs[ESP_IF_WIFI_STA]);
    } else if(event->event_id == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        uint8_t reason = event->event_info.wifi_sta_disconnected.reason;
        // Reason 0 causes crash, use reason 1 (UNSPECIFIED) instead
        if(!reason)
	    reason = WIFI_REASON_UNSPECIFIED;
        log_w("Reason: %u - %s", reason, WiFi.disconnectReasonName((wifi_err_reason_t)reason));
        if(reason == WIFI_REASON_NO_AP_FOUND) {
            WiFiSTAClass::_setStatus(WL_NO_SSID_AVAIL);
        } else if((reason == WIFI_REASON_AUTH_FAIL) && !first_connect){
            WiFiSTAClass::_setStatus(WL_CONNECT_FAILED);
        } else if(reason == WIFI_REASON_BEACON_TIMEOUT || reason == WIFI_REASON_HANDSHAKE_TIMEOUT) {
            WiFiSTAClass::_setStatus(WL_CONNECTION_LOST);
        } else if(reason == WIFI_REASON_AUTH_EXPIRE) {

        } else {
            WiFiSTAClass::_setStatus(WL_DISCONNECTED);
        }
        clearStatusBits(STA_CONNECTED_BIT | STA_HAS_IP_BIT | STA_HAS_IP6_BIT);

        bool DoReconnect = false;
        if(reason == WIFI_REASON_ASSOC_LEAVE) {                                     //Voluntarily disconnected. Don't reconnect!
        }
        else if(first_connect) {                                                    //Retry once for all failure reasons
            first_connect = false;
            DoReconnect = true;
            log_d("WiFi Reconnect Running");
        }
        else if(WiFi.getAutoReconnect() && _isReconnectableReason(reason)) {
            DoReconnect = true;
            log_d("WiFi AutoReconnect Running");
        }
        else if(reason == WIFI_REASON_ASSOC_FAIL) {
            WiFiSTAClass::_setStatus(WL_CONNECT_FAILED);
        }
        if(DoReconnect) {
            WiFi.disconnect();
            WiFi.begin();
        }
    } else if(event->event_id == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
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
    } else if(event->event_id == ARDUINO_EVENT_WIFI_STA_LOST_IP) {
        WiFiSTAClass::_setStatus(WL_IDLE_STATUS);
        clearStatusBits(STA_HAS_IP_BIT);

    } else if(event->event_id == ARDUINO_EVENT_WIFI_AP_START) {
        setStatusBits(AP_STARTED_BIT);
    } else if(event->event_id == ARDUINO_EVENT_WIFI_AP_STOP) {
        clearStatusBits(AP_STARTED_BIT | AP_HAS_CLIENT_BIT);
    } else if(event->event_id == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
        setStatusBits(AP_HAS_CLIENT_BIT);
    } else if(event->event_id == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
        wifi_sta_list_t clients;
        if(esp_wifi_ap_get_sta_list(&clients) != ESP_OK || !clients.num){
            clearStatusBits(AP_HAS_CLIENT_BIT);
        }

    } else if(event->event_id == ARDUINO_EVENT_ETH_START) {
        setStatusBits(ETH_STARTED_BIT);
    } else if(event->event_id == ARDUINO_EVENT_ETH_STOP) {
        clearStatusBits(ETH_STARTED_BIT | ETH_CONNECTED_BIT | ETH_HAS_IP_BIT | ETH_HAS_IP6_BIT);
    } else if(event->event_id == ARDUINO_EVENT_ETH_CONNECTED) {
        setStatusBits(ETH_CONNECTED_BIT);
    } else if(event->event_id == ARDUINO_EVENT_ETH_DISCONNECTED) {
        clearStatusBits(ETH_CONNECTED_BIT | ETH_HAS_IP_BIT | ETH_HAS_IP6_BIT);
    } else if(event->event_id == ARDUINO_EVENT_ETH_GOT_IP) {
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
    } else if(event->event_id == ARDUINO_EVENT_ETH_LOST_IP) {
        clearStatusBits(ETH_HAS_IP_BIT);

    } else if(event->event_id == ARDUINO_EVENT_WIFI_STA_GOT_IP6) {
    	setStatusBits(STA_CONNECTED_BIT | STA_HAS_IP6_BIT);
    } else if(event->event_id == ARDUINO_EVENT_WIFI_AP_GOT_IP6) {
    	setStatusBits(AP_HAS_IP6_BIT);
    } else if(event->event_id == ARDUINO_EVENT_ETH_GOT_IP6) {
    	setStatusBits(ETH_CONNECTED_BIT | ETH_HAS_IP6_BIT);
    } else if(event->event_id == ARDUINO_EVENT_SC_GOT_SSID_PSWD) {
    	WiFi.begin(
			(const char *)event->event_info.sc_got_ssid_pswd.ssid,
			(const char *)event->event_info.sc_got_ssid_pswd.password,
			0,
			((event->event_info.sc_got_ssid_pswd.bssid_set == true)?event->event_info.sc_got_ssid_pswd.bssid:NULL)
		);
    } else if(event->event_id == ARDUINO_EVENT_SC_SEND_ACK_DONE) {
    	esp_smartconfig_stop();
    	WiFiSTAClass::_smartConfigDone = true;
    }

    for(uint32_t i = 0; i < cbEventList.size(); i++) {
        WiFiEventCbList_t entry = cbEventList[i];
        if(entry.cb || entry.fcb || entry.scb) {
            if(entry.event == (arduino_event_id_t) event->event_id || entry.event == ARDUINO_EVENT_MAX) {
                if(entry.cb) {
                    entry.cb((arduino_event_id_t) event->event_id);
                } else if(entry.fcb) {
                    entry.fcb((arduino_event_id_t) event->event_id, (arduino_event_info_t) event->event_info);
                } else {
                    entry.scb(event);
                }
            }
        }
    }
    return ESP_OK;
}

bool WiFiGenericClass::_isReconnectableReason(uint8_t reason) {
    switch(reason) {
        case WIFI_REASON_UNSPECIFIED:
        //Timeouts (retry)
        case WIFI_REASON_AUTH_EXPIRE:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
        case WIFI_REASON_802_1X_AUTH_FAILED:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
        //Transient error (reconnect)
        case WIFI_REASON_AUTH_LEAVE:
        case WIFI_REASON_ASSOC_EXPIRE:
        case WIFI_REASON_ASSOC_TOOMANY:
        case WIFI_REASON_NOT_AUTHED:
        case WIFI_REASON_NOT_ASSOCED:
        case WIFI_REASON_ASSOC_NOT_AUTHED:
        case WIFI_REASON_MIC_FAILURE:
        case WIFI_REASON_IE_IN_4WAY_DIFFERS:
        case WIFI_REASON_INVALID_PMKID:
        case WIFI_REASON_BEACON_TIMEOUT:
        case WIFI_REASON_NO_AP_FOUND:
        case WIFI_REASON_ASSOC_FAIL:
        case WIFI_REASON_CONNECTION_FAIL:
        case WIFI_REASON_AP_TSF_RESET:
        case WIFI_REASON_ROAMING:
            return true;
        default:
            return false;
    }
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
 * enable WiFi long range mode
 * @param enable
 */
void WiFiGenericClass::enableLongRange(bool enable)
{
    _long_range = enable;
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
        if(!wifiLowLevelInit(_persistent)){
            return false;
        }
    } else if(cm && !m){
        return espWiFiStop();
    }

    esp_err_t err;
    if(m & WIFI_MODE_STA){
    	err = set_esp_interface_hostname(ESP_IF_WIFI_STA, get_esp_netif_hostname());
        if(err){
            log_e("Could not set hostname! %d", err);
            return false;
        }
    }
    err = esp_wifi_set_mode(m);
    if(err){
        log_e("Could not set mode! %d", err);
        return false;
    }
    if(_long_range){
        if(m & WIFI_MODE_STA){
            err = esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
            if(err != ESP_OK){
                log_e("Could not enable long range on STA! %d", err);
                return false;
            }
        }
        if(m & WIFI_MODE_AP){
            err = esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_LR);
            if(err != ESP_OK){
                log_e("Could not enable long range on AP! %d", err);
                return false;
            }
        }
    }
    if(!espWiFiStart()){
        return false;
    }

    #ifdef BOARD_HAS_DUAL_ANTENNA
        if(!setDualAntennaConfig(ANT1, ANT2, WIFI_RX_ANT_AUTO, WIFI_TX_ANT_AUTO)){
            log_e("Dual Antenna Config failed!");
            return false;
        }
    #endif

    return true;
}

/**
 * get WiFi mode
 * @return WiFiMode
 */
wifi_mode_t WiFiGenericClass::getMode()
{
    if(!lowLevelInitDone || !_esp_wifi_started){
        return WIFI_MODE_NULL;
    }
    wifi_mode_t mode;
    if(esp_wifi_get_mode(&mode) != ESP_OK){
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
bool WiFiGenericClass::setSleep(bool enabled){
    return setSleep(enabled?WIFI_PS_MIN_MODEM:WIFI_PS_NONE);
}

/**
 * control modem sleep when only in STA mode
 * @param mode wifi_ps_type_t
 * @return ok
 */
bool WiFiGenericClass::setSleep(wifi_ps_type_t sleepType)
{
    if(sleepType != _sleepEnabled){
        _sleepEnabled = sleepType;
        if((getMode() & WIFI_MODE_STA) != 0){
            if(esp_wifi_set_ps(_sleepEnabled) != ESP_OK){
                log_e("esp_wifi_set_ps failed!");
                return false;
            }
        }
        return true;
    }
    return false;
}

/**
 * get modem sleep enabled
 * @return true if modem sleep is enabled
 */
wifi_ps_type_t WiFiGenericClass::getSleep()
{
    return _sleepEnabled;
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

/**
 * Initiate FTM Session.
 * @param frm_count Number of FTM frames requested in terms of 4 or 8 bursts (allowed values - 0(No pref), 16, 24, 32, 64)
 * @param burst_period Requested time period between consecutive FTM bursts in 100's of milliseconds (allowed values - 0(No pref), 2 - 255)
 * @param channel Primary channel of the FTM Responder
 * @param mac MAC address of the FTM Responder
 * @return true on success
 */
bool WiFiGenericClass::initiateFTM(uint8_t frm_count, uint16_t burst_period, uint8_t channel, const uint8_t * mac) {
  wifi_ftm_initiator_cfg_t ftmi_cfg = {
    .resp_mac = {0,0,0,0,0,0},
    .channel = channel,
    .frm_count = frm_count,
    .burst_period = burst_period,
  };
  if(mac != NULL){
    memcpy(ftmi_cfg.resp_mac, mac, 6);
  }
  // Request FTM session with the Responder
  if (ESP_OK != esp_wifi_ftm_initiate_session(&ftmi_cfg)) {
    log_e("Failed to initiate FTM session");
    return false;
  }
  return true;
}

/**
 * Configure Dual antenna.
 * @param gpio_ant1 Configure the GPIO number for the antenna 1 connected to the RF switch (default GPIO2 on ESP32-WROOM-DA)
 * @param gpio_ant2 Configure the GPIO number for the antenna 2 connected to the RF switch (default GPIO25 on ESP32-WROOM-DA)
 * @param rx_mode Set the RX antenna mode. See wifi_rx_ant_t for the options.
 * @param tx_mode Set the TX antenna mode. See wifi_tx_ant_t for the options.
 * @return true on success
 */
bool WiFiGenericClass::setDualAntennaConfig(uint8_t gpio_ant1, uint8_t gpio_ant2, wifi_rx_ant_t rx_mode, wifi_tx_ant_t tx_mode) {

    wifi_ant_gpio_config_t wifi_ant_io;

    if (ESP_OK != esp_wifi_get_ant_gpio(&wifi_ant_io)) {
        log_e("Failed to get antenna configuration");
        return false;
    }

    wifi_ant_io.gpio_cfg[0].gpio_num = gpio_ant1;
    wifi_ant_io.gpio_cfg[0].gpio_select = 1;
    wifi_ant_io.gpio_cfg[1].gpio_num = gpio_ant2;
    wifi_ant_io.gpio_cfg[1].gpio_select = 1;

    if (ESP_OK != esp_wifi_set_ant_gpio(&wifi_ant_io)) {
        log_e("Failed to set antenna GPIO configuration");
        return false;
    }

    // Set antenna default configuration
    wifi_ant_config_t ant_config = {
        .rx_ant_mode = WIFI_ANT_MODE_AUTO,
        .rx_ant_default = WIFI_ANT_MAX, // Ignored in AUTO mode
        .tx_ant_mode = WIFI_ANT_MODE_AUTO,
        .enabled_ant0 = 1,
        .enabled_ant1 = 2,
    };

    switch (rx_mode)
    {
    case WIFI_RX_ANT0:
        ant_config.rx_ant_mode = WIFI_ANT_MODE_ANT0;
        break;
    case WIFI_RX_ANT1:
        ant_config.rx_ant_mode = WIFI_ANT_MODE_ANT1;
        break;
    case WIFI_RX_ANT_AUTO:
        log_i("TX Antenna will be automatically selected");
        ant_config.rx_ant_default = WIFI_ANT_ANT0;
        ant_config.rx_ant_mode = WIFI_ANT_MODE_AUTO;
        // Force TX for AUTO if RX is AUTO
        ant_config.tx_ant_mode = WIFI_ANT_MODE_AUTO;
        goto set_ant;
        break;
    default:
        log_e("Invalid default antenna! Falling back to AUTO");
        ant_config.rx_ant_mode = WIFI_ANT_MODE_AUTO;
        break;
    }

    switch (tx_mode)
    {
    case WIFI_TX_ANT0:
        ant_config.tx_ant_mode = WIFI_ANT_MODE_ANT0;
        break;
    case WIFI_TX_ANT1:
        ant_config.tx_ant_mode = WIFI_ANT_MODE_ANT1;
        break;
    case WIFI_TX_ANT_AUTO:
        log_i("RX Antenna will be automatically selected");
        ant_config.rx_ant_default = WIFI_ANT_ANT0;
        ant_config.tx_ant_mode = WIFI_ANT_MODE_AUTO;
        // Force RX for AUTO if RX is AUTO
        ant_config.rx_ant_mode = WIFI_ANT_MODE_AUTO;
        break;
    default:
        log_e("Invalid default antenna! Falling back to AUTO");
        ant_config.rx_ant_default = WIFI_ANT_ANT0;
        ant_config.tx_ant_mode = WIFI_ANT_MODE_AUTO;
        break;
    }

set_ant:
    if (ESP_OK != esp_wifi_set_ant(&ant_config)) {
        log_e("Failed to set antenna configuration");
        return false;
    }

    return true;
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
    xEventGroupSetBits(_arduino_event_group, WIFI_DNS_DONE_BIT);
}

typedef struct gethostbynameParameters {
    const char *hostname;
    ip_addr_t addr;
    void *callback_arg;
} gethostbynameParameters_t;

/**
 * Callback to execute dns_gethostbyname in lwIP's TCP/IP context
 * @param param Parameters for dns_gethostbyname call
 */
static esp_err_t wifi_gethostbyname_tcpip_ctx(void *param)
{
    gethostbynameParameters_t *parameters = static_cast<gethostbynameParameters_t *>(param);
    return dns_gethostbyname(parameters->hostname, &parameters->addr, &wifi_dns_found_callback, parameters->callback_arg);
}

/**
 * Resolve the given hostname to an IP address. If passed hostname is an IP address, it will be parsed into IPAddress structure.
 * @param aHostname     Name to be resolved or string containing IP address
 * @param aResult       IPAddress structure to store the returned IP address
 * @return 1 if aIPAddrString was successfully converted to an IP address,
 *          else error code
 */
int WiFiGenericClass::hostByName(const char* aHostname, IPAddress& aResult)
{
    if (!aResult.fromString(aHostname))
    {
        gethostbynameParameters_t params;
        params.hostname = aHostname;
        params.callback_arg = &aResult;
        aResult = static_cast<uint32_t>(0);
        waitStatusBits(WIFI_DNS_IDLE_BIT, 16000);
        clearStatusBits(WIFI_DNS_IDLE_BIT | WIFI_DNS_DONE_BIT);
        err_t err = esp_netif_tcpip_exec(wifi_gethostbyname_tcpip_ctx, &params);
        if(err == ERR_OK && params.addr.u_addr.ip4.addr) {
            aResult = params.addr.u_addr.ip4.addr;
        } else if(err == ERR_INPROGRESS) {
            waitStatusBits(WIFI_DNS_DONE_BIT, 15000);  //real internal timeout in lwip library is 14[s]
            clearStatusBits(WIFI_DNS_DONE_BIT);
        }
        setStatusBits(WIFI_DNS_IDLE_BIT);
        if((uint32_t)aResult == 0){
            log_e("DNS Failed for %s", aHostname);
        }
    }
    return (uint32_t)aResult != 0;
}

IPAddress WiFiGenericClass::calculateNetworkID(IPAddress ip, IPAddress subnet) {
	IPAddress networkID;

	for (size_t i = 0; i < 4; i++)
		networkID[i] = subnet[i] & ip[i];

	return networkID;
}

IPAddress WiFiGenericClass::calculateBroadcast(IPAddress ip, IPAddress subnet) {
    IPAddress broadcastIp;
    
    for (int i = 0; i < 4; i++)
        broadcastIp[i] = ~subnet[i] | ip[i];

    return broadcastIp;
}

uint8_t WiFiGenericClass::calculateSubnetCIDR(IPAddress subnetMask) {
	uint8_t CIDR = 0;

	for (uint8_t i = 0; i < 4; i++) {
		if (subnetMask[i] == 0x80)  // 128
			CIDR += 1;
		else if (subnetMask[i] == 0xC0)  // 192
			CIDR += 2;
		else if (subnetMask[i] == 0xE0)  // 224
			CIDR += 3;
		else if (subnetMask[i] == 0xF0)  // 242
			CIDR += 4;
		else if (subnetMask[i] == 0xF8)  // 248
			CIDR += 5;
		else if (subnetMask[i] == 0xFC)  // 252
			CIDR += 6;
		else if (subnetMask[i] == 0xFE)  // 254
			CIDR += 7;
		else if (subnetMask[i] == 0xFF)  // 255
			CIDR += 8;
	}

	return CIDR;
}
