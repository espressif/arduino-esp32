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

// Disable the automatic pin remapping of the API calls in this file
#define ARDUINO_CORE_BUILD

#include "ETH.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_eth.h"
#include "esp_eth_mac.h"
#include "esp_eth_com.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#if CONFIG_ETH_USE_ESP32_EMAC
#include "soc/emac_ext_struct.h"
#include "soc/rtc.h"
#endif /* CONFIG_ETH_USE_ESP32_EMAC */
#include "esp32-hal-periman.h"
#include "lwip/err.h"
#include "lwip/dns.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "esp_netif_defaults.h"
#include "esp_eth_phy.h"

#define NUM_SUPPORTED_ETH_PORTS 3
static ETHClass *_ethernets[NUM_SUPPORTED_ETH_PORTS] = {NULL, NULL, NULL};
static esp_event_handler_instance_t _eth_ev_instance = NULL;

static void _eth_event_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {

  if (event_base == ETH_EVENT) {
    esp_eth_handle_t eth_handle = *((esp_eth_handle_t *)event_data);
    for (int i = 0; i < NUM_SUPPORTED_ETH_PORTS; ++i) {
      if (_ethernets[i] != NULL && _ethernets[i]->handle() == eth_handle) {
        _ethernets[i]->_onEthEvent(event_id, event_data);
      }
    }
  }
}

// This callback needs to be aware of which interface it should match against
static void onEthConnected(arduino_event_id_t event, arduino_event_info_t info) {
  if (event == ARDUINO_EVENT_ETH_CONNECTED) {
    uint8_t index = NUM_SUPPORTED_ETH_PORTS;
    for (int i = 0; i < NUM_SUPPORTED_ETH_PORTS; ++i) {
      if (_ethernets[i] != NULL && _ethernets[i]->handle() == info.eth_connected) {
        index = i;
        break;
      }
    }
    if (index == NUM_SUPPORTED_ETH_PORTS) {
      log_e("Could not find ETH interface with that handle!");
      return;
    }
    if (_ethernets[index]->getStatusBits() & ESP_NETIF_WANT_IP6_BIT) {
      esp_err_t err = esp_netif_create_ip6_linklocal(_ethernets[index]->netif());
      if (err != ESP_OK) {
        log_e("Failed to enable IPv6 Link Local on ETH: [%d] %s", err, esp_err_to_name(err));
      } else {
        log_v("Enabled IPv6 Link Local on %s", _ethernets[index]->desc());
      }
    }
  }
}

esp_eth_handle_t ETHClass::handle() const {
  return _eth_handle;
}

void ETHClass::_onEthEvent(int32_t event_id, void *event_data) {
  arduino_event_t arduino_event;
  arduino_event.event_id = ARDUINO_EVENT_MAX;

  if (event_id == ETHERNET_EVENT_CONNECTED) {
    log_v("%s Connected", desc());
    arduino_event.event_id = ARDUINO_EVENT_ETH_CONNECTED;
    arduino_event.event_info.eth_connected = handle();
    setStatusBits(ESP_NETIF_CONNECTED_BIT);
  } else if (event_id == ETHERNET_EVENT_DISCONNECTED) {
    log_v("%s Disconnected", desc());
    arduino_event.event_id = ARDUINO_EVENT_ETH_DISCONNECTED;
    clearStatusBits(ESP_NETIF_CONNECTED_BIT | ESP_NETIF_HAS_IP_BIT | ESP_NETIF_HAS_LOCAL_IP6_BIT | ESP_NETIF_HAS_GLOBAL_IP6_BIT);
  } else if (event_id == ETHERNET_EVENT_START) {
    log_v("%s Started", desc());
    arduino_event.event_id = ARDUINO_EVENT_ETH_START;
    setStatusBits(ESP_NETIF_STARTED_BIT);
  } else if (event_id == ETHERNET_EVENT_STOP) {
    log_v("%s Stopped", desc());
    arduino_event.event_id = ARDUINO_EVENT_ETH_STOP;
    clearStatusBits(
      ESP_NETIF_STARTED_BIT | ESP_NETIF_CONNECTED_BIT | ESP_NETIF_HAS_IP_BIT | ESP_NETIF_HAS_LOCAL_IP6_BIT | ESP_NETIF_HAS_GLOBAL_IP6_BIT
      | ESP_NETIF_HAS_STATIC_IP_BIT
    );
  }

  if (arduino_event.event_id < ARDUINO_EVENT_MAX) {
    Network.postEvent(&arduino_event);
  }
}

ETHClass::ETHClass(uint8_t eth_index)
  : _eth_handle(NULL), _eth_index(eth_index), _phy_type(ETH_PHY_MAX), _glue_handle(NULL), _mac(NULL), _phy(NULL)
#if ETH_SPI_SUPPORTS_CUSTOM
    ,
    _spi(NULL)
#endif
    ,
    _spi_freq_mhz(20), _pin_cs(-1), _pin_irq(-1), _pin_rst(-1), _pin_sck(-1), _pin_miso(-1), _pin_mosi(-1)
