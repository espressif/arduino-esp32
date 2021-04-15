/*
 ETH.h - espre ETH PHY support.
 Based on WiFi.h from Arduino WiFi shield library.
 Copyright (c) 2011-2014 Arduino.  All right reserved.

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
 */

#include "ETH.h"
#include "esp_system.h"
#ifdef ESP_IDF_VERSION_MAJOR
    #include "esp_event.h"
    #include "esp_eth.h"
    #include "esp_eth_phy.h"
    #include "esp_eth_mac.h"
    #include "esp_eth_com.h"
#else
    #include "eth_phy/phy.h"
    #include "eth_phy/phy_tlk110.h"
    #include "eth_phy/phy_lan8720.h"
#endif
#include "lwip/err.h"
#include "lwip/dns.h"

extern void tcpipInit();

#ifdef ESP_IDF_VERSION_MAJOR

/**
* @brief Callback function invoked when lowlevel initialization is finished
*
* @param[in] eth_handle: handle of Ethernet driver
*
* @return
*       - ESP_OK: process extra lowlevel initialization successfully
*       - ESP_FAIL: error occurred when processing extra lowlevel initialization
*/
//static esp_err_t on_lowlevel_init_done(esp_eth_handle_t eth_handle){
//#define PIN_PHY_POWER 2
//    pinMode(PIN_PHY_POWER, OUTPUT);
//    digitalWrite(PIN_PHY_POWER, HIGH);
//    delay(100);
//    return ESP_OK;
//}

/**
* @brief Callback function invoked when lowlevel deinitialization is finished
*
* @param[in] eth_handle: handle of Ethernet driver
*
* @return
*       - ESP_OK: process extra lowlevel deinitialization successfully
*       - ESP_FAIL: error occurred when processing extra lowlevel deinitialization
*/
//static esp_err_t on_lowlevel_deinit_done(esp_eth_handle_t eth_handle){
//    return ESP_OK;
//}



#else
static int _eth_phy_mdc_pin = -1;
static int _eth_phy_mdio_pin = -1;
static int _eth_phy_power_pin = -1;
static eth_phy_power_enable_func _eth_phy_power_enable_orig = NULL;

static void _eth_phy_config_gpio(void)
{
    if(_eth_phy_mdc_pin < 0 || _eth_phy_mdio_pin < 0){
        log_e("MDC and MDIO pins are not configured!");
        return;
    }
    phy_rmii_configure_data_interface_pins();
    phy_rmii_smi_configure_pins(_eth_phy_mdc_pin, _eth_phy_mdio_pin);
}

static void _eth_phy_power_enable(bool enable)
{
    pinMode(_eth_phy_power_pin, OUTPUT);
    digitalWrite(_eth_phy_power_pin, enable);
    delay(1);
}
#endif

ETHClass::ETHClass()
    :initialized(false)
    ,staticIP(false)
#if ESP_IDF_VERSION_MAJOR > 3
     ,eth_handle(NULL)
#endif
     ,started(false)
#if ESP_IDF_VERSION_MAJOR > 3
     ,eth_link(ETH_LINK_DOWN)
#endif
{
}

ETHClass::~ETHClass()
{}

#ifdef ESP_IDF_VERSION_MAJOR
bool ETHClass::begin(uint8_t phy_addr, int power, int mdc, int mdio, eth_phy_type_t type){

    tcpipInit();

    tcpip_adapter_set_default_eth_handlers();
    
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&cfg);

    if(esp_eth_set_default_handlers(eth_netif) != ESP_OK){
        log_e("esp_eth_set_default_handlers failed");
        return false;
    }
    
    
    esp_eth_mac_t *eth_mac = NULL;
#if CONFIG_ETH_SPI_ETHERNET_DM9051
    if(type == ETH_PHY_DM9051){
        return false;//todo
    } else {
#endif
#if CONFIG_ETH_USE_ESP32_EMAC
        eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
        mac_config.smi_mdc_gpio_num = mdc;
        mac_config.smi_mdio_gpio_num = mdio;
        //mac_config.sw_reset_timeout_ms = 1000;
        eth_mac = esp_eth_mac_new_esp32(&mac_config);
#endif
#if CONFIG_ETH_SPI_ETHERNET_DM9051
    }
