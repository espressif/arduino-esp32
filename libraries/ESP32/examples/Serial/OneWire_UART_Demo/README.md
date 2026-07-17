# One-Wire UART Demo

Demonstrates **one-wire mode**: RX and TX on the **same GPIO**. Same-pin configuration auto-enables one-wire (no separate opt-in API).

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
| **B — Cross connection** | UART1 ↔ UART2 | One signal wire between two one-wire GPIOs | Bidirectional half-duplex request/reply between two UART instances |

Part B runs only when the board has **3+ HP UARTs** and defines `RX2` (UART0 remains available for Serial Monitor output). UART1 first sends a request to UART2; UART2 then sends a reply to UART1 over the same wire.

## Requirements

- `UART_MODE_UART` only (not RS485, not IrDA)
- Same GPIO for RX and TX in `setPins(p, p)` or `begin(..., p, p)` — one-wire is automatic
- `-1` keep-current that makes effective RX equal TX also enables one-wire (e.g. RX=8 TX=9 then `setPins(-1, 8)`)
- **Half-duplex traffic** — only one UART TX output may be enabled at a time

## Loopback options

Set `USE_INTERNAL_LOOPBACK` at the top of the sketch:

| Value | Part A | Part B |
|-------|--------|--------|
| `1` (default) | `uart_internal_loopback(1, ONEWIRE_PIN1)` | No jumper required; an existing jumper is optional and redundant |
| `0` | Optional jumper on ONEWIRE_PIN1 | One signal wire: **ONEWIRE_PIN1 ↔ ONEWIRE_PIN2**; both directions use it in turn |

With `USE_INTERNAL_LOOPBACK=1`, the sketch synchronizes GPIO-matrix output routing before each turn:

- **Turn 1:** both `ONEWIRE_PIN1` and `ONEWIRE_PIN2` carry UART1 TX.
- **Turn 2:** both pins carry UART2 TX.
- Direction changes happen after `flush()`, while UART is idle HIGH.

Consequently, an installed jumper connects two pads carrying the same signal rather than shorting UART1 TX against UART2 TX. The jumper is harmless in this test but electrically redundant because the GPIO matrix already provides the connection.

For external wiring, the sketch changes the listening GPIO to input-only while preserving its UART RX routing, then enables input/output mode only for the current transmitter. This prevents the listening push-pull TX from holding the line HIGH against the sender. Add an external **pull-up** to hold the line at UART idle HIGH during direction changes.

On USB Serial, the sketch prints **`USE_INTERNAL_LOOPBACK`**, the **GPIO numbers** for `ONEWIRE_PIN1` / `ONEWIRE_PIN2`, and wiring instructions (no jumper vs external bus) before each part runs.

## Pins

| Define | Default | Role |
|--------|---------|------|
| `ONEWIRE_PIN1` | `SDA` (board `pins_arduino.h`) | UART1 one-wire GPIO |
| `ONEWIRE_PIN2` | `RX2` | UART2 one-wire GPIO (Part B) |

Change the `#ifndef` defaults for your board if those pads are unavailable. Part B requires two different GPIOs; the sketch rejects equal `ONEWIRE_PIN1` and `ONEWIRE_PIN2` values.

## Electrical warning

ESP-IDF documents that routing TX output and RX input to one GPIO can cause **pad conflicts**. Recommended practices:

- Half-duplex protocol with the listening TX output tri-stated
- External pull-up so the line remains idle HIGH while neither UART drives
- Never enable both push-pull TX outputs on the connected wire simultaneously
- **Not** a replacement for RS485 (which needs separate TX, RX, and RTS to a transceiver)

Internal pull on RX is **disabled** in one-wire mode.

## Why one-wire UART can receive ghost data

In one-wire mode, the pad is deliberately left **floating** (no internal pull), and RX is always listening on the same pin that TX drives. Any brief LOW or noise on that pad looks like a UART start bit, so the receiver produces framing errors, BREAK events, or “ghost” bytes even with no intentional traffic.

Main causes:

1. **No idle bias** — split-pin UART uses RX pull-up so idle stays HIGH. One-wire disables that pull so TX isn’t fighting an internal resistor. When TX is not firmly driving (startup, pin mux change, tri-state between turns, weak/open bus), the line floats and RX samples garbage.
2. **TX/RX pad conflict** — both output and input are on one GPIO. During attach/mux changes the line can glitch LOW, causing a start bit or BREAK.
3. **Self-echo** — TX is also seen on RX. That is expected for intentional transmission; noise on TX looks the same.
4. **External bus without pull-up** — two one-wire peers with listening TX tri-stated leave the wire floating between turns.

## How to prevent it

| Approach | Effect |
|---|---|
| **External pull-up** (approximately 4.7–10 kΩ to 3.3 V) on the one-wire pad or shared bus | Holds idle HIGH when nothing drives—the strongest fix |
| **Drain RX after `begin()` or `setPins()`** using `while (available()) read()` | Clears glitches generated during pin attachment |
| **Ignore data until the first valid frame** or require a known preamble | Filters noise before real protocol traffic |
| **Use half-duplex only**—never leave both TX drivers enabled; tri-state the listener | Avoids push-pull conflicts that corrupt the idle level |
| **Use shorter wires and reduce nearby electrical noise** | Reduces false start bits on a floating pad |
| **Do not re-enable the internal pull in one-wire mode** | Prevents the pull resistor from fighting TX |

The internal pull is intentionally disabled in one-wire mode. Rely on **TX idle HIGH** while transmitting and use an **external pull-up** whenever the line can float.

## Usage

1. Upload `OneWire_UART_Demo.ino`.
2. Open Serial Monitor at **115200** baud.
3. Read the startup lines: one-wire **GPIO numbers**, `USE_INTERNAL_LOOPBACK` mode, and Part A/B wiring summary.
4. If `USE_INTERNAL_LOOPBACK=0`, connect the jumper(s) shown on Serial Monitor, then reset or re-upload.
5. Read Part A/B self-test results on startup.
6. **Part B startup test:** confirm UART2 receives `UART1_TO_UART2`, then UART1 receives `UART2_TO_UART1` over the same connection.
7. **Part B interactive:** type characters — they are sent on UART1; bytes received on UART2 are printed to USB Serial.
8. **Part A only (2-UART boards without Part B):** typed characters echo on UART1’s one-wire GPIO.

## Expected serial output

1. **Startup** — `ONEWIRE_PIN1` / `ONEWIRE_PIN2` GPIO numbers, loopback mode banner, Part A wiring line, Part B preview (when compiled).
2. **Part A** — `"ONEWIRE"` received on UART1 self loopback, or failure text with GPIO and loopback hint.
3. **Part B** — wiring block (internal or one external wire), `"UART1_TO_UART2"` received on UART2, then `"UART2_TO_UART1"` received on UART1.

## Wiring (external loopback, `USE_INTERNAL_LOOPBACK = 0`)

The sketch prints the exact GPIO numbers for your board. Conceptually:

```
UART1 GPIO (ONEWIRE_PIN1) ---- one wire ---- UART2 GPIO (ONEWIRE_PIN2)
                                |
                           pull-up (recommended)
```

For Part A self-test only, you can optionally short `ONEWIRE_PIN1` to itself; Part B needs the inter-UART jumper above.

## Real-world note

For same-pin half-duplex on a **single** UART pad, Part A is the relevant model. For a **bus** between two UART instances, see Part B and keep half-duplex discipline.

For split-pin RX pull behavior, see `RxPull_Demo`.