#if CONFIG_ETH_USE_ESP32_EMAC
    ,
    _pin_mcd(-1), _pin_mdio(-1), _pin_power(-1), _pin_rmii_clock(-1)
#endif /* CONFIG_ETH_USE_ESP32_EMAC */
    ,
    _task_stack_size(4096) {
}

ETHClass::~ETHClass() {}

bool ETHClass::ethDetachBus(void *bus_pointer) {
  ETHClass *bus = (ETHClass *)bus_pointer;
  bus->end();
  return true;
}

void ETHClass::setTaskStackSize(size_t size) {
  _task_stack_size = size;
}

#if CONFIG_ETH_USE_ESP32_EMAC
bool ETHClass::begin(eth_phy_type_t type, int32_t phy_addr, int mdc, int mdio, int power, eth_clock_mode_t clock_mode) {
  esp_err_t ret = ESP_OK;
  if (_eth_index > 2) {
    return false;
  }
  if (_esp_netif != NULL) {
    return true;
  }
  if (phy_addr < ETH_PHY_ADDR_AUTO) {
    log_e("Invalid PHY address: %d, set to ETH_PHY_ADDR_AUTO for auto detection", phy_addr);
    return false;
  }
  perimanSetBusDeinit(ESP32_BUS_TYPE_ETHERNET_RMII, ETHClass::ethDetachBus);
  perimanSetBusDeinit(ESP32_BUS_TYPE_ETHERNET_CLK, ETHClass::ethDetachBus);
  perimanSetBusDeinit(ESP32_BUS_TYPE_ETHERNET_MCD, ETHClass::ethDetachBus);
  perimanSetBusDeinit(ESP32_BUS_TYPE_ETHERNET_MDIO, ETHClass::ethDetachBus);
  if (power != -1) {
    perimanSetBusDeinit(ESP32_BUS_TYPE_ETHERNET_PWR, ETHClass::ethDetachBus);
  }

  Network.begin();
  _ethernets[_eth_index] = this;

  eth_esp32_emac_config_t mac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
  mac_config.clock_config.rmii.clock_mode = (clock_mode) ? EMAC_CLK_OUT : EMAC_CLK_EXT_IN;
  mac_config.clock_config.rmii.clock_gpio = (1 == clock_mode)   ? EMAC_APPL_CLK_OUT_GPIO
                                            : (2 == clock_mode) ? EMAC_CLK_OUT_GPIO
                                            : (3 == clock_mode) ? EMAC_CLK_OUT_180_GPIO
                                                                : EMAC_CLK_IN_GPIO;
  mac_config.smi_mdc_gpio_num = digitalPinToGPIONumber(mdc);
  mac_config.smi_mdio_gpio_num = digitalPinToGPIONumber(mdio);

  _pin_mcd = digitalPinToGPIONumber(mdc);
  _pin_mdio = digitalPinToGPIONumber(mdio);
  _pin_rmii_clock = mac_config.clock_config.rmii.clock_gpio;
  _pin_power = digitalPinToGPIONumber(power);

  if (!perimanClearPinBus(_pin_rmii_clock)) {
    return false;
  }
  if (!perimanClearPinBus(_pin_mcd)) {
    return false;
  }
  if (!perimanClearPinBus(_pin_mdio)) {
    return false;
  }
  if (!perimanClearPinBus(ETH_RMII_TX_EN)) {
    return false;
  }
  if (!perimanClearPinBus(ETH_RMII_TX0)) {
    return false;
  }
  if (!perimanClearPinBus(ETH_RMII_TX1)) {
    return false;
  }
  if (!perimanClearPinBus(ETH_RMII_RX0)) {
    return false;
  }
  if (!perimanClearPinBus(ETH_RMII_RX1_EN)) {
    return false;
  }
  if (!perimanClearPinBus(ETH_RMII_CRS_DV)) {
    return false;
  }
  if (_pin_power != -1) {
    if (!perimanClearPinBus(_pin_power)) {
      return false;
    }
  }

  eth_mac_config_t eth_mac_config = ETH_MAC_DEFAULT_CONFIG();
  eth_mac_config.sw_reset_timeout_ms = 1000;
  eth_mac_config.rx_task_stack_size = _task_stack_size;

  esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config, &eth_mac_config);
  if (mac == NULL) {
    log_e("esp_eth_mac_new_esp32 failed");
    return false;
  }

  eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
  phy_config.phy_addr = phy_addr;
  phy_config.reset_gpio_num = _pin_power;

  esp_eth_phy_t *phy = NULL;
  switch (type) {
    case ETH_PHY_LAN8720: phy = esp_eth_phy_new_lan87xx(&phy_config); break;
    case ETH_PHY_TLK110:  phy = esp_eth_phy_new_ip101(&phy_config); break;
    case ETH_PHY_RTL8201: phy = esp_eth_phy_new_rtl8201(&phy_config); break;
    case ETH_PHY_DP83848: phy = esp_eth_phy_new_dp83848(&phy_config); break;
    case ETH_PHY_KSZ8041: phy = esp_eth_phy_new_ksz80xx(&phy_config); break;
    case ETH_PHY_KSZ8081: phy = esp_eth_phy_new_ksz80xx(&phy_config); break;
    default:              log_e("Unsupported PHY %d", type); break;
  }
  if (phy == NULL) {
    log_e("esp_eth_phy_new failed");
    return false;
  }

  _eth_handle = NULL;
  esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
  ret = esp_eth_driver_install(&eth_config, &_eth_handle);
  if (ret != ESP_OK) {
    log_e("Ethernet driver install failed: %d", ret);
    return false;
  }
  if (_eth_handle == NULL) {
    log_e("esp_eth_driver_install failed! eth_handle is NULL");
    return false;
  }

  esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();

  // Use ESP_NETIF_INHERENT_DEFAULT_ETH when multiple Ethernet interfaces are used and so you need to modify
  // esp-netif configuration parameters for each interface (name, priority, etc.).
  char if_key_str[10];
  char if_desc_str[10];
  char num_str[3];
  itoa(_eth_index, num_str, 10);
  if (_eth_index == 0) {
    strcpy(if_key_str, "ETH_DEF");
  } else {
    strcat(strcpy(if_key_str, "ETH_"), num_str);
  }
  strcat(strcpy(if_desc_str, "eth"), num_str);

  esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
  esp_netif_config.if_key = if_key_str;
  esp_netif_config.if_desc = if_desc_str;
  esp_netif_config.route_prio -= _eth_index * 5;

  cfg.base = &esp_netif_config;

  _esp_netif = esp_netif_new(&cfg);
  if (_esp_netif == NULL) {
    log_e("esp_netif_new failed");
    return false;
  }

  _glue_handle = esp_eth_new_netif_glue(_eth_handle);
  if (_glue_handle == NULL) {
    log_e("esp_eth_new_netif_glue failed");
    return false;
  }

  /* attach Ethernet driver to TCP/IP stack */
  ret = esp_netif_attach(_esp_netif, _glue_handle);
  if (ret != ESP_OK) {
    log_e("esp_netif_attach failed: %d", ret);
    return false;
  }

  if (_eth_ev_instance == NULL && esp_event_handler_instance_register(ETH_EVENT, ESP_EVENT_ANY_ID, &_eth_event_cb, NULL, &_eth_ev_instance)) {
    log_e("event_handler_instance_register for ETH_EVENT Failed!");
    return false;
  }

  /* attach to receive events */
  initNetif((Network_Interface_ID)(ESP_NETIF_ID_ETH + _eth_index));

  Network.onSysEvent(onEthConnected, ARDUINO_EVENT_ETH_CONNECTED);

  ret = esp_eth_start(_eth_handle);
  if (ret != ESP_OK) {
    log_e("esp_eth_start failed: %d", ret);
    return false;
  }

  if (!perimanSetPinBus(_pin_rmii_clock, ESP32_BUS_TYPE_ETHERNET_CLK, (void *)(this), -1, -1)) {
    goto err;
  }
  if (!perimanSetPinBus(_pin_mcd, ESP32_BUS_TYPE_ETHERNET_MCD, (void *)(this), -1, -1)) {
    goto err;
  }
  if (!perimanSetPinBus(_pin_mdio, ESP32_BUS_TYPE_ETHERNET_MDIO, (void *)(this), -1, -1)) {
    goto err;
  }

  if (!perimanSetPinBus(ETH_RMII_TX_EN, ESP32_BUS_TYPE_ETHERNET_RMII, (void *)(this), -1, -1)) {
    goto err;
  }
  if (!perimanSetPinBus(ETH_RMII_TX0, ESP32_BUS_TYPE_ETHERNET_RMII, (void *)(this), -1, -1)) {
    goto err;
  }
  if (!perimanSetPinBus(ETH_RMII_TX1, ESP32_BUS_TYPE_ETHERNET_RMII, (void *)(this), -1, -1)) {
    goto err;
  }
  if (!perimanSetPinBus(ETH_RMII_RX0, ESP32_BUS_TYPE_ETHERNET_RMII, (void *)(this), -1, -1)) {
    goto err;
  }
  if (!perimanSetPinBus(ETH_RMII_RX1_EN, ESP32_BUS_TYPE_ETHERNET_RMII, (void *)(this), -1, -1)) {
    goto err;
  }
  if (!perimanSetPinBus(ETH_RMII_CRS_DV, ESP32_BUS_TYPE_ETHERNET_RMII, (void *)(this), -1, -1)) {
    goto err;
  }

  if (_pin_power != -1) {
    if (!perimanSetPinBus(_pin_power, ESP32_BUS_TYPE_ETHERNET_PWR, (void *)(this), -1, -1)) {
      goto err;
    }
  }

  // holds a few milliseconds to let DHCP start and enter into a good state
  // FIX ME -- addresses issue https://github.com/espressif/arduino-esp32/issues/5733
  delay(50);

  return true;

