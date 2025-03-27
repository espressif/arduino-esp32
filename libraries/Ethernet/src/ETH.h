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

#include "sdkconfig.h"
#if CONFIG_ETH_ENABLED

#ifndef _ETH_H_
#define _ETH_H_
#include "esp_idf_version.h"

//
// Example configurations for pins_arduino.h to allow starting with ETH.begin();
//

// // Example RMII LAN8720 (Olimex, etc.)
// #define ETH_PHY_TYPE        ETH_PHY_LAN8720
// #define ETH_PHY_ADDR         0
// #define ETH_PHY_MDC         23
// #define ETH_PHY_MDIO        18
// #define ETH_PHY_POWER       -1
// #define ETH_CLK_MODE        ETH_CLOCK_GPIO0_IN

// // Example RMII ESP32_Ethernet_V4
// #define ETH_PHY_TYPE        ETH_PHY_TLK110
// #define ETH_PHY_ADDR         1
// #define ETH_PHY_MDC         23
// #define ETH_PHY_MDIO        18
// #define ETH_PHY_POWER       -1
// #define ETH_CLK_MODE        ETH_CLOCK_GPIO0_OUT

// // Example SPI using ESP-IDF's driver
// #define ETH_PHY_TYPE        ETH_PHY_W5500
// #define ETH_PHY_ADDR         1
// #define ETH_PHY_CS          15
// #define ETH_PHY_IRQ          4
// #define ETH_PHY_RST          5
// #define ETH_PHY_SPI_HOST    SPI2_HOST
// #define ETH_PHY_SPI_SCK     14
// #define ETH_PHY_SPI_MISO    12
// #define ETH_PHY_SPI_MOSI    13

// // Example SPI using Arduino's driver
// #define ETH_PHY_TYPE        ETH_PHY_W5500
// #define ETH_PHY_ADDR         1
// #define ETH_PHY_CS          15
// #define ETH_PHY_IRQ          4
// #define ETH_PHY_RST          5
// #define ETH_PHY_SPI         SPI

// This will be uncommented once custom SPI support is available in ESP-IDF
#define ETH_SPI_SUPPORTS_CUSTOM 1
#define ETH_SPI_SUPPORTS_NO_IRQ 1

#include "Network.h"

#if ETH_SPI_SUPPORTS_CUSTOM
#include "SPI.h"
#endif
#include "esp_system.h"
#include "esp_eth.h"
#include "esp_netif.h"

#if CONFIG_ETH_USE_ESP32_EMAC
#define ETH_PHY_IP101 ETH_PHY_TLK110
#if CONFIG_IDF_TARGET_ESP32
typedef enum {
  ETH_CLOCK_GPIO0_IN,
  ETH_CLOCK_GPIO0_OUT,
  ETH_CLOCK_GPIO16_OUT,
  ETH_CLOCK_GPIO17_OUT
} eth_clock_mode_t;
//Dedicated GPIOs for RMII
#define ETH_RMII_TX_EN  21
#define ETH_RMII_TX0    19
#define ETH_RMII_TX1    22
#define ETH_RMII_RX0    25
#define ETH_RMII_RX1_EN 26
#define ETH_RMII_CRS_DV 27
#elif CONFIG_IDF_TARGET_ESP32P4
typedef emac_rmii_clock_mode_t eth_clock_mode_t;
#include "pins_arduino.h"
#ifndef ETH_RMII_TX_EN
#define ETH_RMII_TX_EN 49
#endif
#ifndef ETH_RMII_TX0
#define ETH_RMII_TX0 34
#endif
#ifndef ETH_RMII_TX1
#define ETH_RMII_TX1 35
#endif
#ifndef ETH_RMII_RX0
#define ETH_RMII_RX0 29
#endif
#ifndef ETH_RMII_RX1_EN
#define ETH_RMII_RX1_EN 30
#endif
#ifndef ETH_RMII_CRS_DV
#define ETH_RMII_CRS_DV 28
#endif
#ifndef ETH_RMII_CLK
#define ETH_RMII_CLK 50
#endif
#endif
#endif /* CONFIG_ETH_USE_ESP32_EMAC */

#ifndef ETH_PHY_SPI_FREQ_MHZ
#define ETH_PHY_SPI_FREQ_MHZ 20
#endif /* ETH_PHY_SPI_FREQ_MHZ */

#define ETH_PHY_ADDR_AUTO ESP_ETH_PHY_ADDR_AUTO

typedef enum {
#if CONFIG_ETH_USE_ESP32_EMAC
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
  ETH_PHY_GENERIC,
#define ETH_PHY_JL1101 ETH_PHY_GENERIC
#endif
  ETH_PHY_LAN8720,
  ETH_PHY_TLK110,
  ETH_PHY_RTL8201,
  ETH_PHY_DP83848,
  ETH_PHY_KSZ8041,
  ETH_PHY_KSZ8081,
#endif /* CONFIG_ETH_USE_ESP32_EMAC */
#if CONFIG_ETH_SPI_ETHERNET_DM9051
  ETH_PHY_DM9051,
#endif
#if CONFIG_ETH_SPI_ETHERNET_W5500
  ETH_PHY_W5500,
#endif
#if CONFIG_ETH_SPI_ETHERNET_KSZ8851SNL
  ETH_PHY_KSZ8851,
#endif
  ETH_PHY_MAX
} eth_phy_type_t;

