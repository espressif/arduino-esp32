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

extern void tcpipInit();
extern void add_esp_interface_netif(esp_interface_t interface, esp_netif_t* esp_netif); /* from WiFiGeneric */


ETHClass::ETHClass(uint8_t eth_index)
    :_eth_started(false)
    ,_eth_handle(NULL)
    ,_esp_netif(NULL)
    ,_eth_index(eth_index)
    ,_phy_type(ETH_PHY_MAX)
#if ETH_SPI_SUPPORTS_CUSTOM
    ,_spi(NULL)
#endif
    ,_spi_freq_mhz(20)
    ,_pin_cs(-1)
    ,_pin_irq(-1)
    ,_pin_rst(-1)
    ,_pin_sck(-1)
    ,_pin_miso(-1)
    ,_pin_mosi(-1)
#if CONFIG_ETH_USE_ESP32_EMAC
    ,_pin_mcd(-1)
    ,_pin_mdio(-1)
    ,_pin_power(-1)
    ,_pin_rmii_clock(-1)
#endif /* CONFIG_ETH_USE_ESP32_EMAC */
{}

ETHClass::~ETHClass()
{}

bool ETHClass::ethDetachBus(void * bus_pointer){
    ETHClass *bus = (ETHClass *) bus_pointer;
    if(bus->_eth_started){
        bus->end();
    }
    return true;
}

