/*
 ETH.h - espre ETH PHY support.
 Based on WiFi.h from Ardiono WiFi shield library.
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

#ifndef _ETH_H_
#define _ETH_H_

#include "WiFi.h"
#include "esp_system.h"
#include "esp_eth.h"

#ifndef ETH_PHY_ADDR
#define ETH_PHY_ADDR 0
#endif

#ifndef ETH_PHY_TYPE
#define ETH_PHY_TYPE ETH_PHY_LAN8720
#endif

#ifndef ETH_PHY_POWER
#define ETH_PHY_POWER -1
#endif

#ifndef ETH_PHY_MDC
#define ETH_PHY_MDC 23
#endif

#ifndef ETH_PHY_MDIO
#define ETH_PHY_MDIO 18
#endif

#ifndef ETH_CLK_MODE
#define ETH_CLK_MODE ETH_CLOCK_GPIO0_IN
#endif

#if ESP_IDF_VERSION_MAJOR > 3
typedef enum { ETH_CLOCK_GPIO0_IN, ETH_CLOCK_GPIO0_OUT, ETH_CLOCK_GPIO16_OUT, ETH_CLOCK_GPIO17_OUT } eth_clock_mode_t;
#endif

typedef enum { ETH_PHY_LAN8720, ETH_PHY_TLK110, ETH_PHY_RTL8201, ETH_PHY_DP83848, ETH_PHY_DM9051, ETH_PHY_KSZ8041, ETH_PHY_KSZ8081, ETH_PHY_MAX } eth_phy_type_t;
#define ETH_PHY_IP101 ETH_PHY_TLK110

class ETHClass {
    private:
        bool initialized;
        bool staticIP;
#if ESP_IDF_VERSION_MAJOR > 3
        esp_eth_handle_t eth_handle;

    protected:
        bool started;
        static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
#else
        bool started;
        eth_config_t eth_config;
#endif
    public:
        ETHClass();
        ~ETHClass();

        bool begin(uint8_t phy_addr=ETH_PHY_ADDR, int power=ETH_PHY_POWER, int mdc=ETH_PHY_MDC, int mdio=ETH_PHY_MDIO, eth_phy_type_t type=ETH_PHY_TYPE, eth_clock_mode_t clk_mode=ETH_CLK_MODE, bool use_mac_from_efuse=false);

        bool config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1 = (uint32_t)0x00000000, IPAddress dns2 = (uint32_t)0x00000000);

        const char * getHostname();
        bool setHostname(const char * hostname);

        bool fullDuplex();
        bool linkUp();
        uint8_t linkSpeed();

        bool enableIpV6();
        IPv6Address localIPv6();

        IPAddress localIP();
        IPAddress subnetMask();
        IPAddress gatewayIP();
        IPAddress dnsIP(uint8_t dns_no = 0);

        IPAddress broadcastIP();
        IPAddress networkID();
        uint8_t subnetCIDR();

        uint8_t * macAddress(uint8_t* mac);
        String macAddress();

        friend class WiFiClient;
        friend class WiFiServer;
};

extern ETHClass ETH;

#endif /* _ETH_H_ */
