/*
    This is pure IDF code intended to make an internal loopback connection using IOMUX
    The function uart_internal_loopback() shall be used right after Arduino Serial.begin(...)
    This code "replaces" the physical wiring for connecting TX <--> RX in a loopback
*/

#include "driver/uart.h"
#include "driver/gpio.h"

#include "soc/uart_periph.h"
#include "soc/soc_caps.h"
#include "esp_rom_gpio.h"

// gets the right TX SIGNAL, based on the UART number
#if SOC_UART_NUM > 2
#define UART_TX_SIGNAL(uartNumber) (uartNumber == UART_NUM_0 ? U0TXD_OUT_IDX : (uartNumber == UART_NUM_1 ? U1TXD_OUT_IDX : U2TXD_OUT_IDX))
#else
#define UART_TX_SIGNAL(uartNumber) (uartNumber == UART_NUM_0 ? U0TXD_OUT_IDX : U1TXD_OUT_IDX)
#endif
/*
   Make sure UART's RX signal is connected to TX pin
   This creates a loop that lets us receive anything we send on the UART
*/
void uart_internal_loopback(uint8_t uartNum, int8_t rxPin)
{
  if (uartNum > SOC_UART_NUM - 1 || !GPIO_IS_VALID_GPIO(rxPin)) return;
  esp_rom_gpio_connect_out_signal(rxPin, UART_TX_SIGNAL(uartNum), false, false);
}

