#pragma once

#include "esp_netif_types.h"
#include "esp_event.h"
#include "Arduino.h"
#include "IPv6Address.h"
#include "ESP_Network_Manager.h"

typedef enum {
    ESP_NETIF_ID_STA,
    ESP_NETIF_ID_AP,
    ESP_NETIF_ID_PPP,
    ESP_NETIF_ID_ETH0,
    ESP_NETIF_ID_ETH1,
    ESP_NETIF_ID_ETH2,
    ESP_NETIF_ID_MAX
} ESP_Network_Interface_ID;

#define ESP_NETIF_ID_ETH ESP_NETIF_ID_ETH0

class ESP_Network_Interface {
    public:
        ESP_Network_Interface();
        virtual ~ESP_Network_Interface();

        bool config(IPAddress local_ip = (uint32_t)0x00000000, IPAddress gateway = (uint32_t)0x00000000, IPAddress subnet = (uint32_t)0x00000000, IPAddress dns1 = (uint32_t)0x00000000, IPAddress dns2 = (uint32_t)0x00000000, IPAddress dns3 = (uint32_t)0x00000000);
        bool enableIpV6();

        const char * getHostname();
        bool setHostname(const char * hostname);

        virtual bool started() = 0;
        virtual bool connected() = 0;
        bool linkUp();
        bool hasIP();
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

        void printInfo(Print & out);

        esp_netif_t * netif(){ return _esp_netif; }

    protected:
        esp_netif_t *_esp_netif;
        int32_t _got_ip_event_id;
        int32_t _lost_ip_event_id;
        ESP_Network_Interface_ID _interface_id;
        bool _has_ip;

        bool initNetif(ESP_Network_Interface_ID interface_id);
        void destroyNetif();
        // virtual void getMac(uint8_t* mac) = 0;
        virtual void printDriverInfo(Print & out) = 0;

    public:
        void _onIpEvent(int32_t event_id, void* event_data);

    private:
    	IPAddress calculateNetworkID(IPAddress ip, IPAddress subnet);
    	IPAddress calculateBroadcast(IPAddress ip, IPAddress subnet);
    	uint8_t calculateSubnetCIDR(IPAddress subnetMask);
};
