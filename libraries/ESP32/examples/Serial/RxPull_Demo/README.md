# RX Internal Pull Demo

Demonstrates **why** internal pull on the UART **RX** pad matters on split RX/TX pins, using `enableRxInternalPull()`, `begin(invert)`, and `setRxInvert()`.

## Supported SoCs

| SoC | Support | Notes |
|-----|---------|-------|
| ESP32 | ✓ | HP UART GPIO pull |
| ESP32-S2 | ✓ | HP UART GPIO pull |
| ESP32-S3 | ✓ | HP UART GPIO pull |
| ESP32-C2 | ✓ | HP UART GPIO pull |
| ESP32-C3 | ✓ | HP UART GPIO pull |
| ESP32-C5 | ✓ | HP UART; LP UART uses fixed pins |
| ESP32-C6 | ✓ | HP UART; LP UART uses fixed pins |
| ESP32-C61 | ✓ | HP UART; LP UART uses fixed pins |
| ESP32-H2 | ✓ | HP UART GPIO pull |
| ESP32-P4 | ✓ | HP and LP (matrix) RTC pull APIs |

## Scope

- Applies to **split** RX and TX pins only (not one-wire mode).
- Pull direction follows **RX inversion**: pull-up when normal, pull-down when RX is inverted.
- `setTxInvert()` alone does not change RX pull.
- Call `enableRxInternalPull()` **before** `begin()`. Default is **on** (internal pull applied).

## What the demo shows

| Part | Wiring | Point |
|------|--------|-------|
| **A — Floating RX** | **No wire** on UART1 `RX1` | Pull defines idle when nothing drives RX; floating pad may show UART errors or noise |
| **B — Inverted RX** | No wire required | Pull-down tracks `begin(invert)` / `setRxInvert()`; matches RS-232 style idle |

## Wiring

- Leave **UART1 RX1** unconnected (no jumper, no partner device on RX).
- Default pins come from board definitions (`RX1`, `TX1`). Override the `#ifndef` defaults in the sketch if needed.
- On USB Serial, Part A prints the **RX1 GPIO number** to keep unconnected.

## Expected serial output

1. **Startup** — `UART1 pins: RX=… TX=…`
2. **Floating RX (Part A)** — unconnected RX GPIO called out; IO pull state reports `floating` vs `pull-up`; error count during idle may be higher with pull disabled (environment-dependent).
3. **Inverted RX (Part B)** — pull-up for normal RX, pull-down after `begin(invert=true)` or `setRxInvert(true)`.

## Usage

1. Open `RxPull_Demo.ino` in the Arduino IDE.
2. Select your ESP32 board and upload.
3. Open Serial Monitor at **115200** baud.
4. Leave RX1 unconnected and read the Part A / Part B results.

## Real-world use cases

- **Unconnected or tri-stated RX** — GPS/radio/modem RX before attach, or transceiver high-Z: pull prevents false BREAK/framing from undefined idle.
- **Inverted RX** — RS-232 or inverted PHY: pull-down matches electrical idle LOW.

For same-pin half-duplex buses, see `OneWire_UART_Demo` (same-pin auto-enables one-wire; internal pull is disabled there by design).
