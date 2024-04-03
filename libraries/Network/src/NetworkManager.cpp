/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "NetworkManager.h"
#include "NetworkInterface.h"
#include "IPAddress.h"
#include "esp_netif.h"
#include "lwip/dns.h"
#include "esp_mac.h"
#include "netdb.h"

NetworkManager::NetworkManager(){

}

bool NetworkManager::begin(){
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
        if(!initialized){
        	log_e("esp_netif_init failed!");
        }
    }
    if(initialized){
    	initNetworkEvents();
    }
    return initialized;
}

/**
 * Resolve the given hostname to an IP address.
 * @param aHostname     Name to be resolved
 * @param aResult       IPAddress structure to store the returned IP address
 * @return 1 if aIPAddrString was successfully converted to an IP address,
 *          else error code
 */
int NetworkManager::hostByName(const char* aHostname, IPAddress& aResult)
{
    err_t err = ERR_OK;

    // This should generally check if we have a global address assigned to one of the interfaces.
    // If such address is not assigned, there is no point in trying to get V6 from DNS as we will not be able to reach it.
    // That is of course, if 'preferV6' is not set to true
    static bool hasGlobalV6 = false;
    bool hasGlobalV6Now = false;//ToDo: implement this!
    if(hasGlobalV6 != hasGlobalV6Now){
        hasGlobalV6 = hasGlobalV6Now;
        dns_clear_cache();
        log_d("Clearing DNS cache");
    }

    aResult = static_cast<uint32_t>(0);

    // First check if the host parses as a literal address
    if (aResult.fromString(aHostname)) {
        return 1;
    }

    const char *servname = "0";
    struct addrinfo *res;
    const struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
    };
    err = lwip_getaddrinfo(aHostname, servname, &hints, &res);
    if (err == ERR_OK)
    {
        if (res->ai_family == AF_INET6)
        {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)res->ai_addr;
            // As an array of u8_t
            aResult = IPAddress(IPv6, ipv6->sin6_addr.s6_addr);
            log_d("DNS found IPv6 %s", aResult.toString().c_str());
        }
        else
        {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
            // As a single u32_t
            aResult = IPAddress(ipv4->sin_addr.s_addr);
            log_d("DNS found IPv4 %s", aResult.toString().c_str());
        }

        lwip_freeaddrinfo(res);
        return 1;
    }

    log_e("DNS Failed for '%s' with error '%d'", aHostname, err);
    return err;
}

uint8_t * NetworkManager::macAddress(uint8_t * mac){
    esp_base_mac_addr_get(mac);
    return mac;
}

String NetworkManager::macAddress(void){
    uint8_t mac[6];
    char macStr[18] = { 0 };
    macAddress(mac);
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
}

static char default_hostname[32] = {0,};

const char * NetworkManager::getHostname()
{
    if(default_hostname[0] == 0){
        uint8_t eth_mac[6];
        esp_base_mac_addr_get(eth_mac);
        snprintf(default_hostname, 32, "%s%02X%02X%02X", CONFIG_IDF_TARGET "-", eth_mac[3], eth_mac[4], eth_mac[5]);
    }
    return (const char *)default_hostname;
}

bool NetworkManager::setHostname(const char * name)
{
    if(name){
        snprintf(default_hostname, 32, "%s", name);
    }
    return true;
}

NetworkInterface * getNetifByID(Network_Interface_ID id);

size_t NetworkManager::printTo(Print & out) const {
    size_t bytes = 0;

    for (int i = 0; i < ESP_NETIF_ID_MAX; ++i){
        NetworkInterface * iface = getNetifByID((Network_Interface_ID)i);
        if(iface != NULL && iface->netif() != NULL){
            bytes += out.println(*iface);
        }
    }
    return bytes;
}


NetworkManager Network;
