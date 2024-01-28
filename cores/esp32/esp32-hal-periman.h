/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "soc/soc_caps.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define perimanClearPinBus(p) perimanSetPinBus(p, ESP32_BUS_TYPE_INIT, NULL, -1, -1)

  typedef enum {
    ESP32_BUS_TYPE_INIT,      // IO has not been attached to a bus yet
    ESP32_BUS_TYPE_GPIO,      // IO is used as GPIO
    ESP32_BUS_TYPE_UART_RX,   // IO is used as UART RX pin
    ESP32_BUS_TYPE_UART_TX,   // IO is used as UART TX pin
    ESP32_BUS_TYPE_UART_CTS,  // IO is used as UART CTS pin
    ESP32_BUS_TYPE_UART_RTS,  // IO is used as UART RTS pin
#if SOC_SDM_SUPPORTED
    ESP32_BUS_TYPE_SIGMADELTA,  // IO is used as SigmeDelta output
#endif
#if SOC_ADC_SUPPORTED
    ESP32_BUS_TYPE_ADC_ONESHOT,  // IO is used as ADC OneShot input
    ESP32_BUS_TYPE_ADC_CONT,     // IO is used as ADC continuous input
#endif
#if SOC_DAC_SUPPORTED
    ESP32_BUS_TYPE_DAC_ONESHOT,  // IO is used as DAC OneShot output
    ESP32_BUS_TYPE_DAC_CONT,     // IO is used as DAC continuous output
    ESP32_BUS_TYPE_DAC_COSINE,   // IO is used as DAC cosine output
#endif
#if SOC_LEDC_SUPPORTED
    ESP32_BUS_TYPE_LEDC,  // IO is used as LEDC output
#endif
#if SOC_RMT_SUPPORTED
    ESP32_BUS_TYPE_RMT_TX,  // IO is used as RMT output
    ESP32_BUS_TYPE_RMT_RX,  // IO is used as RMT input
#endif
#if SOC_I2S_SUPPORTED
    ESP32_BUS_TYPE_I2S_STD_MCLK,  // IO is used as I2S STD MCLK pin
    ESP32_BUS_TYPE_I2S_STD_BCLK,  // IO is used as I2S STD BCLK pin
    ESP32_BUS_TYPE_I2S_STD_WS,    // IO is used as I2S STD WS pin
    ESP32_BUS_TYPE_I2S_STD_DOUT,  // IO is used as I2S STD DOUT pin
    ESP32_BUS_TYPE_I2S_STD_DIN,   // IO is used as I2S STD DIN pin

    ESP32_BUS_TYPE_I2S_TDM_MCLK,  // IO is used as I2S TDM MCLK pin
    ESP32_BUS_TYPE_I2S_TDM_BCLK,  // IO is used as I2S TDM BCLK pin
    ESP32_BUS_TYPE_I2S_TDM_WS,    // IO is used as I2S TDM WS pin
    ESP32_BUS_TYPE_I2S_TDM_DOUT,  // IO is used as I2S TDM DOUT pin
    ESP32_BUS_TYPE_I2S_TDM_DIN,   // IO is used as I2S TDM DIN pin

    ESP32_BUS_TYPE_I2S_PDM_TX_CLK,    // IO is used as I2S PDM CLK pin
    ESP32_BUS_TYPE_I2S_PDM_TX_DOUT0,  // IO is used as I2S PDM DOUT0 pin
    ESP32_BUS_TYPE_I2S_PDM_TX_DOUT1,  // IO is used as I2S PDM DOUT1 pin

    ESP32_BUS_TYPE_I2S_PDM_RX_CLK,   // IO is used as I2S PDM CLK pin
    ESP32_BUS_TYPE_I2S_PDM_RX_DIN0,  // IO is used as I2S PDM DIN0 pin
    ESP32_BUS_TYPE_I2S_PDM_RX_DIN1,  // IO is used as I2S PDM DIN1 pin
    ESP32_BUS_TYPE_I2S_PDM_RX_DIN2,  // IO is used as I2S PDM DIN2 pin
    ESP32_BUS_TYPE_I2S_PDM_RX_DIN3,  // IO is used as I2S PDM DIN3 pin
#endif
#if SOC_I2C_SUPPORTED
    ESP32_BUS_TYPE_I2C_MASTER_SDA,  // IO is used as I2C master SDA pin
    ESP32_BUS_TYPE_I2C_MASTER_SCL,  // IO is used as I2C master SCL pin
    ESP32_BUS_TYPE_I2C_SLAVE_SDA,   // IO is used as I2C slave SDA pin
    ESP32_BUS_TYPE_I2C_SLAVE_SCL,   // IO is used as I2C slave SCL pin
#endif
#if SOC_GPSPI_SUPPORTED
    ESP32_BUS_TYPE_SPI_MASTER_SCK,   // IO is used as SPI master SCK pin
    ESP32_BUS_TYPE_SPI_MASTER_MISO,  // IO is used as SPI master MISO pin
    ESP32_BUS_TYPE_SPI_MASTER_MOSI,  // IO is used as SPI master MOSI pin
    ESP32_BUS_TYPE_SPI_MASTER_SS,    // IO is used as SPI master SS pin
#endif
#if SOC_SDMMC_HOST_SUPPORTED
    ESP32_BUS_TYPE_SDMMC_CLK,  // IO is used as SDMMC CLK pin
    ESP32_BUS_TYPE_SDMMC_CMD,  // IO is used as SDMMC CMD pin
    ESP32_BUS_TYPE_SDMMC_D0,   // IO is used as SDMMC D0 pin
    ESP32_BUS_TYPE_SDMMC_D1,   // IO is used as SDMMC D1 pin
    ESP32_BUS_TYPE_SDMMC_D2,   // IO is used as SDMMC D2 pin
    ESP32_BUS_TYPE_SDMMC_D3,   // IO is used as SDMMC D3 pin
#endif
#if SOC_TOUCH_SENSOR_SUPPORTED
    ESP32_BUS_TYPE_TOUCH,  // IO is used as TOUCH pin
#endif
#if SOC_USB_SERIAL_JTAG_SUPPORTED || SOC_USB_OTG_SUPPORTED
    ESP32_BUS_TYPE_USB_DM,  // IO is used as USB DM (+) pin
    ESP32_BUS_TYPE_USB_DP,  // IO is used as USB DP (-) pin
#endif
#if SOC_GPSPI_SUPPORTED
    ESP32_BUS_TYPE_ETHERNET_SPI,  // IO is used as ETHERNET SPI pin
#endif
#if CONFIG_ETH_USE_ESP32_EMAC
    ESP32_BUS_TYPE_ETHERNET_RMII,  // IO is used as ETHERNET RMII pin
    ESP32_BUS_TYPE_ETHERNET_CLK,   // IO is used as ETHERNET CLK pin
    ESP32_BUS_TYPE_ETHERNET_MCD,   // IO is used as ETHERNET MCD pin
    ESP32_BUS_TYPE_ETHERNET_MDIO,  // IO is used as ETHERNET MDIO pin
    ESP32_BUS_TYPE_ETHERNET_PWR,   // IO is used as ETHERNET PWR pin
#endif
    ESP32_BUS_TYPE_MAX
  } peripheral_bus_type_t;

  typedef bool (*peripheral_bus_deinit_cb_t)(void* bus);

  const char* perimanGetTypeName(peripheral_bus_type_t type);

  // Sets the bus type, bus handle, bus number and bus channel for given pin.
  bool perimanSetPinBus(uint8_t pin, peripheral_bus_type_t type, void* bus, int8_t bus_num, int8_t bus_channel);

  // Returns handle of the bus for the given pin if type of bus matches. NULL otherwise
  void* perimanGetPinBus(uint8_t pin, peripheral_bus_type_t type);

  // Returns the type of the bus for the given pin if attached. ESP32_BUS_TYPE_MAX otherwise
  peripheral_bus_type_t perimanGetPinBusType(uint8_t pin);

  // Returns the bus number or unit of the bus for the given pin if set. -1 otherwise
  int8_t perimanGetPinBusNum(uint8_t pin);

  // Returns the bus channel of the bus for the given pin if set. -1 otherwise
  int8_t perimanGetPinBusChannel(uint8_t pin);

  // Sets the peripheral destructor callback. Used to destroy bus when pin is assigned another function
  bool perimanSetBusDeinit(peripheral_bus_type_t type, peripheral_bus_deinit_cb_t cb);

  // Check if given pin is a valid GPIO number
  bool perimanPinIsValid(uint8_t pin);

  // Sets the extra type for non Init bus. Used to customize pin bus name which can be printed by printPerimanInfo().
  bool perimanSetPinBusExtraType(uint8_t pin, const char* extra_type);

  // Returns the extra type of the bus for given pin if set. NULL otherwise
  const char* perimanGetPinBusExtraType(uint8_t pin);

#ifdef __cplusplus
}
#endif
