/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp32-hal-log.h"
#include "esp32-hal-periman.h"
#include "esp_bit_defs.h"

typedef struct ATTR_PACKED {
	peripheral_bus_type_t type;
	const char* extra_type;
	void * bus;
	int8_t bus_num;
	int8_t bus_channel;
} peripheral_pin_item_t;

static peripheral_bus_deinit_cb_t deinit_functions[ESP32_BUS_TYPE_MAX];
static peripheral_pin_item_t pins[SOC_GPIO_PIN_COUNT];

#define GPIO_NOT_VALID(p) ((p >= SOC_GPIO_PIN_COUNT) || ((SOC_GPIO_VALID_GPIO_MASK & (1ULL << p)) == 0))

const char* perimanGetTypeName(peripheral_bus_type_t type) {
	switch(type) {
		case ESP32_BUS_TYPE_INIT: return "INIT";
		case ESP32_BUS_TYPE_GPIO: return "GPIO";
		case ESP32_BUS_TYPE_UART_RX: return "UART_RX";
		case ESP32_BUS_TYPE_UART_TX: return "UART_TX";
		case ESP32_BUS_TYPE_UART_CTS: return "UART_CTS";
		case ESP32_BUS_TYPE_UART_RTS: return "UART_RTS";
#if SOC_SDM_SUPPORTED
		case ESP32_BUS_TYPE_SIGMADELTA: return "SIGMADELTA";
#endif
#if SOC_ADC_SUPPORTED
		case ESP32_BUS_TYPE_ADC_ONESHOT: return "ADC_ONESHOT";
		case ESP32_BUS_TYPE_ADC_CONT: return "ADC_CONT";
#endif
#if SOC_DAC_SUPPORTED
		case ESP32_BUS_TYPE_DAC_ONESHOT: return "DAC_ONESHOT";
		case ESP32_BUS_TYPE_DAC_CONT: return "DAC_CONT";
		case ESP32_BUS_TYPE_DAC_COSINE: return "DAC_COSINE";
#endif
#if SOC_LEDC_SUPPORTED
		case ESP32_BUS_TYPE_LEDC: return "LEDC";
#endif
#if SOC_RMT_SUPPORTED
		case ESP32_BUS_TYPE_RMT_TX: return "RMT_TX";
		case ESP32_BUS_TYPE_RMT_RX: return "RMT_RX";
#endif
#if SOC_I2S_SUPPORTED
		case ESP32_BUS_TYPE_I2S_STD_MCLK: return "I2S_STD_MCLK";
		case ESP32_BUS_TYPE_I2S_STD_BCLK: return "I2S_STD_BCLK";
		case ESP32_BUS_TYPE_I2S_STD_WS: return "I2S_STD_WS";
		case ESP32_BUS_TYPE_I2S_STD_DOUT: return "I2S_STD_DOUT";
		case ESP32_BUS_TYPE_I2S_STD_DIN: return "I2S_STD_DIN";
		case ESP32_BUS_TYPE_I2S_TDM_MCLK: return "I2S_TDM_MCLK";
		case ESP32_BUS_TYPE_I2S_TDM_BCLK: return "I2S_TDM_BCLK";
		case ESP32_BUS_TYPE_I2S_TDM_WS: return "I2S_TDM_WS";
		case ESP32_BUS_TYPE_I2S_TDM_DOUT: return "I2S_TDM_DOUT";
		case ESP32_BUS_TYPE_I2S_TDM_DIN: return "I2S_TDM_DIN";
		case ESP32_BUS_TYPE_I2S_PDM_TX_CLK: return "I2S_PDM_TX_CLK";
		case ESP32_BUS_TYPE_I2S_PDM_TX_DOUT0: return "I2S_PDM_TX_DOUT0";
		case ESP32_BUS_TYPE_I2S_PDM_TX_DOUT1: return "I2S_PDM_TX_DOUT1";
		case ESP32_BUS_TYPE_I2S_PDM_RX_CLK: return "I2S_PDM_RX_CLK";
		case ESP32_BUS_TYPE_I2S_PDM_RX_DIN0: return "I2S_PDM_RX_DIN0";
		case ESP32_BUS_TYPE_I2S_PDM_RX_DIN1: return "I2S_PDM_RX_DIN1";
		case ESP32_BUS_TYPE_I2S_PDM_RX_DIN2: return "I2S_PDM_RX_DIN2";
		case ESP32_BUS_TYPE_I2S_PDM_RX_DIN3: return "I2S_PDM_RX_DIN3";
#endif
#if SOC_I2C_SUPPORTED
		case ESP32_BUS_TYPE_I2C_MASTER_SDA: return "I2C_MASTER_SDA";
		case ESP32_BUS_TYPE_I2C_MASTER_SCL: return "I2C_MASTER_SCL";
		case ESP32_BUS_TYPE_I2C_SLAVE_SDA: return "I2C_SLAVE_SDA";
		case ESP32_BUS_TYPE_I2C_SLAVE_SCL: return "I2C_SLAVE_SCL";
#endif
#if SOC_GPSPI_SUPPORTED
		case ESP32_BUS_TYPE_SPI_MASTER_SCK: return "SPI_MASTER_SCK";
		case ESP32_BUS_TYPE_SPI_MASTER_MISO: return "SPI_MASTER_MISO";
		case ESP32_BUS_TYPE_SPI_MASTER_MOSI: return "SPI_MASTER_MOSI";
		case ESP32_BUS_TYPE_SPI_MASTER_CS: return "SPI_MASTER_CS";
#endif
#if SOC_SDMMC_HOST_SUPPORTED
		case ESP32_BUS_TYPE_SDMMC_CLK: return "SDMMC_CLK";
		case ESP32_BUS_TYPE_SDMMC_CMD: return "SDMMC_CMD";
		case ESP32_BUS_TYPE_SDMMC_D0: return "SDMMC_D0";
		case ESP32_BUS_TYPE_SDMMC_D1: return "SDMMC_D1";
		case ESP32_BUS_TYPE_SDMMC_D2: return "SDMMC_D2";
		case ESP32_BUS_TYPE_SDMMC_D3: return "SDMMC_D3";
#endif
#if SOC_TOUCH_SENSOR_SUPPORTED
		case ESP32_BUS_TYPE_TOUCH: return "TOUCH";
#endif
#if SOC_USB_SERIAL_JTAG_SUPPORTED || SOC_USB_OTG_SUPPORTED
		case ESP32_BUS_TYPE_USB_DM: return "USB_DM";
		case ESP32_BUS_TYPE_USB_DP: return "USB_DP";
#endif
#if SOC_GPSPI_SUPPORTED
		case ESP32_BUS_TYPE_ETHERNET_SPI: return "ETHERNET_SPI";
#endif
#if CONFIG_ETH_USE_ESP32_EMAC
		case ESP32_BUS_TYPE_ETHERNET_RMII: return "ETHERNET_RMII";
		case ESP32_BUS_TYPE_ETHERNET_CLK: return "ETHERNET_CLK";
		case ESP32_BUS_TYPE_ETHERNET_MCD: return "ETHERNET_MCD";
		case ESP32_BUS_TYPE_ETHERNET_MDIO: return "ETHERNET_MDIO";
		case ESP32_BUS_TYPE_ETHERNET_PWR: return "ETHERNET_PWR";
#endif
		default: return "UNKNOWN";
    }
}

