#include "ESP_Network_Interface.h"
#include "esp_netif.h"
#include "esp_netif_defaults.h"
#include "esp_system.h"
#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "lwip/netif.h"
#include "esp32-hal-log.h"

static ESP_Network_Interface * _interfaces[ESP_NETIF_ID_MAX] = { NULL, NULL, NULL, NULL, NULL, NULL};
static esp_event_handler_instance_t _ip_ev_instance = NULL;

static ESP_Network_Interface * getNetifByEspNetif(esp_netif_t *esp_netif){
    for (int i = 0; i < ESP_NETIF_ID_MAX; ++i){
        if(_interfaces[i] != NULL && _interfaces[i]->netif() == esp_netif){
            return _interfaces[i];
        }
    }
    return NULL;
}

// static ESP_Network_Interface * getNetifByID(ESP_Network_Interface_ID id){
//     if(id < ESP_NETIF_ID_MAX){
//         return _interfaces[id];
//     }
//     return NULL;
// }

static void _ip_event_cb(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == IP_EVENT){
        ESP_Network_Interface * netif = NULL;
        if(event_id == IP_EVENT_STA_GOT_IP || event_id == IP_EVENT_ETH_GOT_IP || event_id == IP_EVENT_PPP_GOT_IP){
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            netif = getNetifByEspNetif(event->esp_netif);
        } else if(event_id == IP_EVENT_STA_LOST_IP || event_id == IP_EVENT_PPP_LOST_IP || event_id == IP_EVENT_ETH_LOST_IP){
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            netif = getNetifByEspNetif(event->esp_netif);
        } else if(event_id == IP_EVENT_GOT_IP6){
            ip_event_got_ip6_t* event = (ip_event_got_ip6_t*) event_data;
            netif = getNetifByEspNetif(event->esp_netif);
        } else if(event_id == IP_EVENT_AP_STAIPASSIGNED){
            ip_event_ap_staipassigned_t* event = (ip_event_ap_staipassigned_t*) event_data;
            netif = getNetifByEspNetif(event->esp_netif);
        }
        if(netif != NULL){
            netif->_onIpEvent(event_id, event_data);
        }
    }
}

