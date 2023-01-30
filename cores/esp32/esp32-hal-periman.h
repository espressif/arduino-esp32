/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
	ESP32_BUS_TYPE_INIT, 		// IO has not been attached to a bus yet
	ESP32_BUS_TYPE_GPIO, 		// IO is used as GPIO
	ESP32_BUS_TYPE_SIGMADELTA, 	// IO is used as SigmeDelta output
	ESP32_BUS_TYPE_ADC_ONESHOT, // IO is used as ADC OneShot input
	ESP32_BUS_TYPE_ADC_CONT, 	// IO is used as ADC continuous input
	ESP32_BUS_TYPE_DAC_ONESHOT, // IO is used as DAC OneShot output
	ESP32_BUS_TYPE_DAC_CONT, 	// IO is used as DAC continuous output
	ESP32_BUS_TYPE_DAC_COSINE, 	// IO is used as DAC cosine output
	ESP32_BUS_TYPE_RMT_TX, 		// IO is used as RMT output
	ESP32_BUS_TYPE_RMT_RX, 		// IO is used as RMT input
	ESP32_BUS_TYPE_I2S_STD, 	// IO is used as I2S STD pin
	ESP32_BUS_TYPE_I2S_PDM, 	// IO is used as I2S PDM pin
	ESP32_BUS_TYPE_I2S_TDM, 	// IO is used as I2S TDM pin
	ESP32_BUS_TYPE_UART, 		// IO is used as UART pin
	ESP32_BUS_TYPE_I2C_MASTER, 	// IO is used as I2C master pin
	ESP32_BUS_TYPE_I2C_SLAVE, 	// IO is used as I2C slave pin
	ESP32_BUS_TYPE_SPI_MASTER, 	// IO is used as SPI master pin
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

#ifdef __cplusplus
}
#endif
