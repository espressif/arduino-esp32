/*
 EthernetCompat.h - Arduino Ethernet API compatibility wrapper for esp32 ETH.
 Based on Ethernet.h from Arduino Ethernet shield library.
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

#ifndef _ETHERNET_COMPAT_H_
#define _ETHERNET_COMPAT_H_

#include "ETH.h"

#define ETH_PIN_IRQ 4

enum EthernetLinkStatus {
  Unknown, LinkON, LinkOFF
};

enum EthernetHardwareStatus {
  EthernetNoHardware, EthernetHardwareFound
};

class EthernetClass {

public:
  EthernetClass(ETHClass &ETH);

#if CONFIG_ETH_USE_ESP32_EMAC
  void init(eth_phy_type_t type, int32_t phy_addr, int mdc, int mdio, int power, eth_clock_mode_t clk_mode);
#endif /* CONFIG_ETH_USE_ESP32_EMAC */
#if ETH_SPI_SUPPORTS_CUSTOM
  void init(eth_phy_type_t type, int32_t phy_addr = ETH_PHY_ADDR_AUTO, int cs = SS, int irq = ETH_PIN_IRQ, int rst = -1,
      SPIClass &spi = SPI, int sck = -1, int miso = -1, int mosi = -1,
      uint8_t spi_freq_mhz = ETH_PHY_SPI_FREQ_MHZ);
#endif
  void init(eth_phy_type_t type, int32_t phy_addr, int cs, int irq, int rst,
      spi_host_device_t spi_host, int sck, int miso, int mosi, uint8_t spi_freq_mhz = ETH_PHY_SPI_FREQ_MHZ);

  int begin(uint8_t *mac, unsigned long timeout = 60000);
  void begin(uint8_t *mac, IPAddress ip, IPAddress dns = INADDR_NONE, IPAddress gateway = INADDR_NONE, IPAddress subnet = INADDR_NONE);

  int begin(unsigned long timeout = 60000);
  void begin(IPAddress ip, IPAddress dns = INADDR_NONE, IPAddress gateway = INADDR_NONE, IPAddress subnet = INADDR_NONE);

  int maintain();

  EthernetLinkStatus linkStatus();
  EthernetHardwareStatus hardwareStatus();

  void setHostname(const char *hostname);

  void MACAddress(uint8_t *mac_address); // legacy API
  uint8_t* macAddress(uint8_t *mac);

  IPAddress localIP();
  IPAddress subnetMask();
  IPAddress gatewayIP();
  IPAddress dnsServerIP(); // legacy API
  IPAddress dnsIP(int n = 0);

  void setDnsServerIP(const IPAddress dns_server); // legacy API
  void setDNS(IPAddress dns_server, IPAddress dns2 = INADDR_NONE);

  int hostByName(const char *hostname, IPAddress &result);

private:
  ETHClass& _ETH;

#if CONFIG_ETH_SPI_ETHERNET_W5500
  eth_phy_type_t _phy_type = ETH_PHY_W5500;
#else
  eth_phy_type_t _phy_type = ETH_PHY_MAX;
#endif
  int32_t _phy_addr = ETH_PHY_ADDR_AUTO;
#if CONFIG_ETH_USE_ESP32_EMAC
  int8_t _pin_mdc = -1;
  int8_t _pin_mdio = -1;
  int8_t _pin_power = -1;
  eth_clock_mode_t _rmii_clk_mode = ETH_CLOCK_GPIO0_IN;
#endif /* CONFIG_ETH_USE_ESP32_EMAC */

#if ETH_SPI_SUPPORTS_CUSTOM
  SPIClass* _spi = nullptr;
#endif
  spi_host_device_t _spi_host_device = SPI_HOST_MAX;
  int8_t _pin_cs = SS;
  int8_t _pin_irq = ETH_PIN_IRQ;
  int8_t _pin_rst = -1;
  int8_t _pin_sck = -1;
  int8_t _pin_miso = -1;
  int8_t _pin_mosi = -1;
  uint8_t _spi_freq_mhz = ETH_PHY_SPI_FREQ_MHZ;

  EthernetHardwareStatus _hardwareStatus = EthernetNoHardware;

  bool beginETH(uint8_t *mac);
};

extern EthernetClass Ethernet;

#define EthernetUDP NetworkUDP
#define EthernetServer NetworkServer
#define EthernetClient NetworkClient

#endif /* _ETHERNET_COMPAT_H_ */