err:
  log_e("Failed to set all pins bus to ETHERNET");
  ETHClass::ethDetachBus((void *)(this));
  return false;
}
#endif /* CONFIG_ETH_USE_ESP32_EMAC */

#if ETH_SPI_SUPPORTS_CUSTOM
__unused static void *_eth_spi_init(const void *ctx) {
  return (void *)ctx;
}

__unused static esp_err_t _eth_spi_deinit(void *ctx) {
  return ESP_OK;
}

esp_err_t ETHClass::_eth_spi_read(void *ctx, uint32_t cmd, uint32_t addr, void *data, uint32_t data_len) {
  return ((ETHClass *)ctx)->eth_spi_read(cmd, addr, data, data_len);
}

esp_err_t ETHClass::_eth_spi_write(void *ctx, uint32_t cmd, uint32_t addr, const void *data, uint32_t data_len) {
  return ((ETHClass *)ctx)->eth_spi_write(cmd, addr, data, data_len);
}

esp_err_t ETHClass::eth_spi_read(uint32_t cmd, uint32_t addr, void *data, uint32_t data_len) {
  if (_spi == NULL) {
    return ESP_FAIL;
  }
  // log_i(" 0x%04lx 0x%04lx %lu", cmd, addr, data_len);
  _spi->beginTransaction(SPISettings(_spi_freq_mhz * 1000 * 1000, MSBFIRST, SPI_MODE0));
  digitalWrite(_pin_cs, LOW);

#if CONFIG_ETH_SPI_ETHERNET_DM9051
  if (_phy_type == ETH_PHY_DM9051) {
    _spi->write(((cmd & 0x01) << 7) | (addr & 0x7F));
  } else
#endif
#if CONFIG_ETH_SPI_ETHERNET_W5500
    if (_phy_type == ETH_PHY_W5500) {
    _spi->write16(cmd);
    _spi->write(addr);
  } else
#endif
#if CONFIG_ETH_SPI_ETHERNET_KSZ8851SNL
    if (_phy_type == ETH_PHY_KSZ8851) {
    if (cmd > 1) {
      _spi->write(cmd << 6 | addr);
    } else {
      _spi->write16(cmd << 14 | addr);
    }
  } else
#endif
  {
    log_e("Unsupported PHY module: %d", _phy_type);
    digitalWrite(_pin_cs, HIGH);
    _spi->endTransaction();
    return ESP_FAIL;
  }
  _spi->transferBytes(NULL, (uint8_t *)data, data_len);

  digitalWrite(_pin_cs, HIGH);
  _spi->endTransaction();
  return ESP_OK;
}