void ESP_Network_Interface::_onIpEvent(int32_t event_id, void* event_data){
    arduino_event_t arduino_event;
    arduino_event.event_id = ARDUINO_EVENT_MAX;
    if(event_id == _got_ip_event_id){
        _has_ip = true;
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        log_v("%s Got %sIP: " IPSTR " MASK: " IPSTR " GW: " IPSTR, desc(), event->ip_changed?"New ":"Same ", IP2STR(&event->ip_info.ip), IP2STR(&event->ip_info.netmask), IP2STR(&event->ip_info.gw));
#endif
        memcpy(&arduino_event.event_info.got_ip, event_data, sizeof(ip_event_got_ip_t));
#if SOC_WIFI_SUPPORTED
        if(_interface_id == ESP_NETIF_ID_STA){
            arduino_event.event_id = ARDUINO_EVENT_WIFI_STA_GOT_IP;
            Network.setStatusBits(STA_HAS_IP_BIT);
        } else
#endif
        // if(_interface_id == ESP_NETIF_ID_PPP){
        //     arduino_event.event_id = ARDUINO_EVENT_PPP_GOT_IP;
        //     Network.setStatusBits(PPP_HAS_IP_BIT);
        // } else 
        if(_interface_id >= ESP_NETIF_ID_ETH && _interface_id < ESP_NETIF_ID_MAX){
            arduino_event.event_id = ARDUINO_EVENT_ETH_GOT_IP;
            Network.setStatusBits(ETH_HAS_IP_BIT(_interface_id - ESP_NETIF_ID_ETH));
        }
    } else if(event_id == _lost_ip_event_id){
        _has_ip = false;
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
        log_v("%s Lost IP", desc());
#endif
#if SOC_WIFI_SUPPORTED
        if(_interface_id == ESP_NETIF_ID_STA){
            arduino_event.event_id = ARDUINO_EVENT_WIFI_STA_LOST_IP;
            Network.clearStatusBits(STA_HAS_IP_BIT);
        } else 
#endif
        // if(_interface_id == ESP_NETIF_ID_PPP){
        //     arduino_event.event_id = ARDUINO_EVENT_PPP_LOST_IP;
        //     Network.clearStatusBits(PPP_HAS_IP_BIT);
        // } else 
        if(_interface_id >= ESP_NETIF_ID_ETH && _interface_id < ESP_NETIF_ID_MAX){
            arduino_event.event_id = ARDUINO_EVENT_ETH_LOST_IP;
            Network.clearStatusBits(ETH_HAS_IP_BIT(_interface_id - ESP_NETIF_ID_ETH));
        }
    } else if(event_id == IP_EVENT_GOT_IP6){
        ip_event_got_ip6_t* event = (ip_event_got_ip6_t*) event_data;
        esp_ip6_addr_type_t addr_type = esp_netif_ip6_get_addr_type(&event->ip6_info.ip);
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE        
        char if_name[NETIF_NAMESIZE] = {0,};
        netif_index_to_name(event->ip6_info.ip.zone, if_name);
        static const char * addr_types[] = { "UNKNOWN", "GLOBAL", "LINK_LOCAL", "SITE_LOCAL", "UNIQUE_LOCAL", "IPV4_MAPPED_IPV6" };
        log_v("IF %s Got IPv6: Interface: %d, IP Index: %d, Type: %s, Zone: %d (%s), Address: " IPV6STR, desc(), _interface_id, event->ip_index, addr_types[addr_type], event->ip6_info.ip.zone, if_name, IPV62STR(event->ip6_info.ip));
#endif
        memcpy(&arduino_event.event_info.got_ip6, event_data, sizeof(ip_event_got_ip6_t));
#if SOC_WIFI_SUPPORTED
        if(_interface_id == ESP_NETIF_ID_STA){
            arduino_event.event_id = ARDUINO_EVENT_WIFI_STA_GOT_IP6;
            if(addr_type == ESP_IP6_ADDR_IS_GLOBAL){
                Network.setStatusBits(STA_HAS_IP6_GLOBAL_BIT);
            } else if(addr_type == ESP_IP6_ADDR_IS_LINK_LOCAL){
                Network.setStatusBits(STA_HAS_IP6_BIT);
            }
        } else if(_interface_id == ESP_NETIF_ID_AP){
            arduino_event.event_id = ARDUINO_EVENT_WIFI_AP_GOT_IP6;
            if(addr_type == ESP_IP6_ADDR_IS_LINK_LOCAL){
                Network.setStatusBits(AP_HAS_IP6_BIT);
            }
        } else 
#endif
        // if(_interface_id == ESP_NETIF_ID_PPP){
        //     arduino_event.event_id = ARDUINO_EVENT_PPP_GOT_IP6;
        //     Network.setStatusBits(PPP_HAS_IP6_BIT);
        // } else 
        if(_interface_id >= ESP_NETIF_ID_ETH && _interface_id < ESP_NETIF_ID_MAX){
            arduino_event.event_id = ARDUINO_EVENT_ETH_GOT_IP6;
            if(addr_type == ESP_IP6_ADDR_IS_GLOBAL){
                Network.setStatusBits(ETH_HAS_IP6_GLOBAL_BIT(_interface_id - ESP_NETIF_ID_ETH));
            } else if(addr_type == ESP_IP6_ADDR_IS_LINK_LOCAL){
                Network.setStatusBits(ETH_HAS_IP6_BIT(_interface_id - ESP_NETIF_ID_ETH));
            }
        }
#if SOC_WIFI_SUPPORTED
    } else if(event_id == IP_EVENT_AP_STAIPASSIGNED && _interface_id == ESP_NETIF_ID_AP){
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
        ip_event_ap_staipassigned_t* event = (ip_event_ap_staipassigned_t*) event_data;
        log_v("%s Assigned IP: " IPSTR " to MAC: %02X:%02X:%02X:%02X:%02X:%02X", desc(), IP2STR(&event->ip), event->mac[0], event->mac[1], event->mac[2], event->mac[3], event->mac[4], event->mac[5]);
#endif
        arduino_event.event_id = ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED;
        memcpy(&arduino_event.event_info.wifi_ap_staipassigned, event_data, sizeof(ip_event_ap_staipassigned_t));
#endif
    }
    
    if(arduino_event.event_id < ARDUINO_EVENT_MAX){
        Network.postEvent(&arduino_event);
    }
}

