#pragma once

// Routines that generate BREAK in the UART for testing purpose

// Forces a BREAK in the line based on SERIAL_8N1 configuration at any baud rate
void uart_send_break(uint8_t uartNum);
// Sends a buffer and at the end of the stream, it generates BREAK in the line
int uart_send_msg_with_break(uint8_t uartNum, uint8_t *msg, size_t msgSize);