#endif

    if(eth_mac == NULL){
        log_e("esp_eth_mac_new_esp32 failed");
        return false;
    }

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = phy_addr;
    phy_config.reset_gpio_num = power;
    esp_eth_phy_t *eth_phy = NULL;
    switch(type){
        case ETH_PHY_LAN8720:
            eth_phy = esp_eth_phy_new_lan8720(&phy_config);
            break;
        case ETH_PHY_TLK110:
            eth_phy = esp_eth_phy_new_ip101(&phy_config);
            break;
        case ETH_PHY_RTL8201:
            eth_phy = esp_eth_phy_new_rtl8201(&phy_config);
            break;
        case ETH_PHY_DP83848:
            eth_phy = esp_eth_phy_new_dp83848(&phy_config);
            break;
#if CONFIG_ETH_SPI_ETHERNET_DM9051
        case ETH_PHY_DM9051:
            eth_phy = esp_eth_phy_new_dm9051(&phy_config);
            break;
#endif
        case ETH_PHY_KSZ8081:
            eth_phy = esp_eth_phy_new_ksz8081(&phy_config);
        default:
            break;
    }
    if(eth_phy == NULL){
        log_e("esp_eth_phy_new failed");
        return false;
    }

    eth_handle = NULL;
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(eth_mac, eth_phy);
    //eth_config.on_lowlevel_init_done = on_lowlevel_init_done;
    //eth_config.on_lowlevel_deinit_done = on_lowlevel_deinit_done;
    if(esp_eth_driver_install(&eth_config, &eth_handle) != ESP_OK || eth_handle == NULL){
        log_e("esp_eth_driver_install failed");
        return false;
    }
    
    /* attach Ethernet driver to TCP/IP stack */
    if(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)) != ESP_OK){
        log_e("esp_netif_attach failed");
        return false;
    }

    if(esp_eth_start(eth_handle) != ESP_OK){
        log_e("esp_eth_start failed");
        return false;
    }

    return true;
}
#else
bool ETHClass::begin(uint8_t phy_addr, int power, int mdc, int mdio, eth_phy_type_t type, eth_clock_mode_t clock_mode)
{
    esp_err_t err;
    if(initialized){
        err = esp_eth_enable();
        if(err){
            log_e("esp_eth_enable error: %d", err);
            return false;
        }
        started = true;
        return true;
    }
    _eth_phy_mdc_pin = mdc;
    _eth_phy_mdio_pin = mdio;
    _eth_phy_power_pin = power;

    if(type == ETH_PHY_LAN8720){
        eth_config_t config = phy_lan8720_default_ethernet_config;
        memcpy(&eth_config, &config, sizeof(eth_config_t));
    } else if(type == ETH_PHY_TLK110){
        eth_config_t config = phy_tlk110_default_ethernet_config;
        memcpy(&eth_config, &config, sizeof(eth_config_t));
    } else if(type == ETH_PHY_IP101) {
      eth_config_t config = phy_ip101_default_ethernet_config;
      memcpy(&eth_config, &config, sizeof(eth_config_t));
    } else {
        log_e("Bad ETH_PHY type: %u", (uint8_t)type);
        return false;
    }

    eth_config.phy_addr = (eth_phy_base_t)phy_addr;
    eth_config.clock_mode = clock_mode;
    eth_config.gpio_config = _eth_phy_config_gpio;
    eth_config.tcpip_input = tcpip_adapter_eth_input;
    if(_eth_phy_power_pin >= 0){
        _eth_phy_power_enable_orig = eth_config.phy_power_enable;
        eth_config.phy_power_enable = _eth_phy_power_enable;
    }

    tcpipInit();
    err = esp_eth_init(&eth_config);
    if(!err){
        initialized = true;
        err = esp_eth_enable();
        if(err){
            log_e("esp_eth_enable error: %d", err);
        } else {
            started = true;
            return true;
        }
    } else {
        log_e("esp_eth_init error: %d", err);
    }
    return false;
}
#endif

bool ETHClass::config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1, IPAddress dns2)
{
    esp_err_t err = ESP_OK;
    tcpip_adapter_ip_info_t info;
	
    if(local_ip != (uint32_t)0x00000000 && local_ip != INADDR_NONE){
        info.ip.addr = static_cast<uint32_t>(local_ip);
        info.gw.addr = static_cast<uint32_t>(gateway);
        info.netmask.addr = static_cast<uint32_t>(subnet);
    } else {
        info.ip.addr = 0;
        info.gw.addr = 0;
        info.netmask.addr = 0;
	}

    err = tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_ETH);
    if(err != ESP_OK && err != ESP_ERR_TCPIP_ADAPTER_DHCP_ALREADY_STOPPED){
        log_e("DHCP could not be stopped! Error: %d", err);
        return false;
    }

    err = tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_ETH, &info);
    if(err != ERR_OK){
        log_e("STA IP could not be configured! Error: %d", err);
        return false;
}
    if(info.ip.addr){
        staticIP = true;
    } else {
        err = tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_ETH);
        if(err != ESP_OK && err != ESP_ERR_TCPIP_ADAPTER_DHCP_ALREADY_STARTED){
            log_w("DHCP could not be started! Error: %d", err);
            return false;
        }
        staticIP = false;
    }

    ip_addr_t d;
    d.type = IPADDR_TYPE_V4;

    if(dns1 != (uint32_t)0x00000000 && dns1 != INADDR_NONE) {
        // Set DNS1-Server
        d.u_addr.ip4.addr = static_cast<uint32_t>(dns1);
        dns_setserver(0, &d);
    }

    if(dns2 != (uint32_t)0x00000000 && dns2 != INADDR_NONE) {
        // Set DNS2-Server
        d.u_addr.ip4.addr = static_cast<uint32_t>(dns2);
        dns_setserver(1, &d);
    }

    return true;
}