esp_err_t ETHClass::eth_spi_write(uint32_t cmd, uint32_t addr, const void *data, uint32_t data_len) {
  if (_spi == NULL) {
    return ESP_FAIL;
  }
  // log_i("0x%04lx 0x%04lx %lu", cmd, addr, data_len);
  _spi->beginTransaction(SPISettings(_spi_freq_mhz * 1000 * 1000, MSBFIRST, SPI_MODE0));
  digitalWrite(_pin_cs, LOW);

#if CONFIG_ETH_SPI_ETHERNET_DM9051
  if (_phy_type == ETH_PHY_DM9051) {
    _spi->write(((cmd & 0x01) << 7) | (addr & 0x7F));
  } else
#endif
#if CONFIG_ETH_SPI_ETHERNET_W5500
    if (_phy_type == ETH_PHY_W5500) {
    _spi->write16(cmd);
    _spi->write(addr);
  } else
#endif
#if CONFIG_ETH_SPI_ETHERNET_KSZ8851SNL
    if (_phy_type == ETH_PHY_KSZ8851) {
    if (cmd > 1) {
      _spi->write(cmd << 6 | addr);
    } else {
      _spi->write16(cmd << 14 | addr);
    }
  } else
#endif
  {
    log_e("Unsupported PHY module: %d", _phy_type);
    digitalWrite(_pin_cs, HIGH);
    _spi->endTransaction();
    return ESP_FAIL;
  }
  _spi->writeBytes((const uint8_t *)data, data_len);

  digitalWrite(_pin_cs, HIGH);
  _spi->endTransaction();
  return ESP_OK;
}
#endif