ESP_Network_Interface::ESP_Network_Interface()
    : _esp_netif(NULL)
    , _got_ip_event_id(-1)
    , _lost_ip_event_id(-1)
    , _interface_id(ESP_NETIF_ID_MAX)
    , _has_ip(false)
{}

ESP_Network_Interface::~ESP_Network_Interface(){
    destroyNetif();
}

bool ESP_Network_Interface::hasIP(){
    return _has_ip;
}

IPAddress ESP_Network_Interface::calculateNetworkID(IPAddress ip, IPAddress subnet) {
	IPAddress networkID;

	for (size_t i = 0; i < 4; i++)
		networkID[i] = subnet[i] & ip[i];

	return networkID;
}

IPAddress ESP_Network_Interface::calculateBroadcast(IPAddress ip, IPAddress subnet) {
    IPAddress broadcastIp;
    
    for (int i = 0; i < 4; i++)
        broadcastIp[i] = ~subnet[i] | ip[i];

    return broadcastIp;
}

uint8_t ESP_Network_Interface::calculateSubnetCIDR(IPAddress subnetMask) {
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

void ESP_Network_Interface::destroyNetif(){
    if(_esp_netif != NULL){
        esp_netif_destroy(_esp_netif);
        _esp_netif = NULL;
    }
}

bool ESP_Network_Interface::initNetif(ESP_Network_Interface_ID interface_id){
    if(_esp_netif == NULL || interface_id >= ESP_NETIF_ID_MAX){
        return false;
    }
    _interface_id = interface_id;
    _got_ip_event_id = esp_netif_get_event_id(_esp_netif, ESP_NETIF_IP_EVENT_GOT_IP);
    _lost_ip_event_id = esp_netif_get_event_id(_esp_netif, ESP_NETIF_IP_EVENT_LOST_IP);
    _interfaces[_interface_id] = this;

    if(_ip_ev_instance == NULL && esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &_ip_event_cb, NULL, &_ip_ev_instance)){
        log_e("event_handler_instance_register for IP_EVENT Failed!");
        return false;
    }
    return true;
}

bool ESP_Network_Interface::config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1, IPAddress dns2, IPAddress dns3)
{
    if(_esp_netif == NULL){
        return false;
    }
    esp_err_t err = ESP_OK;
    esp_netif_ip_info_t info;
    esp_netif_dns_info_t d1;
    esp_netif_dns_info_t d2;
    esp_netif_dns_info_t d3;
    d1.ip.type = IPADDR_TYPE_V4;
    d2.ip.type = IPADDR_TYPE_V4;
    d3.ip.type = IPADDR_TYPE_V4;

    if(static_cast<uint32_t>(local_ip) != 0){
        info.ip.addr = static_cast<uint32_t>(local_ip);
        info.gw.addr = static_cast<uint32_t>(gateway);
        info.netmask.addr = static_cast<uint32_t>(subnet);
        d1.ip.u_addr.ip4.addr = static_cast<uint32_t>(dns1);
        d2.ip.u_addr.ip4.addr = static_cast<uint32_t>(dns2);
        d3.ip.u_addr.ip4.addr = static_cast<uint32_t>(dns3);
    } else {
        info.ip.addr = 0;
        info.gw.addr = 0;
        info.netmask.addr = 0;
        d1.ip.u_addr.ip4.addr = 0;
        d2.ip.u_addr.ip4.addr = 0;
        d3.ip.u_addr.ip4.addr = 0;
	}

    // Stop DHCPC
    err = esp_netif_dhcpc_stop(_esp_netif);
    if(err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED){
        log_e("DHCP could not be stopped! Error: %d", err);
        return false;
    }

    // Set IPv4, Netmask, Gateway
    err = esp_netif_set_ip_info(_esp_netif, &info);
    if(err != ERR_OK){
        log_e("ETH IP could not be configured! Error: %d", err);
        return false;
    }
    
    // Set DNS Servers
    esp_netif_set_dns_info(_esp_netif, ESP_NETIF_DNS_MAIN, &d1);
    esp_netif_set_dns_info(_esp_netif, ESP_NETIF_DNS_BACKUP, &d2);
    esp_netif_set_dns_info(_esp_netif, ESP_NETIF_DNS_FALLBACK, &d3);

    // Start DHCPC if static IP was set
    if(info.ip.addr == 0){
        err = esp_netif_dhcpc_start(_esp_netif);
        if(err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED){
            log_w("DHCP could not be started! Error: %d", err);
            return false;
        }
    } else {
        _has_ip = true;
    }

    return true;
}