class ETHClass : public NetworkInterface {
public:
  ETHClass(uint8_t eth_index = 0);
  ~ETHClass();

#if CONFIG_ETH_USE_ESP32_EMAC
  bool begin(eth_phy_type_t type, int32_t phy_addr, int mdc, int mdio, int power, eth_clock_mode_t clk_mode);
#endif /* CONFIG_ETH_USE_ESP32_EMAC */
#if ETH_SPI_SUPPORTS_CUSTOM
  bool begin(eth_phy_type_t type, int32_t phy_addr, int cs, int irq, int rst, SPIClass &spi, uint8_t spi_freq_mhz = ETH_PHY_SPI_FREQ_MHZ);
#endif
  bool begin(
    eth_phy_type_t type, int32_t phy_addr, int cs, int irq, int rst, spi_host_device_t spi_host, int sck = -1, int miso = -1, int mosi = -1,
    uint8_t spi_freq_mhz = ETH_PHY_SPI_FREQ_MHZ
  );

  bool begin() {
#if defined(ETH_PHY_TYPE) && defined(ETH_PHY_ADDR)
#if defined(CONFIG_ETH_USE_ESP32_EMAC) && defined(ETH_PHY_POWER) && defined(ETH_PHY_MDC) && defined(ETH_PHY_MDIO) && defined(ETH_CLK_MODE)
    return begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);
#elif defined(ETH_PHY_CS) && defined(ETH_PHY_IRQ) && defined(ETH_PHY_RST)
#if ETH_SPI_SUPPORTS_CUSTOM && defined(ETH_PHY_SPI)
    return begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_CS, ETH_PHY_IRQ, ETH_PHY_RST, ETH_PHY_SPI, ETH_PHY_SPI_FREQ_MHZ);
#elif defined(ETH_PHY_SPI_HOST) && defined(ETH_PHY_SPI_SCK) && defined(ETH_PHY_SPI_MISO) && defined(ETH_PHY_SPI_MOSI)
    return begin(
      ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_CS, ETH_PHY_IRQ, ETH_PHY_RST, ETH_PHY_SPI_HOST, ETH_PHY_SPI_SCK, ETH_PHY_SPI_MISO, ETH_PHY_SPI_MOSI,
      ETH_PHY_SPI_FREQ_MHZ
    );
#endif
#endif
#endif
    return false;
  }

  void end();

  // This function must be called before `begin()`
  void setTaskStackSize(size_t size);

  // ETH Handle APIs
  bool fullDuplex() const;
  bool setFullDuplex(bool on);

  uint16_t linkSpeed() const;
  bool setLinkSpeed(uint16_t speed);  //10 or 100

  bool autoNegotiation() const;
  bool setAutoNegotiation(bool on);

  uint32_t phyAddr() const;

  esp_eth_handle_t handle() const;

#if ETH_SPI_SUPPORTS_CUSTOM
  static esp_err_t _eth_spi_read(void *ctx, uint32_t cmd, uint32_t addr, void *data, uint32_t data_len);
  static esp_err_t _eth_spi_write(void *ctx, uint32_t cmd, uint32_t addr, const void *data, uint32_t data_len);
#endif

protected:
#if ETH_SPI_SUPPORTS_CUSTOM
  esp_err_t eth_spi_read(uint32_t cmd, uint32_t addr, void *data, uint32_t data_len);
  esp_err_t eth_spi_write(uint32_t cmd, uint32_t addr, const void *data, uint32_t data_len);
#endif

  // void getMac(uint8_t* mac);
  size_t printDriverInfo(Print &out) const;
  // static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

public:
  void _onEthEvent(int32_t event_id, void *event_data);

private:
  esp_eth_handle_t _eth_handle;
  uint8_t _eth_index;
  eth_phy_type_t _phy_type;
  esp_eth_netif_glue_handle_t _glue_handle;
  esp_eth_mac_t *_mac;
  esp_eth_phy_t *_phy;
  bool _eth_started;
  uint16_t _link_speed;
  bool _full_duplex;
  bool _auto_negotiation;
#if ETH_SPI_SUPPORTS_CUSTOM
  SPIClass *_spi;
  char _cs_str[10];
#endif
  uint8_t _spi_freq_mhz;
  int8_t _pin_cs;
  int8_t _pin_irq;
  int8_t _pin_rst;
  int8_t _pin_sck;
  int8_t _pin_miso;
  int8_t _pin_mosi;
#if CONFIG_ETH_USE_ESP32_EMAC
  int8_t _pin_mcd;
  int8_t _pin_mdio;
  int8_t _pin_power;
  int8_t _pin_rmii_clock;
#endif /* CONFIG_ETH_USE_ESP32_EMAC */
  size_t _task_stack_size;
  network_event_handle_t _eth_connected_event_handle;

  static bool ethDetachBus(void *bus_pointer);
  bool beginSPI(
    eth_phy_type_t type, int32_t phy_addr, uint8_t *mac_addr, int cs, int irq, int rst,
#if ETH_SPI_SUPPORTS_CUSTOM
    SPIClass *spi,
#endif
    int sck, int miso, int mosi, spi_host_device_t spi_host, uint8_t spi_freq_mhz
  );
  bool _setFullDuplex(bool on);
  bool _setLinkSpeed(uint16_t speed);
  bool _setAutoNegotiation(bool on);

  friend class EthernetClass;  // to access beginSPI
};

extern ETHClass ETH;

#endif /* _ETH_H_ */
#endif /* CONFIG_ETH_ENABLED */
