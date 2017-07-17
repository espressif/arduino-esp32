#include "Ethernet.h"

extern "C" {
	#include "WiFiGeneric.h"
	//void tcpipInit();
};


static eth_config_t eth_config = ETH_PHY_CONF;

static void ethernet_config_gpio(void){
    // RMII data pins are fixed:
    // CRS_DRV = GPIO27
    // TXD0 = GPIO19
    // TXD1 = GPIO22
    // TX_EN = GPIO21
    // RXD0 = GPIO25
    // RXD1 = GPIO26
    // CLK == GPIO0
    phy_rmii_configure_data_interface_pins();
    // MDC is GPIO 23, MDIO is GPIO 18
    phy_rmii_smi_configure_pins(PIN_SMI_MDC, PIN_SMI_MDIO);
}

void tcpipInit();

int EthernetClass::begin()
{
	eth_config.phy_addr = ETH_PHY_ADDR;
    eth_config.gpio_config = ethernet_config_gpio;
    eth_config.tcpip_input = tcpip_adapter_eth_input;
    //eth_config.phy_power_enable = ethernet_power_enable;
    tcpipInit();
    esp_err_t err = esp_eth_init(&eth_config);
    if(!err){
        err = esp_eth_enable();
		return 1;
    }
	return 0;
}

int EthernetClass::config(IPAddress local_ip, IPAddress subnet, IPAddress gateway)
{
	// stop the DHCP service
	if(tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_ETH) == ESP_OK) {
		// continue
	}
	else {
		return 0;
	}
	
	// create a new TCP/IP info 
	tcpip_adapter_ip_info_t info;
    info.ip.addr = static_cast<uint32_t>((local_ip));
    info.gw.addr = static_cast<uint32_t>((gateway));
    info.netmask.addr = static_cast<uint32_t>((subnet));
	// apply the IP settings
	if(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_ETH, &info) == ESP_OK) {
        // continue
    } else {
		return 0;
    }
	
	return 1;
}

int EthernetClass::begin(IPAddress local_ip, IPAddress subnet, IPAddress gateway)
{
	// connect the callback function
	esp_event_loop_init(&WiFiGenericClass::_eventCallback, NULL);
	
	// init the TCP/IP adapter
	tcpip_adapter_init();
	
	// manually configure the adapter
	if(!config(local_ip,subnet,gateway))
	{
		return 0;
	}
	
	eth_config.phy_addr = ETH_PHY_ADDR;
    eth_config.gpio_config = ethernet_config_gpio;
    eth_config.tcpip_input = tcpip_adapter_eth_input;
    //eth_config.phy_power_enable = ethernet_power_enable;
    esp_err_t err = esp_eth_init(&eth_config);
    if(!err){
        err = esp_eth_enable();
		return 1;
    }
	
	return 0;
}

IPAddress EthernetClass::localIP()
{
	tcpip_adapter_ip_info_t ip;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_ETH, &ip);
    return IPAddress(ip.ip.addr);
}

IPAddress EthernetClass::subnetMask()
{
    tcpip_adapter_ip_info_t ip;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_ETH, &ip);
    return IPAddress(ip.netmask.addr);

}

IPAddress EthernetClass::gatewayIP()
{
    tcpip_adapter_ip_info_t ip;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_ETH, &ip);
    return IPAddress(ip.gw.addr);
}

bool EthernetClass::setHostname(const char * hostname)
{
    return tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_ETH, hostname) == 0;
}

bool EthernetClass::isFullDuplex()
{
    return eth_config.phy_get_duplex_mode();
}

uint8_t EthernetClass::getLinkSpeed()
{
    return eth_config.phy_get_speed_mode()?100:10;
}

String EthernetClass::getMacAddress()
{
    uint8_t mac[6];
    char macStr[18] = { 0 };
    esp_eth_get_mac(mac);
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
}

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_ETHERNET)
EthernetClass Ethernet;
#endif
