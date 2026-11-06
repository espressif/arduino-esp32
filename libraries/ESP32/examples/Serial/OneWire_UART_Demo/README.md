# One-Wire UART Demo

Demonstrates **one-wire mode**: RX and TX on the **same GPIO**. Same-pin configuration auto-enables one-wire (no separate opt-in API).

This is a single-board example that runs with **no external wiring**. UART1 transmits and receives on one GPIO; the sketch uses the UART internal loopback so UART1 receives its own transmission on that same pad.

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

UART1 sends a test string on a single GPIO and receives it back on the same pad, proving that same-pin RX/TX automatically enables one-wire mode. After the self-test, anything typed in the Serial Monitor is sent on the one-wire GPIO and echoed back.

## How one-wire is enabled

Use the same GPIO for RX and TX — one-wire is automatic:

```cpp
Serial1.begin(115200, SERIAL_8N1, pin, pin);  // RX == TX -> one-wire
```

The pad is configured **open-drain**: TX pulls the line LOW and releases HIGH. On a real one-wire bus, an **external pull-up resistor to 3.3 V** (a suitable conventional value) holds the line idle HIGH. This example needs no external wiring: it routes TX back to RX internally with `uart_internal_loopback()` and enables a weak internal pull-up so released HIGH bits are observable. That internal pull-up is only a test fixture and does not replace the external resistor on a real bus.

## Pins

| Define | Default | Role |
|--------|---------|------|
| `ONEWIRE_PIN` | `SDA` (board `pins_arduino.h`) | UART1 one-wire pad (RX and TX) |

Override `ONEWIRE_PIN` before its `#ifndef` default if the selected pad is unavailable.

## Requirements

- `UART_MODE_UART` only (not RS485, not IrDA)
- Same GPIO for RX and TX in `begin(..., p, p)` — one-wire is automatic
- **Half-duplex traffic** — only one direction transmits at a time
- On a real bus: an **external pull-up to 3.3 V** (internal RX pull is disabled in one-wire mode)

## Why one-wire UART can receive ghost data

In one-wire mode, RX always listens on the same open-drain pin that TX uses. The pad releases HIGH bits, so a pull-up must hold the bus idle HIGH. Any brief LOW or noise looks like a UART start bit and can produce framing errors, BREAK events, or "ghost" bytes.

Main causes:

1. **Missing external pull-up** — open-drain TX releases HIGH and cannot establish idle HIGH by itself.
2. **Startup/mux transients** — attaching the shared pin can briefly look like a LOW start bit.
3. **Self-echo** — TX is also seen on RX. That is expected for intentional transmission.

Mitigations: add the external pull-up, drain RX after `begin()` (`while (available()) read()`), keep traffic half-duplex, and discard the expected self-echo after each write.

## Usage

1. Upload `OneWire_UART_Demo.ino`.
2. Open Serial Monitor at **115200** baud.
3. Read the startup lines and the self-echo result.
4. Type text and press Send — bytes are transmitted on the one-wire GPIO and echoed back.

## Real-world note

For a one-wire **bus** between two separate ESP32-family boards using one signal wire plus common GND and an external pull-up, see [`OneWire_UART_Two_Boards`](../OneWire_UART_Two_Boards/).

For split-pin RX pull behavior, see `RxPull_Demo`.