#if CONFIG_ETH_USE_ESP32_EMAC
bool ETHClass::begin(eth_phy_type_t type, uint8_t phy_addr, int mdc, int mdio, int power, eth_clock_mode_t clock_mode)
{
    esp_err_t ret = ESP_OK;
    if(_esp_netif != NULL){
        return true;
    }
    perimanSetBusDeinit(ESP32_BUS_TYPE_ETHERNET_RMII, ETHClass::ethDetachBus);
    perimanSetBusDeinit(ESP32_BUS_TYPE_ETHERNET_CLK, ETHClass::ethDetachBus);
    perimanSetBusDeinit(ESP32_BUS_TYPE_ETHERNET_MCD, ETHClass::ethDetachBus);
    perimanSetBusDeinit(ESP32_BUS_TYPE_ETHERNET_MDIO, ETHClass::ethDetachBus);
    if(power != -1){
        perimanSetBusDeinit(ESP32_BUS_TYPE_ETHERNET_PWR, ETHClass::ethDetachBus);
    }

    tcpipInit();

    eth_esp32_emac_config_t mac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    mac_config.clock_config.rmii.clock_mode = (clock_mode) ? EMAC_CLK_OUT : EMAC_CLK_EXT_IN;
    mac_config.clock_config.rmii.clock_gpio = (1 == clock_mode) ? EMAC_APPL_CLK_OUT_GPIO : (2 == clock_mode) ? EMAC_CLK_OUT_GPIO : (3 == clock_mode) ? EMAC_CLK_OUT_180_GPIO : EMAC_CLK_IN_GPIO;
    mac_config.smi_mdc_gpio_num = digitalPinToGPIONumber(mdc);
    mac_config.smi_mdio_gpio_num = digitalPinToGPIONumber(mdio);

    _pin_mcd = digitalPinToGPIONumber(mdc);
    _pin_mdio = digitalPinToGPIONumber(mdio);
    _pin_rmii_clock = mac_config.clock_config.rmii.clock_gpio;
    _pin_power = digitalPinToGPIONumber(power);

    if(!perimanClearPinBus(_pin_rmii_clock)){ return false; }
    if(!perimanClearPinBus(_pin_mcd)){ return false; }
    if(!perimanClearPinBus(_pin_mdio)){ return false; }
    if(!perimanClearPinBus(ETH_RMII_TX_EN)){ return false; }
    if(!perimanClearPinBus(ETH_RMII_TX0)){ return false; }
    if(!perimanClearPinBus(ETH_RMII_TX1)){ return false; }
    if(!perimanClearPinBus(ETH_RMII_RX0)){ return false; }
    if(!perimanClearPinBus(ETH_RMII_RX1_EN)){ return false; }
    if(!perimanClearPinBus(ETH_RMII_CRS_DV)){ return false; }
    if(_pin_power != -1){
        if(!perimanClearPinBus(_pin_power)){ return false; }
    }

    eth_mac_config_t eth_mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_mac_config.sw_reset_timeout_ms = 1000;

    esp_eth_mac_t * mac = esp_eth_mac_new_esp32(&mac_config, &eth_mac_config);
    if(mac == NULL){
        log_e("esp_eth_mac_new_esp32 failed");
        return false;
    }

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = phy_addr;
    phy_config.reset_gpio_num = _pin_power;

    esp_eth_phy_t *phy = NULL;
    switch(type){
        case ETH_PHY_LAN8720:
            phy = esp_eth_phy_new_lan87xx(&phy_config);
            break;
        case ETH_PHY_TLK110:
            phy = esp_eth_phy_new_ip101(&phy_config);
            break;
        case ETH_PHY_RTL8201:
            phy = esp_eth_phy_new_rtl8201(&phy_config);
            break;
        case ETH_PHY_DP83848:
            phy = esp_eth_phy_new_dp83848(&phy_config);
            break;
        case ETH_PHY_KSZ8041:
            phy = esp_eth_phy_new_ksz80xx(&phy_config);
            break;
        case ETH_PHY_KSZ8081:
            phy = esp_eth_phy_new_ksz80xx(&phy_config);
            break;
        default:
            log_e("Unsupported PHY %d", type);
            break;
    }
    if(phy == NULL){
        log_e("esp_eth_phy_new failed");
        return false;
    }

    _eth_handle = NULL;
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    ret = esp_eth_driver_install(&eth_config, &_eth_handle);
    if(ret != ESP_OK){
        log_e("SPI Ethernet driver install failed: %d", ret);
        return false;
    }
    if(_eth_handle == NULL){
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
    strcat(strcpy(if_key_str, "ETH_"), num_str);
    strcat(strcpy(if_desc_str, "eth"), num_str);

    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
    esp_netif_config.if_key = if_key_str;
    esp_netif_config.if_desc = if_desc_str;
    esp_netif_config.route_prio -= _eth_index*5;

    cfg.base = &esp_netif_config;

    _esp_netif = esp_netif_new(&cfg);

    /* attach Ethernet driver to TCP/IP stack */
    ret = esp_netif_attach(_esp_netif, esp_eth_new_netif_glue(_eth_handle));
    if(ret != ESP_OK){
        log_e("esp_netif_attach failed: %d", ret);
        return false;
    }

    /* attach to WiFiGeneric to receive events */
    add_esp_interface_netif(ESP_IF_ETH, _esp_netif);

    ret = esp_eth_start(_eth_handle);
    if(ret != ESP_OK){
        log_e("esp_eth_start failed: %d", ret);
        return false;
    }
    _eth_started = true;

    if(!perimanSetPinBus(_pin_rmii_clock, ESP32_BUS_TYPE_ETHERNET_CLK, (void *)(this), -1, -1)){ goto err; }
    if(!perimanSetPinBus(_pin_mcd, ESP32_BUS_TYPE_ETHERNET_MCD, (void *)(this), -1, -1)){ goto err; }
    if(!perimanSetPinBus(_pin_mdio,  ESP32_BUS_TYPE_ETHERNET_MDIO, (void *)(this), -1, -1)){ goto err; }

    if(!perimanSetPinBus(ETH_RMII_TX_EN, ESP32_BUS_TYPE_ETHERNET_RMII, (void *)(this), -1, -1)){ goto err; }
    if(!perimanSetPinBus(ETH_RMII_TX0, ESP32_BUS_TYPE_ETHERNET_RMII, (void *)(this), -1, -1)){ goto err; }
    if(!perimanSetPinBus(ETH_RMII_TX1,  ESP32_BUS_TYPE_ETHERNET_RMII, (void *)(this), -1, -1)){ goto err; }
    if(!perimanSetPinBus(ETH_RMII_RX0, ESP32_BUS_TYPE_ETHERNET_RMII, (void *)(this), -1, -1)){ goto err; }
    if(!perimanSetPinBus(ETH_RMII_RX1_EN, ESP32_BUS_TYPE_ETHERNET_RMII, (void *)(this), -1, -1)){ goto err; }
    if(!perimanSetPinBus(ETH_RMII_CRS_DV,  ESP32_BUS_TYPE_ETHERNET_RMII, (void *)(this), -1, -1)){ goto err; }

    if(_pin_power != -1){
        if(!perimanSetPinBus(_pin_power,  ESP32_BUS_TYPE_ETHERNET_PWR, (void *)(this), -1, -1)){ goto err; }
    }
    // holds a few milliseconds to let DHCP start and enter into a good state
    // FIX ME -- adresses issue https://github.com/espressif/arduino-esp32/issues/5733
    delay(50);

    return true;

err:
    log_e("Failed to set all pins bus to ETHERNET");
    ETHClass::ethDetachBus((void *)(this));
    return false;
}
#endif /* CONFIG_ETH_USE_ESP32_EMAC */

#if ETH_SPI_SUPPORTS_CUSTOM
static void *_eth_spi_init(const void *ctx){
    return (void*)ctx;
}

static esp_err_t _eth_spi_deinit(void *ctx){
    return ESP_OK;
}

esp_err_t ETHClass::_eth_spi_read(void *ctx, uint32_t cmd, uint32_t addr, void *data, uint32_t data_len){
    return ((ETHClass*)ctx)->eth_spi_read(cmd, addr, data, data_len);
}

esp_err_t ETHClass::_eth_spi_write(void *ctx, uint32_t cmd, uint32_t addr, const void *data, uint32_t data_len){
    return ((ETHClass*)ctx)->eth_spi_write(cmd, addr, data, data_len);
}

esp_err_t ETHClass::eth_spi_read(uint32_t cmd, uint32_t addr, void *data, uint32_t data_len){
    if(_spi == NULL){
        return ESP_FAIL;
    }
    // log_i(" 0x%04lx 0x%04lx %lu", cmd, addr, data_len);
    _spi->beginTransaction(SPISettings(_spi_freq_mhz * 1000 * 1000, MSBFIRST, SPI_MODE0));
    digitalWrite(_pin_cs, LOW);

#if CONFIG_ETH_SPI_ETHERNET_DM9051
    if(_phy_type == ETH_PHY_DM9051){
        _spi->write(((cmd & 0x01) << 7) | (addr & 0x7F));
    } else
#endif
#if CONFIG_ETH_SPI_ETHERNET_W5500
    if(_phy_type == ETH_PHY_W5500){
        _spi->write16(cmd);
        _spi->write(addr);
    } else
#endif
#if CONFIG_ETH_SPI_ETHERNET_KSZ8851SNL
    if(_phy_type == ETH_PHY_KSZ8851){
        if(cmd > 1){
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

esp_err_t ETHClass::eth_spi_write(uint32_t cmd, uint32_t addr, const void *data, uint32_t data_len){
    if(_spi == NULL){
        return ESP_FAIL;
    }
    // log_i("0x%04lx 0x%04lx %lu", cmd, addr, data_len);
    _spi->beginTransaction(SPISettings(_spi_freq_mhz * 1000 * 1000, MSBFIRST, SPI_MODE0));
    digitalWrite(_pin_cs, LOW);

#if CONFIG_ETH_SPI_ETHERNET_DM9051
    if(_phy_type == ETH_PHY_DM9051){
        _spi->write(((cmd & 0x01) << 7) | (addr & 0x7F));
    } else
#endif
#if CONFIG_ETH_SPI_ETHERNET_W5500
    if(_phy_type == ETH_PHY_W5500){
        _spi->write16(cmd);
        _spi->write(addr);
    } else
#endif
#if CONFIG_ETH_SPI_ETHERNET_KSZ8851SNL
    if(_phy_type == ETH_PHY_KSZ8851){
        if(cmd > 1){
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

bool ETHClass::beginSPI(eth_phy_type_t type, uint8_t phy_addr, int cs, int irq, int rst, 
#if ETH_SPI_SUPPORTS_CUSTOM
    SPIClass *spi, 
#endif
    int sck, int miso, int mosi, spi_host_device_t spi_host, uint8_t spi_freq_mhz){
    esp_err_t ret = ESP_OK;

    if(_eth_started || _esp_netif != NULL || _eth_handle != NULL){
        log_w("ETH Already Started");
        return true;
    }
    if(cs < 0 || irq < 0){
        log_e("CS and IRQ pins must be defined!");
        return false;
    }

    perimanSetBusDeinit(ESP32_BUS_TYPE_ETHERNET_SPI, ETHClass::ethDetachBus);

    if(_pin_cs != -1){
        if(!perimanClearPinBus(_pin_cs)){ return false; }
    }
    if(_pin_rst != -1){
        if(!perimanClearPinBus(_pin_rst)){ return false; }
    }
    if(_pin_irq != -1){
        if(!perimanClearPinBus(_pin_irq)){ return false; }
    }
    if(_pin_sck != -1){
        if(!perimanClearPinBus(_pin_sck)){ return false; }
    }
    if(_pin_miso != -1){
        if(!perimanClearPinBus(_pin_miso)){ return false; }
    }
    if(_pin_mosi != -1){
        if(!perimanClearPinBus(_pin_mosi)){ return false; }
    }

#if ETH_SPI_SUPPORTS_CUSTOM
    _spi = spi;
#endif
    if(spi_freq_mhz){
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
    if(_spi != NULL){
        pinMode(_pin_cs, OUTPUT);
        digitalWrite(_pin_cs, HIGH);
        perimanSetPinBusExtraType(_pin_cs, "ETH_CS");
    }
#endif

    // Init SPI bus
    if(_pin_sck >= 0 && _pin_miso >= 0 && _pin_mosi >= 0){
        spi_bus_config_t buscfg = {
            .mosi_io_num = _pin_mosi,
            .miso_io_num = _pin_miso,
            .sclk_io_num = _pin_sck,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
        };
        ret = spi_bus_initialize(spi_host, &buscfg, SPI_DMA_CH_AUTO);
        if(ret != ESP_OK){
            log_e("SPI bus initialize failed: %d", ret);
            return false;
        }
    }

    tcpipInit();

    // Install GPIO ISR handler to be able to service SPI Eth modules interrupts
    ret = gpio_install_isr_service(0);
    if(ret != ESP_OK && ret != ESP_ERR_INVALID_STATE){
        log_e("GPIO ISR handler install failed: %d", ret);
        return false;
    }

    // Init common MAC and PHY configs to default
    eth_mac_config_t eth_mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    // Update PHY config based on board specific configuration
    phy_config.phy_addr = phy_addr;
    phy_config.reset_gpio_num = _pin_rst;

    // Configure SPI interface for specific SPI module
    spi_device_interface_config_t spi_devcfg = {
        .mode = 0,
        .clock_speed_hz = _spi_freq_mhz * 1000 * 1000,
        .input_delay_ns = 20,
        .spics_io_num = _pin_cs,
        .queue_size = 20,
    };

    esp_eth_mac_t *mac = NULL;
    esp_eth_phy_t *phy = NULL;
#if CONFIG_ETH_SPI_ETHERNET_W5500
    if(type == ETH_PHY_W5500){
        eth_w5500_config_t mac_config = ETH_W5500_DEFAULT_CONFIG(spi_host, &spi_devcfg);
        mac_config.int_gpio_num = _pin_irq;
#if ETH_SPI_SUPPORTS_CUSTOM
        if(_spi != NULL){
            mac_config.custom_spi_driver.config = this;
            mac_config.custom_spi_driver.init = _eth_spi_init;
            mac_config.custom_spi_driver.deinit = _eth_spi_deinit;
            mac_config.custom_spi_driver.read = _eth_spi_read;
            mac_config.custom_spi_driver.write = _eth_spi_write;
        }
#endif
        mac = esp_eth_mac_new_w5500(&mac_config, &eth_mac_config);
        phy = esp_eth_phy_new_w5500(&phy_config);
    } else 
#endif
#if CONFIG_ETH_SPI_ETHERNET_DM9051
    if(type == ETH_PHY_DM9051){
        eth_dm9051_config_t mac_config = ETH_DM9051_DEFAULT_CONFIG(spi_host, &spi_devcfg);
        mac_config.int_gpio_num = _pin_irq;
#if ETH_SPI_SUPPORTS_CUSTOM
        if(_spi != NULL){
            mac_config.custom_spi_driver.config = this;
            mac_config.custom_spi_driver.init = _eth_spi_init;
            mac_config.custom_spi_driver.deinit = _eth_spi_deinit;
            mac_config.custom_spi_driver.read = _eth_spi_read;
            mac_config.custom_spi_driver.write = _eth_spi_write;
        }
#endif
        mac = esp_eth_mac_new_dm9051(&mac_config, &eth_mac_config);
        phy = esp_eth_phy_new_dm9051(&phy_config);
    } else
#endif
#if CONFIG_ETH_SPI_ETHERNET_KSZ8851SNL
    if(type == ETH_PHY_KSZ8851){
        eth_ksz8851snl_config_t mac_config = ETH_KSZ8851SNL_DEFAULT_CONFIG(spi_host, &spi_devcfg);
        mac_config.int_gpio_num = _pin_irq;
#if ETH_SPI_SUPPORTS_CUSTOM
        if(_spi != NULL){
            mac_config.custom_spi_driver.config = this;
            mac_config.custom_spi_driver.init = _eth_spi_init;
            mac_config.custom_spi_driver.deinit = _eth_spi_deinit;
            mac_config.custom_spi_driver.read = _eth_spi_read;
            mac_config.custom_spi_driver.write = _eth_spi_write;
        }
#endif
        mac = esp_eth_mac_new_ksz8851snl(&mac_config, &eth_mac_config);
        phy = esp_eth_phy_new_ksz8851snl(&phy_config);
    } else
#endif
    {
        log_e("Unsupported PHY module: %d", (int)type);
        return false;
    }

    // Init Ethernet driver to default and install it
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    ret = esp_eth_driver_install(&eth_config, &_eth_handle);
    if(ret != ESP_OK){
        log_e("SPI Ethernet driver install failed: %d", ret);
        return false;
    }
    if(_eth_handle == NULL){
        log_e("esp_eth_driver_install failed! eth_handle is NULL");
        return false;
    }

    // Derive a new MAC address for this interface
    uint8_t base_mac_addr[ETH_ADDR_LEN];
    ret = esp_efuse_mac_get_default(base_mac_addr);
    if(ret != ESP_OK){
        log_e("Get EFUSE MAC failed: %d", ret);
        return false;
    }
    uint8_t mac_addr[ETH_ADDR_LEN];
    base_mac_addr[ETH_ADDR_LEN - 1] += _eth_index; //Increment by the ETH number
    esp_derive_local_mac(mac_addr, base_mac_addr);

    ret = esp_eth_ioctl(_eth_handle, ETH_CMD_S_MAC_ADDR, mac_addr);
    if(ret != ESP_OK){
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
    strcat(strcpy(if_key_str, "ETH_"), num_str);
    strcat(strcpy(if_desc_str, "eth"), num_str);

    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
    esp_netif_config.if_key = if_key_str;
    esp_netif_config.if_desc = if_desc_str;
    esp_netif_config.route_prio -= _eth_index*5;

    cfg.base = &esp_netif_config;

    _esp_netif = esp_netif_new(&cfg);
    if(_esp_netif == NULL){
        log_e("esp_netif_new failed");
        return false;
    }
    // Attach Ethernet driver to TCP/IP stack
    esp_eth_netif_glue_handle_t new_netif_glue = esp_eth_new_netif_glue(_eth_handle);
    if(new_netif_glue == NULL){
        log_e("esp_eth_new_netif_glue failed");
        return false;
    }

    ret = esp_netif_attach(_esp_netif, new_netif_glue);
    if(ret != ESP_OK){
        log_e("esp_netif_attach failed: %d", ret);
        return false;
    }

    // attach to WiFiGeneric to receive events
    add_esp_interface_netif(ESP_IF_ETH, _esp_netif);

    // Start Ethernet driver state machine
    ret = esp_eth_start(_eth_handle);
    if(ret != ESP_OK){
        log_e("esp_eth_start failed: %d", ret);
        return false;
    }

    _eth_started = true;

    // If Arduino's SPI is used, cs pin is in GPIO mode
#if ETH_SPI_SUPPORTS_CUSTOM
    if(_spi == NULL){
#endif
        if(!perimanSetPinBus(_pin_cs, ESP32_BUS_TYPE_ETHERNET_SPI, (void *)(this), -1, -1)){ goto err; }
#if ETH_SPI_SUPPORTS_CUSTOM
    }
#endif
    if(!perimanSetPinBus(_pin_irq, ESP32_BUS_TYPE_ETHERNET_SPI, (void *)(this), -1, -1)){ goto err; }

    if(_pin_sck != -1){
        if(!perimanSetPinBus(_pin_sck, ESP32_BUS_TYPE_ETHERNET_SPI, (void *)(this), -1, -1)){ goto err; }
    }
    if(_pin_miso != -1){
        if(!perimanSetPinBus(_pin_miso, ESP32_BUS_TYPE_ETHERNET_SPI, (void *)(this), -1, -1)){ goto err; }
    }
    if(_pin_mosi != -1){
        if(!perimanSetPinBus(_pin_mosi,  ESP32_BUS_TYPE_ETHERNET_SPI, (void *)(this), -1, -1)){ goto err; }
    }
    if(_pin_rst != -1){
        if(!perimanSetPinBus(_pin_rst,  ESP32_BUS_TYPE_ETHERNET_SPI, (void *)(this), -1, -1)){ goto err; }
    }

    return true;

err:
    log_e("Failed to set all pins bus to ETHERNET");
    ETHClass::ethDetachBus((void *)(this));
    return false;
}

#if ETH_SPI_SUPPORTS_CUSTOM
bool ETHClass::begin(eth_phy_type_t type, uint8_t phy_addr, int cs, int irq, int rst, SPIClass &spi, uint8_t spi_freq_mhz){

    return beginSPI(type, phy_addr, cs, irq, rst, &spi, -1, -1, -1, SPI2_HOST, spi_freq_mhz);
}
#endif

bool ETHClass::begin(eth_phy_type_t type, uint8_t phy_addr, int cs, int irq, int rst, spi_host_device_t spi_host, int sck, int miso, int mosi, uint8_t spi_freq_mhz){

    return beginSPI(type, phy_addr, cs, irq, rst,
#if ETH_SPI_SUPPORTS_CUSTOM 
        NULL, 
#endif
        sck, miso, mosi, spi_host, spi_freq_mhz);
}

void ETHClass::end(void)
{
    _eth_started = false;

    if(_esp_netif != NULL){
        esp_netif_destroy(_esp_netif);
        _esp_netif = NULL;
    }

    if(_eth_handle != NULL){
        if(esp_eth_stop(_eth_handle) != ESP_OK){
            log_e("Failed to stop Ethernet");
            return;
        }
        if(esp_eth_driver_uninstall(_eth_handle) != ESP_OK){
            log_e("Failed to stop Ethernet");
            return;
        }
        _eth_handle = NULL;
    }

#if ETH_SPI_SUPPORTS_CUSTOM
    _spi = NULL;
#endif

#if CONFIG_ETH_USE_ESP32_EMAC
    if(_pin_rmii_clock != -1 && _pin_mcd != -1 && _pin_mdio != -1){
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

    if(_pin_power != -1){
        perimanClearPinBus(_pin_power);
        _pin_power = -1;
    }
#endif /* CONFIG_ETH_USE_ESP32_EMAC */
    if(_pin_cs != -1){
        perimanClearPinBus(_pin_cs);
        _pin_cs = -1;
    }
    if(_pin_irq != -1){
        perimanClearPinBus(_pin_irq);
        _pin_irq = -1;
    }
    if(_pin_sck != -1){
        perimanClearPinBus(_pin_sck);
        _pin_sck = -1;
    }
    if(_pin_miso != -1){
        perimanClearPinBus(_pin_miso);
        _pin_miso = -1;
    }
    if(_pin_mosi != -1){
        perimanClearPinBus(_pin_mosi);
        _pin_mosi = -1;
    }
    if(_pin_rst != -1){
        perimanClearPinBus(_pin_rst);
        _pin_rst = -1;
    }
}

bool ETHClass::config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1, IPAddress dns2)
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

IPAddress ETHClass::localIP()
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

IPAddress ETHClass::subnetMask()
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

IPAddress ETHClass::gatewayIP()
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

IPAddress ETHClass::dnsIP(uint8_t dns_no)
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

IPAddress ETHClass::broadcastIP()
{
    if(_esp_netif == NULL){
        return IPAddress();
    }
    esp_netif_ip_info_t ip;
    if(esp_netif_get_ip_info(_esp_netif, &ip)){
        return IPAddress();
    }
    return WiFiGenericClass::calculateBroadcast(IPAddress(ip.gw.addr), IPAddress(ip.netmask.addr));
}

IPAddress ETHClass::networkID()
{
    if(_esp_netif == NULL){
        return IPAddress();
    }
    esp_netif_ip_info_t ip;
    if(esp_netif_get_ip_info(_esp_netif, &ip)){
        return IPAddress();
    }
    return WiFiGenericClass::calculateNetworkID(IPAddress(ip.gw.addr), IPAddress(ip.netmask.addr));
}

uint8_t ETHClass::subnetCIDR()
{
    if(_esp_netif == NULL){
        return (uint8_t)0;
    }
    esp_netif_ip_info_t ip;
    if(esp_netif_get_ip_info(_esp_netif, &ip)){
        return (uint8_t)0;
    }
    return WiFiGenericClass::calculateSubnetCIDR(IPAddress(ip.netmask.addr));
}

const char * ETHClass::getHostname()
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

bool ETHClass::setHostname(const char * hostname)
{
    if(_esp_netif == NULL){
        return false;
    }
    return esp_netif_set_hostname(_esp_netif, hostname) == 0;
}

bool ETHClass::enableIpV6()
{
    if(_esp_netif == NULL){
        return false;
    }
    return esp_netif_create_ip6_linklocal(_esp_netif) == 0;
}

IPv6Address ETHClass::localIPv6()
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

const char * ETHClass::ifkey(void)
{
    if(_esp_netif == NULL){
        return "";
    }
    return esp_netif_get_ifkey(_esp_netif);
}

const char * ETHClass::desc(void)
{
    if(_esp_netif == NULL){
        return "";
    }
    return esp_netif_get_desc(_esp_netif);
}

String ETHClass::impl_name(void)
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

bool ETHClass::connected()
{
    return WiFiGenericClass::getStatusBits() & ETH_CONNECTED_BIT;
}

bool ETHClass::hasIP()
{
    return WiFiGenericClass::getStatusBits() & ETH_HAS_IP_BIT;
}

bool ETHClass::linkUp()
{
    if(_esp_netif == NULL){
        return false;
    }
    return esp_netif_is_netif_up(_esp_netif);
}

bool ETHClass::fullDuplex()
{
    if(_eth_handle == NULL){
        return false;
    }
    eth_duplex_t link_duplex;
    esp_eth_ioctl(_eth_handle, ETH_CMD_G_DUPLEX_MODE, &link_duplex);
    return (link_duplex == ETH_DUPLEX_FULL);
}

bool ETHClass::autoNegotiation()
{
    if(_eth_handle == NULL){
        return false;
    }
    bool auto_nego;
    esp_eth_ioctl(_eth_handle, ETH_CMD_G_AUTONEGO, &auto_nego);
    return auto_nego;
}

uint32_t ETHClass::phyAddr()
{
    if(_eth_handle == NULL){
        return 0;
    }
    uint32_t phy_addr;
    esp_eth_ioctl(_eth_handle, ETH_CMD_G_PHY_ADDR, &phy_addr);
    return phy_addr;
}

uint8_t ETHClass::linkSpeed()
{
    if(_eth_handle == NULL){
        return 0;
    }
    eth_speed_t link_speed;
    esp_eth_ioctl(_eth_handle, ETH_CMD_G_SPEED, &link_speed);
    return (link_speed == ETH_SPEED_10M)?10:100;
}

uint8_t * ETHClass::macAddress(uint8_t* mac)
{
    if(_eth_handle == NULL){
        return NULL;
    }
    if(!mac){
        return NULL;
    }
    esp_eth_ioctl(_eth_handle, ETH_CMD_G_MAC_ADDR, mac);
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

void ETHClass::printInfo(Print & out){
    out.print(desc());
    out.print(":");
    if(linkUp()){
        out.print(" <UP");
    } else {
        out.print(" <DOWN");
    }
    out.print(",");
    out.print(linkSpeed());
    out.print("M");
    if(fullDuplex()){
        out.print(",FULL_DUPLEX");
    }
    if(autoNegotiation()){
        out.print(",AUTO");
    }
    out.println(">");

    out.print("      ");
    out.print("ether ");
    out.print(macAddress());
    out.printf(" phy 0x%lX", phyAddr());
    out.println();

    out.print("      ");
    out.print("inet ");
    out.print(localIP());
    out.print(" netmask ");
    out.print(subnetMask());
    out.print(" broadcast ");
    out.print(broadcastIP());
    out.println();

    out.print("      ");
    out.print("gateway ");
    out.print(gatewayIP());
    out.print(" dns ");
    out.print(dnsIP());
    out.println();

    out.println();
}

ETHClass ETH;