bool ETHClass::beginSPI(
  eth_phy_type_t type, int32_t phy_addr, uint8_t *mac_addr_p, int cs, int irq, int rst,
#if ETH_SPI_SUPPORTS_CUSTOM
  SPIClass *spi,
#endif
  int sck, int miso, int mosi, spi_host_device_t spi_host, uint8_t spi_freq_mhz
) {
  esp_err_t ret = ESP_OK;

  if (_esp_netif != NULL || _eth_handle != NULL) {
    log_w("ETH Already Started");
    return true;
  }
#if ETH_SPI_SUPPORTS_NO_IRQ
  if (cs < 0) {
    log_e("CS pin must be defined!");
#else
  if (cs < 0 || irq < 0) {
    log_e("CS and IRQ pins must be defined!");
#endif
    return false;
  }
  if (phy_addr < ETH_PHY_ADDR_AUTO) {
    log_e("Invalid PHY address: %d, set to ETH_PHY_ADDR_AUTO for auto detection", phy_addr);
    return false;
  }

  perimanSetBusDeinit(ESP32_BUS_TYPE_ETHERNET_SPI, ETHClass::ethDetachBus);

  if (_pin_cs != -1) {
    if (!perimanClearPinBus(_pin_cs)) {
      return false;
    }
  }
  if (_pin_rst != -1) {
    if (!perimanClearPinBus(_pin_rst)) {
      return false;
    }
  }
  if (_pin_irq != -1) {
    if (!perimanClearPinBus(_pin_irq)) {
      return false;
    }
  }
  if (_pin_sck != -1) {
    if (!perimanClearPinBus(_pin_sck)) {
      return false;
    }
  }
  if (_pin_miso != -1) {
    if (!perimanClearPinBus(_pin_miso)) {
      return false;
    }
  }
  if (_pin_mosi != -1) {
    if (!perimanClearPinBus(_pin_mosi)) {
      return false;
    }
  }

#if ETH_SPI_SUPPORTS_CUSTOM
  _spi = spi;
#endif
  if (spi_freq_mhz) {
    _spi_freq_mhz = spi_freq_mhz;
  }
  _phy_type = type;
  _pin_cs = digitalPinToGPIONumber(cs);
  _pin_irq = digitalPinToGPIONumber(irq);
  _pin_rst = digitalPinToGPIONumber(rst);
  _pin_sck = digitalPinToGPIONumber(sck);
  _pin_miso = digitalPinToGPIONumber(miso);
  _pin_mosi = digitalPinToGPIONumber(mosi);

#if ETH_SPI_SUPPORTS_CUSTOM
  if (_spi != NULL) {
    pinMode(_pin_cs, OUTPUT);
    digitalWrite(_pin_cs, HIGH);
    char cs_num_str[3];
    itoa(_eth_index, cs_num_str, 10);
    strcat(strcpy(_cs_str, "ETH_CS["), cs_num_str);
    strcat(_cs_str, "]");
    perimanSetPinBusExtraType(_pin_cs, _cs_str);
  }
#endif

  // Init SPI bus
  if (_pin_sck >= 0 && _pin_miso >= 0 && _pin_mosi >= 0) {
    spi_bus_config_t buscfg;
    memset(&buscfg, 0, sizeof(spi_bus_config_t));
    buscfg.mosi_io_num = _pin_mosi;
    buscfg.miso_io_num = _pin_miso;
    buscfg.sclk_io_num = _pin_sck;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.data4_io_num = -1;
    buscfg.data5_io_num = -1;
    buscfg.data6_io_num = -1;
    buscfg.data7_io_num = -1;
    buscfg.max_transfer_sz = -1;
    ret = spi_bus_initialize(spi_host, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
      log_e("SPI bus initialize failed: %d", ret);
      return false;
    }
  }

  Network.begin();
  _ethernets[_eth_index] = this;

  // Install GPIO ISR handler to be able to service SPI Eth modules interrupts
  ret = gpio_install_isr_service(0);
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    log_e("GPIO ISR handler install failed: %d", ret);
    return false;
  }

  // Init common MAC and PHY configs to default
  __unused eth_mac_config_t eth_mac_config = ETH_MAC_DEFAULT_CONFIG();
  __unused eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

  // Set RX Task Stack Size
  eth_mac_config.rx_task_stack_size = _task_stack_size;

  // Update PHY config based on board specific configuration
  phy_config.phy_addr = phy_addr;
  phy_config.reset_gpio_num = _pin_rst;

  // Configure SPI interface for specific SPI module
  spi_device_interface_config_t spi_devcfg;
  memset(&spi_devcfg, 0, sizeof(spi_device_interface_config_t));
  spi_devcfg.mode = 0;
  spi_devcfg.clock_speed_hz = _spi_freq_mhz * 1000 * 1000;
  spi_devcfg.input_delay_ns = 20;
  spi_devcfg.spics_io_num = _pin_cs;
  spi_devcfg.queue_size = 20;

#if CONFIG_ETH_SPI_ETHERNET_W5500
  if (type == ETH_PHY_W5500) {
    eth_w5500_config_t mac_config = ETH_W5500_DEFAULT_CONFIG(spi_host, &spi_devcfg);
    mac_config.int_gpio_num = _pin_irq;
#if ETH_SPI_SUPPORTS_NO_IRQ
    if (_pin_irq < 0) {
      mac_config.poll_period_ms = 10;
    }
#endif
#if ETH_SPI_SUPPORTS_CUSTOM
    if (_spi != NULL) {
      mac_config.custom_spi_driver.config = this;
      mac_config.custom_spi_driver.init = _eth_spi_init;
      mac_config.custom_spi_driver.deinit = _eth_spi_deinit;
      mac_config.custom_spi_driver.read = _eth_spi_read;
      mac_config.custom_spi_driver.write = _eth_spi_write;
    }
#endif
    _mac = esp_eth_mac_new_w5500(&mac_config, &eth_mac_config);
    _phy = esp_eth_phy_new_w5500(&phy_config);
  } else
#endif
#if CONFIG_ETH_SPI_ETHERNET_DM9051
    if (type == ETH_PHY_DM9051) {
    eth_dm9051_config_t mac_config = ETH_DM9051_DEFAULT_CONFIG(spi_host, &spi_devcfg);
    mac_config.int_gpio_num = _pin_irq;
#if ETH_SPI_SUPPORTS_CUSTOM
    if (_spi != NULL) {
      mac_config.custom_spi_driver.config = this;
      mac_config.custom_spi_driver.init = _eth_spi_init;
      mac_config.custom_spi_driver.deinit = _eth_spi_deinit;
      mac_config.custom_spi_driver.read = _eth_spi_read;
      mac_config.custom_spi_driver.write = _eth_spi_write;
    }
#endif
    _mac = esp_eth_mac_new_dm9051(&mac_config, &eth_mac_config);
    _phy = esp_eth_phy_new_dm9051(&phy_config);
  } else
#endif
#if CONFIG_ETH_SPI_ETHERNET_KSZ8851SNL
    if (type == ETH_PHY_KSZ8851) {
    eth_ksz8851snl_config_t mac_config = ETH_KSZ8851SNL_DEFAULT_CONFIG(spi_host, &spi_devcfg);
    mac_config.int_gpio_num = _pin_irq;
#if ETH_SPI_SUPPORTS_CUSTOM
    if (_spi != NULL) {
      mac_config.custom_spi_driver.config = this;
      mac_config.custom_spi_driver.init = _eth_spi_init;
      mac_config.custom_spi_driver.deinit = _eth_spi_deinit;
      mac_config.custom_spi_driver.read = _eth_spi_read;
      mac_config.custom_spi_driver.write = _eth_spi_write;
    }
#endif
    _mac = esp_eth_mac_new_ksz8851snl(&mac_config, &eth_mac_config);
    _phy = esp_eth_phy_new_ksz8851snl(&phy_config);
  } else
#endif
  {
    log_e("Unsupported PHY module: %d", (int)type);
    return false;
  }

  // Init Ethernet driver to default and install it
  esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(_mac, _phy);
  ret = esp_eth_driver_install(&eth_config, &_eth_handle);
  if (ret != ESP_OK) {
    log_e("SPI Ethernet driver install failed: %d", ret);
    return false;
  }
  if (_eth_handle == NULL) {
    log_e("esp_eth_driver_install failed! eth_handle is NULL");
    return false;
  }

  uint8_t mac_addr[ETH_ADDR_LEN];
  if (mac_addr_p != nullptr) {
    memcpy(mac_addr, mac_addr_p, ETH_ADDR_LEN);
  } else {
    // Derive a new MAC address for this interface
    uint8_t base_mac_addr[ETH_ADDR_LEN];
    ret = esp_efuse_mac_get_default(base_mac_addr);
    if (ret != ESP_OK) {
      log_e("Get EFUSE MAC failed: %d", ret);
      return false;
    }
    base_mac_addr[ETH_ADDR_LEN - 1] += _eth_index;  //Increment by the ETH number
    esp_derive_local_mac(mac_addr, base_mac_addr);
  }

  ret = esp_eth_ioctl(_eth_handle, ETH_CMD_S_MAC_ADDR, mac_addr);
  if (ret != ESP_OK) {
    log_e("SPI Ethernet MAC address config failed: %d", ret);
    return false;
  }

  // Use ESP_NETIF_DEFAULT_ETH when just one Ethernet interface is used and you don't need to modify
  // default esp-netif configuration parameters.
  esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();

  // Use ESP_NETIF_INHERENT_DEFAULT_ETH when multiple Ethernet interfaces are used and so you need to modify
  // esp-netif configuration parameters for each interface (name, priority, etc.).
  char if_key_str[10];
  char if_desc_str[10];
  char num_str[3];
  itoa(_eth_index, num_str, 10);
  if (_eth_index == 0) {
    strcpy(if_key_str, "ETH_DEF");
  } else {
    strcat(strcpy(if_key_str, "ETH_"), num_str);
  }
  strcat(strcpy(if_desc_str, "eth"), num_str);

  esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
  esp_netif_config.if_key = if_key_str;
  esp_netif_config.if_desc = if_desc_str;
  esp_netif_config.route_prio -= _eth_index * 5;

  cfg.base = &esp_netif_config;

  _esp_netif = esp_netif_new(&cfg);
  if (_esp_netif == NULL) {
    log_e("esp_netif_new failed");
    return false;
  }
  // Attach Ethernet driver to TCP/IP stack
  _glue_handle = esp_eth_new_netif_glue(_eth_handle);
  if (_glue_handle == NULL) {
    log_e("esp_eth_new_netif_glue failed");
    return false;
  }

  ret = esp_netif_attach(_esp_netif, _glue_handle);
  if (ret != ESP_OK) {
    log_e("esp_netif_attach failed: %d", ret);
    return false;
  }

  if (_eth_ev_instance == NULL && esp_event_handler_instance_register(ETH_EVENT, ESP_EVENT_ANY_ID, &_eth_event_cb, NULL, &_eth_ev_instance)) {
    log_e("event_handler_instance_register for ETH_EVENT Failed!");
    return false;
  }

  /* attach to receive events */
  initNetif((Network_Interface_ID)(ESP_NETIF_ID_ETH + _eth_index));

  // Start Ethernet driver state machine
  ret = esp_eth_start(_eth_handle);
  if (ret != ESP_OK) {
    log_e("esp_eth_start failed: %d", ret);
    return false;
  }

  // If Arduino's SPI is used, cs pin is in GPIO mode
#if ETH_SPI_SUPPORTS_CUSTOM
  if (_spi == NULL) {
#endif
    if (!perimanSetPinBus(_pin_cs, ESP32_BUS_TYPE_ETHERNET_SPI, (void *)(this), _eth_index, -1)) {
      goto err;
    }
    perimanSetPinBusExtraType(_pin_cs, "ETH_SPI_CS");
#if ETH_SPI_SUPPORTS_CUSTOM
  }
#endif
#if ETH_SPI_SUPPORTS_NO_IRQ
  if (_pin_irq != -1) {
#endif
    if (!perimanSetPinBus(_pin_irq, ESP32_BUS_TYPE_ETHERNET_SPI, (void *)(this), _eth_index, -1)) {
      goto err;
    }
    perimanSetPinBusExtraType(_pin_irq, "ETH_IRQ");
#if ETH_SPI_SUPPORTS_NO_IRQ
  }
#endif
  if (_pin_sck != -1) {
    if (!perimanSetPinBus(_pin_sck, ESP32_BUS_TYPE_ETHERNET_SPI, (void *)(this), _eth_index, -1)) {
      goto err;
    }
    perimanSetPinBusExtraType(_pin_sck, "ETH_SPI_SCK");
  }
  if (_pin_miso != -1) {
    if (!perimanSetPinBus(_pin_miso, ESP32_BUS_TYPE_ETHERNET_SPI, (void *)(this), _eth_index, -1)) {
      goto err;
    }
    perimanSetPinBusExtraType(_pin_miso, "ETH_SPI_MISO");
  }
  if (_pin_mosi != -1) {
    if (!perimanSetPinBus(_pin_mosi, ESP32_BUS_TYPE_ETHERNET_SPI, (void *)(this), _eth_index, -1)) {
      goto err;
    }
    perimanSetPinBusExtraType(_pin_mosi, "ETH_SPI_MOSI");
  }
  if (_pin_rst != -1) {
    if (!perimanSetPinBus(_pin_rst, ESP32_BUS_TYPE_ETHERNET_SPI, (void *)(this), _eth_index, -1)) {
      goto err;
    }
    perimanSetPinBusExtraType(_pin_rst, "ETH_RST");
  }

  Network.onSysEvent(onEthConnected, ARDUINO_EVENT_ETH_CONNECTED);

  return true;

err:
  log_e("Failed to set all pins bus to ETHERNET");
  ETHClass::ethDetachBus((void *)(this));
  return false;
}