const char * ESP_Network_Interface::getHostname()
{
    if(_esp_netif == NULL){
        return "";
    }
    const char * hostname;
    if(esp_netif_get_hostname(_esp_netif, &hostname)){
        return NULL;
    }
    return hostname;
}

bool ESP_Network_Interface::setHostname(const char * hostname)
{
    if(_esp_netif == NULL){
        return false;
    }
    return esp_netif_set_hostname(_esp_netif, hostname) == 0;
}

bool ESP_Network_Interface::linkUp()
{
    if(_esp_netif == NULL){
        return false;
    }
    return esp_netif_is_netif_up(_esp_netif);
}

const char * ESP_Network_Interface::ifkey(void)
{
    if(_esp_netif == NULL){
        return "";
    }
    return esp_netif_get_ifkey(_esp_netif);
}

const char * ESP_Network_Interface::desc(void)
{
    if(_esp_netif == NULL){
        return "";
    }
    return esp_netif_get_desc(_esp_netif);
}

String ESP_Network_Interface::impl_name(void)
{
    if(_esp_netif == NULL){
        return String("");
    }
    char netif_name[8];
    esp_err_t err = esp_netif_get_netif_impl_name(_esp_netif, netif_name);
    if(err != ESP_OK){
        log_e("Failed to get netif impl_name: %d", err);
        return String("");
    }
    return String(netif_name);
}

uint8_t * ESP_Network_Interface::macAddress(uint8_t* mac)
{
    if(!mac || _esp_netif == NULL){
        return NULL;
    }
    esp_err_t err = esp_netif_get_mac(_esp_netif, mac);
    if(err != ESP_OK){
        log_e("Failed to get netif mac: %d", err);
        return NULL;
    }
    // getMac(mac);
    return mac;
}

String ESP_Network_Interface::macAddress(void)
{
    uint8_t mac[6] = {0,0,0,0,0,0};
    char macStr[18] = { 0 };
    macAddress(mac);
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
}

IPAddress ESP_Network_Interface::localIP()
{
    if(_esp_netif == NULL){
        return IPAddress();
    }
    esp_netif_ip_info_t ip;
    if(esp_netif_get_ip_info(_esp_netif, &ip)){
        return IPAddress();
    }
    return IPAddress(ip.ip.addr);
}

IPAddress ESP_Network_Interface::subnetMask()
{
    if(_esp_netif == NULL){
        return IPAddress();
    }
    esp_netif_ip_info_t ip;
    if(esp_netif_get_ip_info(_esp_netif, &ip)){
        return IPAddress();
    }
    return IPAddress(ip.netmask.addr);
}

IPAddress ESP_Network_Interface::gatewayIP()
{
    if(_esp_netif == NULL){
        return IPAddress();
    }
    esp_netif_ip_info_t ip;
    if(esp_netif_get_ip_info(_esp_netif, &ip)){
        return IPAddress();
    }
    return IPAddress(ip.gw.addr);
}

