# One-Wire UART Demo

Demonstrates **one-wire mode**: RX and TX on the **same GPIO** using `enableOneWireMode(true)`.

## Supported SoCs

| SoC | One-wire on HP UART | Notes |
|-----|---------------------|-------|
| ESP32 | ✓ | GPIO matrix |
| ESP32-S2 | ✓ | GPIO matrix |
| ESP32-S3 | ✓ | GPIO matrix |
| ESP32-C2 | ✓ | GPIO matrix |
| ESP32-C3 | ✓ | GPIO matrix |
| ESP32-C5 | ✓ | HP UART only |
| ESP32-C6 | ✓ | HP UART only |
| ESP32-C61 | ✓ | HP UART only |
| ESP32-H2 | ✓ | GPIO matrix |
| ESP32-P4 | ✓ | HP UART and LP UART (GPIO matrix) |

LP UART on **ESP32-C5/C6/C61** uses fixed RX/TX pins — one-wire is **not** supported there.

## What the demo shows

| Part | UARTs | Connection | Purpose |
|------|-------|------------|---------|
| **A — Self loopback** | UART1 only | Same GPIO for RX+TX | Proves one-wire TX/RX on one pad |
| **B — Peer loopback** | UART1 → UART2 | Two one-wire GPIOs | Half-duplex bus between two UART instances |

Part B runs only when the board has **3+ HP UARTs** and defines `RX2` (same guard as `IrdaMode_DualUART_Demo` / `RxPull_Demo`).

## Requirements

- `enableOneWireMode(true)` **before** `begin()`
- `UART_MODE_UART` only (not RS485, not IrDA)
- Same GPIO for RX and TX in `setPins(p, p)` or `begin(..., p, p)`
- **Half-duplex traffic** — do not transmit from both UARTs at once

## Loopback options

Set `USE_INTERNAL_LOOPBACK` at the top of the sketch:

| Value | Part A | Part B |
|-------|--------|--------|
| `1` (default) | `uart_internal_loopback(1, ONEWIRE_PIN1)` | `uart_internal_loopback(1, ONEWIRE_PIN2)` routes UART1 TX to UART2’s GPIO |
| `0` | Optional jumper on ONEWIRE_PIN1 | Jumper **ONEWIRE_PIN1 ↔ ONEWIRE_PIN2** |

For external bus wiring, add a **pull-up** on the shared line (open-drain / multi-drop style). Push-pull one-wire on two GPIOs wired together can work for this demo if only one side drives at a time.

On USB Serial, the sketch prints **`USE_INTERNAL_LOOPBACK`**, the **GPIO numbers** for `ONEWIRE_PIN1` / `ONEWIRE_PIN2`, and wiring instructions (no jumper vs external bus) before each part runs.

## Pins

| Define | Default | Role |
|--------|---------|------|
| `ONEWIRE_PIN1` | `SDA` (board `pins_arduino.h`) | UART1 one-wire GPIO |
| `ONEWIRE_PIN2` | `RX2` | UART2 one-wire GPIO (Part B) |

Change the `#ifndef` defaults for your board if those pads are unavailable.

## Electrical warning

ESP-IDF documents that routing TX output and RX input to one GPIO can cause **pad conflicts**. Recommended practices:

- Half-duplex protocol (never drive TX while listening on the same peer)
- Open-drain TX with external pull-up on shared bus lines
- **Not** a replacement for RS485 (which needs separate TX, RX, and RTS to a transceiver)

Internal pull on RX is **disabled** in one-wire mode.

## Usage

1. Upload `OneWire_UART_Demo.ino`.
2. Open Serial Monitor at **115200** baud.
3. Read the startup lines: one-wire **GPIO numbers**, `USE_INTERNAL_LOOPBACK` mode, and Part A/B wiring summary.
4. If `USE_INTERNAL_LOOPBACK=0`, connect the jumper(s) shown on Serial Monitor, then reset or re-upload.
5. Read Part A/B self-test results on startup.
6. **Part B interactive:** type characters — they are sent on UART1; bytes received on UART2 are printed to USB Serial.
7. **Part A only (2-UART boards without Part B):** typed characters echo on UART1’s one-wire GPIO.

## Expected serial output

1. **Startup** — `ONEWIRE_PIN1` / `ONEWIRE_PIN2` GPIO numbers, loopback mode banner, Part A wiring line, Part B preview (when compiled).
2. **Part A** — `"ONEWIRE"` received on UART1 self loopback, or failure text with GPIO and loopback hint.
3. **Part B** — wiring block (internal or external), then `"ONEWIRE"` on UART2, or failure text with GPIO pair to connect when external loopback is used.

## Wiring (external loopback, `USE_INTERNAL_LOOPBACK = 0`)

The sketch prints the exact GPIO numbers for your board. Conceptually:

```
UART1 GPIO (ONEWIRE_PIN1) ---- bus ---- UART2 GPIO (ONEWIRE_PIN2)
                                |
                           pull-up (recommended)
```

For Part A self-test only, you can optionally short `ONEWIRE_PIN1` to itself; Part B needs the inter-UART jumper above.

## Real-world note

For same-pin half-duplex on a **single** UART pad, Part A is the relevant model. For a **bus** between two UART instances, see Part B and keep half-duplex discipline.

For split-pin RX pull behavior, see `RxPull_Demo`.