bool perimanSetPinBus(uint8_t pin, peripheral_bus_type_t type, void * bus, int8_t bus_num, int8_t bus_channel){
	peripheral_bus_type_t otype = ESP32_BUS_TYPE_INIT;
	void * obus = NULL;
	if(GPIO_NOT_VALID(pin)){
		log_e("Invalid pin: %u", pin);
		return false;
	}
	if(type >= ESP32_BUS_TYPE_MAX){
		log_e("Invalid type: %s (%u) when setting pin %u", perimanGetTypeName(type), (unsigned int)type, pin);
		return false;
	}
	if(type > ESP32_BUS_TYPE_GPIO && bus == NULL){
		log_e("Bus is NULL for pin %u with type %s (%u)", pin, perimanGetTypeName(type), (unsigned int)type);
		return false;
	}
	if (type == ESP32_BUS_TYPE_INIT && bus != NULL){
		log_e("Can't set a Bus to INIT Type (pin %u)", pin);
		return false;
	}
	otype = pins[pin].type;
	obus = pins[pin].bus;
	if(type == otype && bus == obus){
		if (type != ESP32_BUS_TYPE_INIT) {
		    log_i("Pin %u already has type %s (%u) with bus %p", pin, perimanGetTypeName(type), (unsigned int)type, bus);
		}
		return true;
	}
	if(obus != NULL){
		if(deinit_functions[otype] == NULL){
			log_e("No deinit function for type %s (%u) (pin %u)", perimanGetTypeName(otype), (unsigned int)otype, pin);
			return false;
		}
		if(!deinit_functions[otype](obus)){
			log_e("Deinit function for previous bus type %s (%u) failed (pin %u)", perimanGetTypeName(otype), (unsigned int)otype, pin);
			return false;
		}
	}
	pins[pin].type = type;
	pins[pin].bus = bus;
	pins[pin].bus_num = bus_num;
	pins[pin].bus_channel = bus_channel;
	pins[pin].extra_type = NULL;
	log_v("Pin %u successfully set to type %s (%u) with bus %p", pin, perimanGetTypeName(type), (unsigned int)type, bus);
	return true;
}