IPAddress ETHClass::localIP()
{
    tcpip_adapter_ip_info_t ip;
    if(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_ETH, &ip)){
        return IPAddress();
    }
    return IPAddress(ip.ip.addr);
}

IPAddress ETHClass::subnetMask()
{
    tcpip_adapter_ip_info_t ip;
    if(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_ETH, &ip)){
        return IPAddress();
    }
    return IPAddress(ip.netmask.addr);
}

IPAddress ETHClass::gatewayIP()
{
    tcpip_adapter_ip_info_t ip;
    if(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_ETH, &ip)){
        return IPAddress();
    }
    return IPAddress(ip.gw.addr);
}

IPAddress ETHClass::dnsIP(uint8_t dns_no)
{
    const ip_addr_t * dns_ip = dns_getserver(dns_no);
    return IPAddress(dns_ip->u_addr.ip4.addr);
}

IPAddress ETHClass::broadcastIP()
{
    tcpip_adapter_ip_info_t ip;
    if(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_ETH, &ip)){
        return IPAddress();
    }
    return WiFiGenericClass::calculateBroadcast(IPAddress(ip.gw.addr), IPAddress(ip.netmask.addr));
}

IPAddress ETHClass::networkID()
{
    tcpip_adapter_ip_info_t ip;
    if(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_ETH, &ip)){
        return IPAddress();
    }
    return WiFiGenericClass::calculateNetworkID(IPAddress(ip.gw.addr), IPAddress(ip.netmask.addr));
}

uint8_t ETHClass::subnetCIDR()
{
    tcpip_adapter_ip_info_t ip;
    if(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_ETH, &ip)){
        return (uint8_t)0;
    }
    return WiFiGenericClass::calculateSubnetCIDR(IPAddress(ip.netmask.addr));
}

const char * ETHClass::getHostname()
{
    const char * hostname;
    if(tcpip_adapter_get_hostname(TCPIP_ADAPTER_IF_ETH, &hostname)){
        return NULL;
    }
    return hostname;
}

bool ETHClass::setHostname(const char * hostname)
{
    return tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_ETH, hostname) == 0;
}

bool ETHClass::fullDuplex()
{
#ifdef ESP_IDF_VERSION_MAJOR
    return true;//todo: do not see an API for this
#else
    return eth_config.phy_get_duplex_mode();
#endif
}

bool ETHClass::linkUp()
{
#ifdef ESP_IDF_VERSION_MAJOR
    return eth_link == ETH_LINK_UP;
#else
    return eth_config.phy_check_link();
#endif
}

uint8_t ETHClass::linkSpeed()
{
#ifdef ESP_IDF_VERSION_MAJOR
    eth_speed_t link_speed;
    esp_eth_ioctl(eth_handle, ETH_CMD_G_SPEED, &link_speed);
    return (link_speed == ETH_SPEED_10M)?10:100;
#else
    return eth_config.phy_get_speed_mode()?100:10;
#endif
}

bool ETHClass::enableIpV6()
{
    return tcpip_adapter_create_ip6_linklocal(TCPIP_ADAPTER_IF_ETH) == 0;
}

IPv6Address ETHClass::localIPv6()
{
    static ip6_addr_t addr;
    if(tcpip_adapter_get_ip6_linklocal(TCPIP_ADAPTER_IF_ETH, &addr)){
        return IPv6Address();
    }
    return IPv6Address(addr.addr);
}

uint8_t * ETHClass::macAddress(uint8_t* mac)
{
    if(!mac){
        return NULL;
    }
#ifdef ESP_IDF_VERSION_MAJOR
    esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac);
#else
    esp_eth_get_mac(mac);
#endif
    return mac;
}

String ETHClass::macAddress(void)
{
    uint8_t mac[6] = {0,0,0,0,0,0};
    char macStr[18] = { 0 };
    macAddress(mac);
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
}

ETHClass ETH;
