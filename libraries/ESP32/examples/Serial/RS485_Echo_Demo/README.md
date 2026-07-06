# RS485 Echo Demo

Relays data between USB Serial and an **RS485 half-duplex** bus using `UART_MODE_RS485_HALF_DUPLEX`.

- USB `Serial`: **115200** baud (debug / Serial Monitor)
- RS485 `Serial1`: **9600** baud (bus side, as configured in the sketch)

## Pin Requirements

RS485 requires **separate** GPIO pins:

| Signal | Role |
|--------|------|
| RX | UART receive from transceiver |
| TX | UART transmit to transceiver |
| RTS | Drives transceiver DE/~RE (direction) |

**One-wire mode (`enableOneWireMode`) is not compatible with RS485.** Do not use the same GPIO for RX and TX; use an external RS485 transceiver (e.g. MAX485) with distinct TX, RX, and direction control.

## Wiring

See the sketch header and [ESP-IDF RS485 documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/uart.html#rs485-driver-hardware-connection).

Default pins in the sketch:

- RX: GPIO 16
- TX: GPIO 5
- RTS: GPIO 4

Adjust for your board and transceiver.

## Usage

1. Connect an RS485 transceiver to the configured pins.
2. Upload `RS485_Echo_Demo.ino`.
3. Open Serial Monitor at **115200** baud (USB `Serial`).
4. Connect a second RS485 node on the bus at **9600** baud (UART1 / `Serial1` rate in the sketch).
5. Data typed on either side appears on the other.