bool perimanSetPinBusExtraType(uint8_t pin, const char* extra_type){
	if(GPIO_NOT_VALID(pin)){
		log_e("Invalid pin: %u", pin);
		return false;
	}
	if (pins[pin].type == ESP32_BUS_TYPE_INIT) {
		log_e("Can't set extra type for Bus INIT Type (pin %u)", pin);
		return false;
	}
	pins[pin].extra_type = extra_type;
	log_v("Successfully set extra_type %s for pin %u", extra_type, pin);
	return true;
}

void * perimanGetPinBus(uint8_t pin, peripheral_bus_type_t type){
	if(GPIO_NOT_VALID(pin)){
		log_e("Invalid pin: %u", pin);
		return NULL;
	}
	if(type >= ESP32_BUS_TYPE_MAX  || type  == ESP32_BUS_TYPE_INIT){
		log_e("Invalid type %s (%u) for pin %u", perimanGetTypeName(type), (unsigned int)type, pin);
		return NULL;
	}
	if(pins[pin].type == type){
		return pins[pin].bus;
	}
	return NULL;
}

peripheral_bus_type_t perimanGetPinBusType(uint8_t pin){
	if(GPIO_NOT_VALID(pin)){
		log_e("Invalid pin: %u", pin);
		return ESP32_BUS_TYPE_MAX;
	}
	return pins[pin].type;
}

const char* perimanGetPinBusExtraType(uint8_t pin){
	if(GPIO_NOT_VALID(pin)){
		log_e("Invalid pin: %u", pin);
		return NULL;
	}
	return pins[pin].extra_type;
}

int8_t perimanGetPinBusNum(uint8_t pin){
	if(GPIO_NOT_VALID(pin)){
		log_e("Invalid pin: %u", pin);
		return -1;
	}
	return pins[pin].bus_num;
}

int8_t perimanGetPinBusChannel(uint8_t pin){
	if(GPIO_NOT_VALID(pin)){
		log_e("Invalid pin: %u", pin);
		return -1;
	}
	return pins[pin].bus_channel;
}

bool perimanSetBusDeinit(peripheral_bus_type_t type, peripheral_bus_deinit_cb_t cb){
	if(type >= ESP32_BUS_TYPE_MAX || type == ESP32_BUS_TYPE_INIT){
		log_e("Invalid type: %s (%u)", perimanGetTypeName(type), (unsigned int)type);
		return false;
	}
	if(cb == NULL){
		log_e("Callback is NULL when setting deinit function for type %s (%u)", perimanGetTypeName(type), (unsigned int)type);
		return false;
	}
	deinit_functions[type] = cb;
	log_v("Deinit function for type %s (%u) successfully set to %p", perimanGetTypeName(type), (unsigned int)type, cb);
	return true;
}

bool perimanPinIsValid(uint8_t pin){
	return !(GPIO_NOT_VALID(pin));
}