#if ETH_SPI_SUPPORTS_CUSTOM
bool ETHClass::begin(eth_phy_type_t type, int32_t phy_addr, int cs, int irq, int rst, SPIClass &spi, uint8_t spi_freq_mhz) {

  return beginSPI(type, phy_addr, nullptr, cs, irq, rst, &spi, -1, -1, -1, SPI2_HOST, spi_freq_mhz);
}
#endif

bool ETHClass::begin(
  eth_phy_type_t type, int32_t phy_addr, int cs, int irq, int rst, spi_host_device_t spi_host, int sck, int miso, int mosi, uint8_t spi_freq_mhz
) {

  return beginSPI(
    type, phy_addr, nullptr, cs, irq, rst,
#if ETH_SPI_SUPPORTS_CUSTOM
    NULL,
#endif
    sck, miso, mosi, spi_host, spi_freq_mhz
  );
}

static bool empty_ethDetachBus(void *bus_pointer) {
  return true;
}

void ETHClass::end(void) {

  Network.removeEvent(onEthConnected, ARDUINO_EVENT_ETH_CONNECTED);

  if (_eth_handle != NULL) {
    if (esp_eth_stop(_eth_handle) != ESP_OK) {
      log_e("Failed to stop Ethernet");
      return;
    }
    //wait for stop
    while (getStatusBits() & ESP_NETIF_STARTED_BIT) {
      delay(10);
    }
    //delete glue first
    if (_glue_handle != NULL) {
      if (esp_eth_del_netif_glue(_glue_handle) != ESP_OK) {
        log_e("Failed to del_netif_glue Ethernet");
        return;
      }
      _glue_handle = NULL;
    }
    //uninstall driver
    if (esp_eth_driver_uninstall(_eth_handle) != ESP_OK) {
      log_e("Failed to uninstall Ethernet");
      return;
    }
    _eth_handle = NULL;
    //delete mac
    if (_mac != NULL) {
      _mac->del(_mac);
      _mac = NULL;
    }
    //delete phy
    if (_phy != NULL) {
      _phy->del(_phy);
      _phy = NULL;
    }
  }

  if (_eth_ev_instance != NULL) {
    bool do_not_unreg_ev_handler = false;
    for (int i = 0; i < NUM_SUPPORTED_ETH_PORTS; ++i) {
      if (_ethernets[i] != NULL && _ethernets[i]->netif() != NULL && _ethernets[i]->netif() != _esp_netif) {
        do_not_unreg_ev_handler = true;
        break;
      }
    }
    if (!do_not_unreg_ev_handler) {
      if (esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, &_eth_event_cb) == ESP_OK) {
        _eth_ev_instance = NULL;
        log_v("Unregistered event handler");
      } else {
        log_e("Failed to unregister event handler");
      }
    }
  }

  destroyNetif();

