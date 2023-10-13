#include "ESP_Network_Interface.h"
#include "esp_netif.h"
#include "esp_netif_defaults.h"
#include "esp_system.h"
#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "esp32-hal-log.h"

ESP_Network_Interface::ESP_Network_Interface():_esp_netif(NULL){}
ESP_Network_Interface::ESP_Network_Interface(ESP_Network_Interface & _if){ _esp_netif = _if.netif(); }
ESP_Network_Interface::~ESP_Network_Interface(){ destroyNetif(); }

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

void ESP_Network_Interface::destroyNetif(void){
    if(_esp_netif != NULL){
        esp_netif_destroy(_esp_netif);
        _esp_netif = NULL;
    }
}

bool ESP_Network_Interface::config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1, IPAddress dns2)
{
    if(_esp_netif == NULL){
        return false;
    }
    esp_err_t err = ESP_OK;
    esp_netif_ip_info_t info;
    esp_netif_dns_info_t d1;
    esp_netif_dns_info_t d2;
    d1.ip.type = IPADDR_TYPE_V4;
    d2.ip.type = IPADDR_TYPE_V4;

    if(static_cast<uint32_t>(local_ip) != 0){
        info.ip.addr = static_cast<uint32_t>(local_ip);
        info.gw.addr = static_cast<uint32_t>(gateway);
        info.netmask.addr = static_cast<uint32_t>(subnet);
        d1.ip.u_addr.ip4.addr = static_cast<uint32_t>(dns1);
        d2.ip.u_addr.ip4.addr = static_cast<uint32_t>(dns2);
    } else {
        info.ip.addr = 0;
        info.gw.addr = 0;
        info.netmask.addr = 0;
        d1.ip.u_addr.ip4.addr = 0;
        d2.ip.u_addr.ip4.addr = 0;
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
    
    // Set DNS1-Server
    esp_netif_set_dns_info(_esp_netif, ESP_NETIF_DNS_MAIN, &d1);

    // Set DNS2-Server
    esp_netif_set_dns_info(_esp_netif, ESP_NETIF_DNS_BACKUP, &d2);

    // Start DHCPC if static IP was set
    if(info.ip.addr == 0){
        err = esp_netif_dhcpc_start(_esp_netif);
        if(err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED){
            log_w("DHCP could not be started! Error: %d", err);
            return false;
        }
    }

    return true;
}

bool ESP_Network_Interface::enableIpV6()
{
    if(_esp_netif == NULL){
        return false;
    }
    return esp_netif_create_ip6_linklocal(_esp_netif) == 0;
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
    if(!mac){
        return NULL;
    }
    getMac(mac);
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

IPv6Address ESP_Network_Interface::localIPv6()
{
    if(_esp_netif == NULL){
        return IPv6Address();
    }
    static esp_ip6_addr_t addr;
    if(esp_netif_get_ip6_linklocal(_esp_netif, &addr)){
        return IPv6Address();
    }
    return IPv6Address(addr.addr);
}
