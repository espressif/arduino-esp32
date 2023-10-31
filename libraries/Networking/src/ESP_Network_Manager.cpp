#include "ESP_Network_Manager.h"
#include "esp_netif.h"
#include "lwip/ip_addr.h"
#include "lwip/dns.h"
#include "esp32-hal-log.h"
#include "esp_mac.h"

ESP_Network_Manager::ESP_Network_Manager(){

}

bool ESP_Network_Manager::begin(){
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
 * DNS callback
 * @param name
 * @param ipaddr
 * @param callback_arg
 */
static void wifi_dns_found_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg){
    if(ipaddr) {
        (*reinterpret_cast<IPAddress*>(callback_arg)) = ipaddr->u_addr.ip4.addr;
    }
    Network.setStatusBits(WIFI_DNS_DONE_BIT);
}

/**
 * Resolve the given hostname to an IP address. If passed hostname is an IP address, it will be parsed into IPAddress structure.
 * @param aHostname     Name to be resolved or string containing IP address
 * @param aResult       IPAddress structure to store the returned IP address
 * @return 1 if aIPAddrString was successfully converted to an IP address,
 *          else error code
 */
int ESP_Network_Manager::hostByName(const char* aHostname, IPAddress& aResult){
    if (!aResult.fromString(aHostname))
    {
        ip_addr_t addr;
        aResult = static_cast<uint32_t>(0);
        waitStatusBits(WIFI_DNS_IDLE_BIT, 16000);
        clearStatusBits(WIFI_DNS_IDLE_BIT | WIFI_DNS_DONE_BIT);
        err_t err = dns_gethostbyname(aHostname, &addr, &wifi_dns_found_callback, &aResult);
        if(err == ERR_OK && addr.u_addr.ip4.addr) {
            aResult = addr.u_addr.ip4.addr;
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

uint8_t * ESP_Network_Manager::macAddress(uint8_t * mac){
    esp_base_mac_addr_get(mac);
    return mac;
}

String ESP_Network_Manager::macAddress(void){
    uint8_t mac[6];
    char macStr[18] = { 0 };
    macAddress(mac);
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
}

ESP_Network_Manager Network;
