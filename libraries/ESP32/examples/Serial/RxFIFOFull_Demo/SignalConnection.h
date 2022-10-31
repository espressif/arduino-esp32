#pragma once

// Make sure UART's RX signal is connected to TX pin
// This creates a loop that lets us receive anything we send on the UART
void uart_internal_loopback(uint8_t uartNum, int8_t rxPin);