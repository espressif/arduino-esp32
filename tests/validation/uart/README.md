# UART Validation Test

Comprehensive test suite for the `HardwareSerial` (UART) driver covering transmission, baudrate changes, pin management, peripheral manager integration, CPU frequency changes, hardware flow control, and IrDA mode.

## Test Cases

| Test Function | Description |
|---|---|
| `begin_when_running_test` | Call `begin()` twice without crash |
| `basic_transmission_test` | Transmit and receive a message via internal loopback |
| `resize_buffers_test` | Verify RX/TX buffer resize behavior (while running and stopped) |
| `change_baudrate_test` | Change baudrate to 9600 and back, verify transmission at each rate |
| `change_cpu_frequency_test` | Change CPU frequency, verify UART still works at new and original frequency |
| `disabled_uart_calls_test` | Call all UART methods on a stopped UART, verify safe return values |
| `enabled_uart_calls_test` | Call all UART methods on a running UART, verify correct return values |
| `auto_baudrate_test` | Auto baudrate detection (ESP32 and ESP32-S2 only) |
| `periman_test` | Peripheral manager: attach I2C to UART pins, verify UART stops, then re-enable |
| `change_pins_test` | Change UART RX/TX pins and verify transmission on new pins |
| `irda_mode_test` | Set IrDA mode, switch TX/RX directions, verify directional behavior with 2 UARTs |
| `inter_uart_pin_move_test` | Move UART1 pins to UART2's pins, verify UART2 is terminated (requires 2+ UARTs) |
| `same_uart_pin_swap_test` | Swap RX and TX on the same UART, verify it stays running |
| `move_rx_tx_to_cts_rts_test` | Move RX/TX pins to CTS/RTS slots, verify UART is terminated |
| `cross_uart_cts_rts_test` | Assign another UART's pins as CTS/RTS, verify source UART terminates (requires 2+ UARTs) |
| `end_when_stopped_test` | Call `end()` twice without crash |
| `hardware_flow_control_test` | Enable RTS/CTS flow control with internal loopback, verify transmission |

## Requirements

- **Hardware**: Any ESP32 variant with at least 2 UART peripherals (UART0 for reporting, UART1+ for testing)
- **Wokwi/QEMU**: QEMU not supported

## Notes

- UART0 (`Serial`) is reserved for test status reporting; UART1+ are configured with internal loopback (TX routed back to RX) for self-test.
- The `auto_baudrate_test` is only compiled on ESP32 and ESP32-S2 targets.
- Pin assignments vary by target; the sketch uses default `RX1`/`TX1`/`RX2`/`TX2` macros except on ESP32-P4 which uses hardcoded pins 2/3.
- Tests involving pin reassignment between UARTs require at least 2 UART peripherals beyond UART0.
