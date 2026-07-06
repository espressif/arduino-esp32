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
- Call `enableRxInternalPull()` **before** `begin()`.

## What the demo shows

| Part | Wiring | Point |
|------|--------|-------|
| **A — Floating RX** | **No wire** on UART1 `RX1` | Pull defines idle when nothing drives RX; floating pad may show UART errors or noise |
| **B — Inverted RX** | No wire required | Pull-down tracks `begin(invert)` / `setRxInvert()`; matches RS-232 style idle |
| **C — Wired loopback** | Jumper **TX1 → RX2** (3+ UART boards); exact GPIOs printed on Serial | Partner TX holds idle — pull on UART2 RX is often **redundant** but reception still works with pull on or off |

Part C is compiled only when the board defines `RX2` and has three or more HP UARTs (same guard as `IrdaMode_DualUART_Demo`).

## Wiring

### Parts A and B (required)

- Leave **UART1 RX1** unconnected (no jumper, no partner device on RX).
- Default pins come from board definitions (`RX1`, `TX1`). Override the `#ifndef` defaults in the sketch if needed.
- On USB Serial, Part A prints the **RX1 GPIO number** to keep unconnected (for example `No jumper on UART1 RX1 (GPIO 15).`).

### Part C (optional)

Single-board loopback on ESP32-class boards with UART2:

```
UART1 TX1 (GPIO from board) -----> UART2 RX2 (GPIO from board)
```

The sketch prints the **exact GPIO numbers** for your board:

- At startup (after UART1 pin summary): `Part C (after A & B): jumper UART1 TX1 GPIO … -> UART2 RX2 GPIO …`
- Again before Part C runs: a wiring block with `UART1 TX1 (GPIO …) ------> UART2 RX2 (GPIO …)`
- If no data is received: the same GPIO pair is repeated in the failure line

Parts A and B run **without** the Part C jumper. Connect the wire when the wiring block appears (or after reading the startup preview), then **reset or re-upload** before expecting Part C to pass.

Do **not** tie UART1 RX1 for Part A while the Part C jumper is on UART2 RX — run A and B first, add the jumper, then reset for Part C.

## Expected serial output

1. **Startup** — `UART1 pins: RX=… TX=…`; on 3+ UART boards, a one-line Part C jumper preview with GPIO numbers.
2. **Floating RX (Part A)** — unconnected RX GPIO called out; IO pull state reports `floating` vs `pull-up`; error count during idle may be higher with pull disabled (environment-dependent).
3. **Inverted RX (Part B)** — pull-up for normal RX, pull-down after `begin(invert=true)` or `setRxInvert(true)`.
4. **Wired loopback (Part C)** — wiring block with GPIO numbers, then `"RxPull loopback"` received on UART2 with `enableRxInternalPull(true)` and `false` on UART2 RX.

## Usage

1. Open `RxPull_Demo.ino` in the Arduino IDE.
2. Select your ESP32 board and upload.
3. Open Serial Monitor at **115200** baud.
4. Read the printed **GPIO numbers** for your board (startup line and Part C wiring block).
5. For Part C, connect the jumper shown on Serial Monitor (`UART1 TX1` GPIO → `UART2 RX2` GPIO), then reset or re-upload so loopback runs with the wire in place.

Verify pull direction with the printed IO pull state from `gpio_get_io_config()` or a scope on the RX GPIO.

## Real-world use cases

- **Unconnected or tri-stated RX** — GPS/radio/modem RX before attach, or transceiver high-Z: pull prevents false BREAK/framing from undefined idle.
- **Inverted RX** — RS-232 or inverted PHY: pull-down matches electrical idle LOW.
- **Direct TX→RX wire** — partner output drives idle; internal pull is secondary (Part C illustrates this).

For same-pin half-duplex buses, see `OneWire_UART_Demo` (internal pull is disabled there by design).
