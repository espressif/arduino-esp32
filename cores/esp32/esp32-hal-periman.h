/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __cplusplus
extern "C"
{
#endif

#include "soc/soc_caps.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
	ESP32_BUS_TYPE_INIT, 		// IO has not been attached to a bus yet
	ESP32_BUS_TYPE_GPIO, 		// IO is used as GPIO
	ESP32_BUS_TYPE_UART_RX,  	// IO is used as UART RX pin
	ESP32_BUS_TYPE_UART_TX, 	// IO is used as UART TX pin
	ESP32_BUS_TYPE_UART_CTS,	// IO is used as UART CTS pin
	ESP32_BUS_TYPE_UART_RTS,	// IO is used as UART RTS pin
#if SOC_SDM_SUPPORTED
	ESP32_BUS_TYPE_SIGMADELTA, 	// IO is used as SigmeDelta output
#endif
#if SOC_ADC_SUPPORTED
	ESP32_BUS_TYPE_ADC_ONESHOT, // IO is used as ADC OneShot input
	ESP32_BUS_TYPE_ADC_CONT, 	// IO is used as ADC continuous input
#endif
#if SOC_DAC_SUPPORTED
	ESP32_BUS_TYPE_DAC_ONESHOT, // IO is used as DAC OneShot output
	ESP32_BUS_TYPE_DAC_CONT, 	// IO is used as DAC continuous output
	ESP32_BUS_TYPE_DAC_COSINE, 	// IO is used as DAC cosine output
#endif
#if SOC_LEDC_SUPPORTED
	ESP32_BUS_TYPE_LEDC, 		// IO is used as LEDC output
#endif
#if SOC_RMT_SUPPORTED
	ESP32_BUS_TYPE_RMT_TX, 		// IO is used as RMT output
	ESP32_BUS_TYPE_RMT_RX, 		// IO is used as RMT input
#endif
#if SOC_I2S_SUPPORTED
	ESP32_BUS_TYPE_I2S_STD, 	// IO is used as I2S STD pin
	ESP32_BUS_TYPE_I2S_TDM, 	// IO is used as I2S TDM pin
	ESP32_BUS_TYPE_I2S_PDM_TX, 	// IO is used as I2S PDM pin
	ESP32_BUS_TYPE_I2S_PDM_RX, 	// IO is used as I2S PDM pin
#endif
#if SOC_I2C_SUPPORTED
	ESP32_BUS_TYPE_I2C_MASTER, 	// IO is used as I2C master pin
	ESP32_BUS_TYPE_I2C_SLAVE, 	// IO is used as I2C slave pin
#endif
#if SOC_GPSPI_SUPPORTED
	ESP32_BUS_TYPE_SPI_MASTER, 	// IO is used as SPI master pin
#endif
#if SOC_SDMMC_HOST_SUPPORTED
	ESP32_BUS_TYPE_SDMMC, 		// IO is used as SDMMC pin
#endif
#if SOC_TOUCH_SENSOR_SUPPORTED
	ESP32_BUS_TYPE_TOUCH, 		// IO is used as TOUCH pin
#endif
#if SOC_USB_SERIAL_JTAG_SUPPORTED || SOC_USB_OTG_SUPPORTED
	ESP32_BUS_TYPE_USB, 		// IO is used as USB pin
#endif
#if SOC_GPSPI_SUPPORTED
	ESP32_BUS_TYPE_ETHERNET,	// IO is used as ETHERNET-RMII pin
#endif
	ESP32_BUS_TYPE_MAX
} peripheral_bus_type_t;

typedef bool (*peripheral_bus_deinit_cb_t)(void * bus);

// Sets the bus type and bus handle for given pin.
bool perimanSetPinBus(uint8_t pin, peripheral_bus_type_t type, void * bus);

// Returns handle of the bus for the given pin if type of bus matches. NULL otherwise
void * perimanGetPinBus(uint8_t pin, peripheral_bus_type_t type);

// Returns the type of the bus for the given pin if attached. ESP32_BUS_TYPE_MAX otherwise
peripheral_bus_type_t perimanGetPinBusType(uint8_t pin);

// Sets the peripheral destructor callback. Used to destroy bus when pin is assigned another function
bool perimanSetBusDeinit(peripheral_bus_type_t type, peripheral_bus_deinit_cb_t cb);

// Check if given pin is a valid GPIO number 
bool perimanPinIsValid(uint8_t pin);

#ifdef __cplusplus
}
#endif