#if ETH_SPI_SUPPORTS_CUSTOM
  _spi = NULL;
#endif
#if CONFIG_ETH_USE_ESP32_EMAC
  perimanSetBusDeinit(ESP32_BUS_TYPE_ETHERNET_RMII, empty_ethDetachBus);
  perimanSetBusDeinit(ESP32_BUS_TYPE_ETHERNET_CLK, empty_ethDetachBus);
  perimanSetBusDeinit(ESP32_BUS_TYPE_ETHERNET_MCD, empty_ethDetachBus);
  perimanSetBusDeinit(ESP32_BUS_TYPE_ETHERNET_MDIO, empty_ethDetachBus);
  perimanSetBusDeinit(ESP32_BUS_TYPE_ETHERNET_PWR, empty_ethDetachBus);
  if (_pin_rmii_clock != -1 && _pin_mcd != -1 && _pin_mdio != -1) {
    perimanClearPinBus(_pin_rmii_clock);
    perimanClearPinBus(_pin_mcd);
    perimanClearPinBus(_pin_mdio);

    perimanClearPinBus(ETH_RMII_TX_EN);
    perimanClearPinBus(ETH_RMII_TX0);
    perimanClearPinBus(ETH_RMII_TX1);
    perimanClearPinBus(ETH_RMII_RX0);
    perimanClearPinBus(ETH_RMII_RX1_EN);
    perimanClearPinBus(ETH_RMII_CRS_DV);

    _pin_rmii_clock = -1;
    _pin_mcd = -1;
    _pin_mdio = -1;
  }

  if (_pin_power != -1) {
    perimanClearPinBus(_pin_power);
    _pin_power = -1;
  }
