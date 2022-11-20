/*
    This is pure IDF code intended to generate BREAK in an UART line
*/

#include "driver/uart.h"

// Forces a BREAK in the line based on SERIAL_8N1 configuration at any baud rate
void uart_send_break(uint8_t uartNum)
{
  uint32_t currentBaudrate = 0;
  uart_get_baudrate(uartNum, &currentBaudrate);
  // calculates 10 bits of breaks in microseconds for baudrates up to 500mbps
  // This is very sensetive timing... it works fine for SERIAL_8N1
  uint32_t breakTime = (uint32_t) (10.0 * (1000000.0 / currentBaudrate));
  uart_set_line_inverse(uartNum, UART_SIGNAL_TXD_INV);
  ets_delay_us(breakTime);
  uart_set_line_inverse(uartNum, UART_SIGNAL_INV_DISABLE);
}

// Sends a buffer and at the end of the stream, it generates BREAK in the line
int uart_send_msg_with_break(uint8_t uartNum, uint8_t *msg, size_t msgSize)
{
  // 12 bits long BREAK for 8N1
  return uart_write_bytes_with_break(uartNum, (const void *)msg, msgSize, 12);
}
