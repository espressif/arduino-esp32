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

typedef struct gethostbynameParameters {
    const char *hostname;
    ip_addr_t addr;
    uint8_t addr_type;
    int result;
} gethostbynameParameters_t;

/**
 * DNS callback
 * @param name
 * @param ipaddr
 * @param callback_arg
 */
static void wifi_dns_found_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    gethostbynameParameters_t *parameters = static_cast<gethostbynameParameters_t *>(callback_arg);
    if(ipaddr) {
        if(parameters->result == 0){
            memcpy(&(parameters->addr), ipaddr, sizeof(ip_addr_t));
            parameters->result = 1;
        }
    } else {
        parameters->result = -1;
    }
    Network.setStatusBits(NET_DNS_DONE_BIT);
}

/**
 * Callback to execute dns_gethostbyname in lwIP's TCP/IP context
 * @param param Parameters for dns_gethostbyname call
 */
static esp_err_t wifi_gethostbyname_tcpip_ctx(void *param)
{
    gethostbynameParameters_t *parameters = static_cast<gethostbynameParameters_t *>(param);
    return dns_gethostbyname_addrtype(parameters->hostname, &parameters->addr, &wifi_dns_found_callback, parameters, parameters->addr_type);
}

/**
 * Resolve the given hostname to an IP address.
 * @param aHostname     Name to be resolved
 * @param aResult       IPAddress structure to store the returned IP address
 * @return 1 if aIPAddrString was successfully converted to an IP address,
 *          else error code
 */
int ESP_Network_Manager::hostByName(const char* aHostname, IPAddress& aResult, bool preferV6)
{
    err_t err = ERR_OK;
    gethostbynameParameters_t params;

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
    params.hostname = aHostname;
    params.addr_type = (preferV6 || hasGlobalV6)?LWIP_DNS_ADDRTYPE_IPV6_IPV4:LWIP_DNS_ADDRTYPE_IPV4;
    params.result = 0;
    aResult.to_ip_addr_t(&(params.addr));

    if (!aResult.fromString(aHostname)) {
        Network.waitStatusBits(NET_DNS_IDLE_BIT, 16000);
        Network.clearStatusBits(NET_DNS_IDLE_BIT | NET_DNS_DONE_BIT);

        err = esp_netif_tcpip_exec(wifi_gethostbyname_tcpip_ctx, &params);
        if (err == ERR_OK) {
            aResult.from_ip_addr_t(&(params.addr));
        } else if (err == ERR_INPROGRESS) {
            Network.waitStatusBits(NET_DNS_DONE_BIT, 15000);  //real internal timeout in lwip library is 14[s]
            Network.clearStatusBits(NET_DNS_DONE_BIT);
            if (params.result == 1) {
                aResult.from_ip_addr_t(&(params.addr));
                err = ERR_OK;
            }
        }
        Network.setStatusBits(NET_DNS_IDLE_BIT);
    }
    if (err == ERR_OK) {
        return 1;
    }
    log_e("DNS Failed for '%s' with error '%d' and result '%d'", aHostname, err, params.result);
    return err;
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
