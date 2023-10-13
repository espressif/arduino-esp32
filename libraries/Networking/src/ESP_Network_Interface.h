#pragma once

#include "esp_netif_types.h"
#include "Arduino.h"
#include "IPv6Address.h"

class ESP_Network_Interface {
    public:
        ESP_Network_Interface();
        ESP_Network_Interface(ESP_Network_Interface & _if);
        virtual ~ESP_Network_Interface();

        bool config(IPAddress local_ip = (uint32_t)0x00000000, IPAddress gateway = (uint32_t)0x00000000, IPAddress subnet = (uint32_t)0x00000000, IPAddress dns1 = (uint32_t)0x00000000, IPAddress dns2 = (uint32_t)0x00000000);
        bool enableIpV6();

        const char * getHostname();
        bool setHostname(const char * hostname);

        bool linkUp();
        const char * ifkey();
        const char * desc();
        String impl_name();

        uint8_t * macAddress(uint8_t* mac);
        String macAddress();
        IPAddress localIP();
        IPAddress subnetMask();
        IPAddress gatewayIP();
        IPAddress dnsIP(uint8_t dns_no = 0);
        IPAddress broadcastIP();
        IPAddress networkID();
        uint8_t subnetCIDR();
        IPv6Address localIPv6();

        esp_netif_t * netif(){ return _esp_netif; }

    protected:
        esp_netif_t *_esp_netif;

        void destroyNetif();
        virtual void getMac(uint8_t* mac) = 0;

    private:
    	IPAddress calculateNetworkID(IPAddress ip, IPAddress subnet);
    	IPAddress calculateBroadcast(IPAddress ip, IPAddress subnet);
    	uint8_t calculateSubnetCIDR(IPAddress subnetMask);
};