IPAddress ESP_Network_Interface::dnsIP(uint8_t dns_no)
{
    if(_esp_netif == NULL){
        return IPAddress();
    }
    esp_netif_dns_info_t d;
    if(esp_netif_get_dns_info(_esp_netif, dns_no?ESP_NETIF_DNS_BACKUP:ESP_NETIF_DNS_MAIN, &d) != ESP_OK){
        return IPAddress();
    }
    return IPAddress(d.ip.u_addr.ip4.addr);
}

IPAddress ESP_Network_Interface::broadcastIP()
{
    if(_esp_netif == NULL){
        return IPAddress();
    }
    esp_netif_ip_info_t ip;
    if(esp_netif_get_ip_info(_esp_netif, &ip)){
        return IPAddress();
    }
    return calculateBroadcast(IPAddress(ip.gw.addr), IPAddress(ip.netmask.addr));
}

IPAddress ESP_Network_Interface::networkID()
{
    if(_esp_netif == NULL){
        return IPAddress();
    }
    esp_netif_ip_info_t ip;
    if(esp_netif_get_ip_info(_esp_netif, &ip)){
        return IPAddress();
    }
    return calculateNetworkID(IPAddress(ip.gw.addr), IPAddress(ip.netmask.addr));
}

uint8_t ESP_Network_Interface::subnetCIDR()
{
    if(_esp_netif == NULL){
        return (uint8_t)0;
    }
    esp_netif_ip_info_t ip;
    if(esp_netif_get_ip_info(_esp_netif, &ip)){
        return (uint8_t)0;
    }
    return calculateSubnetCIDR(IPAddress(ip.netmask.addr));
}

IPAddress ESP_Network_Interface::localIPv6()
{
    if(_esp_netif == NULL){
        return IPAddress(IPv6);
    }
    static esp_ip6_addr_t addr;
    if(esp_netif_get_ip6_linklocal(_esp_netif, &addr)){
        return IPAddress(IPv6);
    }
    return IPAddress(IPv6, (const uint8_t *)addr.addr, addr.zone);
}

IPAddress ESP_Network_Interface::globalIPv6()
{
    if(_esp_netif == NULL){
        return IPAddress(IPv6);
    }
    static esp_ip6_addr_t addr;
    if(esp_netif_get_ip6_global(_esp_netif, &addr)){
        return IPAddress(IPv6);
    }
    return IPAddress(IPv6, (const uint8_t *)addr.addr, addr.zone);
}

void ESP_Network_Interface::printInfo(Print & out){
    out.print(desc());
    out.print(":");
    if(linkUp()){
        out.print(" <UP");
    } else {
        out.print(" <DOWN");
    }
    printDriverInfo(out);
    out.println(">");

    out.print("      ");
    out.print("ether ");
    out.print(macAddress());
    out.println();

    out.print("      ");
    out.print("inet ");
    out.print(localIP());
    out.print(" netmask ");
    out.print(subnetMask());
    out.print(" broadcast ");
    out.print(broadcastIP());
    out.println();

    out.print("      ");
    out.print("gateway ");
    out.print(gatewayIP());
    out.print(" dns ");
    out.print(dnsIP());
    out.println();

    static const char * types[] = { "UNKNOWN", "GLOBAL", "LINK_LOCAL", "SITE_LOCAL", "UNIQUE_LOCAL", "IPV4_MAPPED_IPV6" };
    esp_ip6_addr_t if_ip6[CONFIG_LWIP_IPV6_NUM_ADDRESSES];
    int v6addrs = esp_netif_get_all_ip6(_esp_netif, if_ip6);
    for (int i = 0; i < v6addrs; ++i){
        out.print("      ");
        out.print("inet6 ");
        IPAddress(IPv6, (const uint8_t *)if_ip6[i].addr, if_ip6[i].zone).printTo(out, true);
        out.print(" type ");
        out.print(types[esp_netif_ip6_get_addr_type(&if_ip6[i])]);
        out.println();
    }

    out.println();
}