#endif /* CONFIG_ETH_USE_ESP32_EMAC */
  perimanSetBusDeinit(ESP32_BUS_TYPE_ETHERNET_SPI, empty_ethDetachBus);
  if (_pin_cs != -1) {
    perimanClearPinBus(_pin_cs);
    _pin_cs = -1;
  }
  if (_pin_irq != -1) {
    perimanClearPinBus(_pin_irq);
    _pin_irq = -1;
  }
  if (_pin_sck != -1) {
    perimanClearPinBus(_pin_sck);
    _pin_sck = -1;
  }
  if (_pin_miso != -1) {
    perimanClearPinBus(_pin_miso);
    _pin_miso = -1;
  }
  if (_pin_mosi != -1) {
    perimanClearPinBus(_pin_mosi);
    _pin_mosi = -1;
  }
  if (_pin_rst != -1) {
    perimanClearPinBus(_pin_rst);
    _pin_rst = -1;
  }
}

bool ETHClass::fullDuplex() const {
  if (_eth_handle == NULL) {
    return false;
  }
  eth_duplex_t link_duplex;
  esp_eth_ioctl(_eth_handle, ETH_CMD_G_DUPLEX_MODE, &link_duplex);
  return (link_duplex == ETH_DUPLEX_FULL);
}

bool ETHClass::autoNegotiation() const {
  if (_eth_handle == NULL) {
    return false;
  }
  bool auto_nego;
  esp_eth_ioctl(_eth_handle, ETH_CMD_G_AUTONEGO, &auto_nego);
  return auto_nego;
}

uint32_t ETHClass::phyAddr() const {
  if (_eth_handle == NULL) {
    return 0;
  }
  uint32_t phy_addr;
  esp_eth_ioctl(_eth_handle, ETH_CMD_G_PHY_ADDR, &phy_addr);
  return phy_addr;
}

uint8_t ETHClass::linkSpeed() const {
  if (_eth_handle == NULL) {
    return 0;
  }
  eth_speed_t link_speed;
  esp_eth_ioctl(_eth_handle, ETH_CMD_G_SPEED, &link_speed);
  return (link_speed == ETH_SPEED_10M) ? 10 : 100;
}

// void ETHClass::getMac(uint8_t* mac)
// {
//     if(_eth_handle != NULL && mac != NULL){
//         esp_eth_ioctl(_eth_handle, ETH_CMD_G_MAC_ADDR, mac);
//     }
// }

size_t ETHClass::printDriverInfo(Print &out) const {
  size_t bytes = 0;
  bytes += out.print(",");
  bytes += out.print(linkSpeed());
  bytes += out.print("M");
  if (fullDuplex()) {
    bytes += out.print(",FULL_DUPLEX");
  }
  if (autoNegotiation()) {
    bytes += out.print(",AUTO");
  }
  bytes += out.printf(",ADDR:0x%lX", phyAddr());
  return bytes;
}

ETHClass ETH;
