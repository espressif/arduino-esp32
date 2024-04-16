/*
 EthernetCompat.cpp - Arduino Ethernet API compatibility wrapper for esp32 ETH.
 Based on Ethernet.h from Arduino Ethernet shield library.

 This library is free software { you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation { either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY { without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library { if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "EthernetCompat.h"

EthernetClass::EthernetClass(ETHClass &ETH) :
    _ETH(ETH) {

}

#if CONFIG_ETH_USE_ESP32_EMAC
void EthernetClass::init(eth_phy_type_t type, int32_t phy_addr, int mdc, int mdio, int power, eth_clock_mode_t clk_mode) {
  _phy_type = type;
  _phy_addr = phy_addr;
  _pin_mdc = mdc;
  _pin_mdio = mdio;
  _pin_power = power;
  _rmii_clk_mode = clk_mode;
}
#endif /* CONFIG_ETH_USE_ESP32_EMAC */

#if ETH_SPI_SUPPORTS_CUSTOM
void EthernetClass::init(eth_phy_type_t type, int32_t phy_addr, int cs, int irq, int rst, SPIClass &spi, int sck, int miso, int mosi, uint8_t spi_freq_mhz) {
  _spi = &spi;
  init(type, phy_addr, cs, irq, rst, SPI_HOST_MAX, sck, miso, mosi, spi_freq_mhz);
}
#endif

void EthernetClass::init(eth_phy_type_t type, int32_t phy_addr, int cs, int irq, int rst, spi_host_device_t spi_host, int sck, int miso, int mosi, uint8_t spi_freq_mhz) {
  _phy_type = type;
  _phy_addr = phy_addr;
  _pin_cs = cs;
  _pin_irq = irq;
  _pin_rst = rst;
  _spi_host_device = spi_host;
  _pin_sck = sck;
  _pin_miso = miso;
  _pin_mosi = mosi;
  _spi_freq_mhz = spi_freq_mhz;
}

int EthernetClass::begin(uint8_t *mac, unsigned long timeout) {
  if (_ETH.netif() != NULL) {
    _ETH.config(INADDR_NONE);
  }
  if (beginETH(mac)) {
    _hardwareStatus = EthernetHardwareFound;
    if (timeout) {
      const unsigned long start = millis();
      while (!_ETH.hasIP() && ((millis() - start) < timeout)) {
        delay(10);
      }
    }
  }
  return _ETH.hasIP();
}

void EthernetClass::begin(uint8_t *mac, IPAddress local_ip, IPAddress dns_ip, IPAddress gateway_ip, IPAddress netmask) {

  if (local_ip.type() == IPv4) {
    // setting auto values
    if (dns_ip == INADDR_NONE) {
      dns_ip = local_ip;
      dns_ip[3] = 1;
    }
    if (gateway_ip == INADDR_NONE) {
      gateway_ip = local_ip;
      gateway_ip[3] = 1;
    }
    if (netmask == INADDR_NONE) {
      netmask = IPAddress(255, 255, 255, 0);
    }
  }
  if (_ETH.config(local_ip, gateway_ip, netmask, dns_ip) && beginETH(mac)) {
    _hardwareStatus = EthernetHardwareFound;
  }
}

bool EthernetClass::beginETH(uint8_t *mac) {
#if CONFIG_ETH_USE_ESP32_EMAC
  if (_pin_mdc != -1) {
    return _ETH.begin(_phy_type, _phy_addr, _pin_mdc, _pin_mdio, _pin_power, _rmii_clk_mode);
  }
#endif
  if (_spi_host_device != SPI_HOST_MAX) {
    return _ETH.beginSPI(_phy_type, _phy_addr, mac, _pin_cs, _pin_irq, _pin_rst,
#if ETH_SPI_SUPPORTS_CUSTOM
        nullptr,
#endif
        _pin_sck, _pin_miso, _pin_mosi, _spi_host_device, _spi_freq_mhz);
  }
#if ETH_SPI_SUPPORTS_CUSTOM
  if (_spi == nullptr) {
    _spi = &SPI;
  }
  _spi->begin(_pin_sck, _pin_miso, _pin_mosi, -1);
  return _ETH.beginSPI(_phy_type, _phy_addr, mac, _pin_cs, _pin_irq, _pin_rst, _spi, -1, -1, -1, SPI_HOST_MAX, _spi_freq_mhz);
#endif
}

int EthernetClass::begin(unsigned long timeout) {
  return begin(nullptr, timeout);
}

void EthernetClass::begin(IPAddress ip, IPAddress dns, IPAddress gateway, IPAddress subnet) {
  begin(nullptr, ip, dns, gateway, subnet);
}

int EthernetClass::maintain() {
  return 0;
}

EthernetLinkStatus EthernetClass::linkStatus() {
  if (_ETH.netif() == NULL) {
    return Unknown;
  }
  return _ETH.linkUp() ? LinkON : LinkOFF;
}

EthernetHardwareStatus EthernetClass::hardwareStatus() {
  return _hardwareStatus;
}

void EthernetClass::setHostname(const char *hostname) {
  _ETH.setHostname(hostname);
}

void EthernetClass::MACAddress(uint8_t *mac_address) {
  _ETH.macAddress(mac_address);
}

uint8_t* EthernetClass::macAddress(uint8_t *mac) {
  return _ETH.macAddress(mac);
}

IPAddress EthernetClass::localIP() {
  return _ETH.localIP();
}

IPAddress EthernetClass::subnetMask() {
  return _ETH.subnetMask();
}

IPAddress EthernetClass::gatewayIP() {
  return _ETH.gatewayIP();
}

IPAddress EthernetClass::dnsServerIP() {
  return _ETH.dnsIP();
}

IPAddress EthernetClass::dnsIP(int n) {
  return _ETH.dnsIP(n);
}

void EthernetClass::setDnsServerIP(const IPAddress dns_server) {
  _ETH.dnsIP(0, dns_server);
}

void EthernetClass::setDNS(IPAddress dns_server, IPAddress dns_server2) {
  _ETH.dnsIP(0, dns_server);
  if (dns_server2 != INADDR_NONE) {
    _ETH.dnsIP(1, dns_server2);
  }
}

int EthernetClass::hostByName(const char *hostname, IPAddress &result) {
  return Network.hostByName(hostname, result);
}

EthernetClass Ethernet(ETH);
