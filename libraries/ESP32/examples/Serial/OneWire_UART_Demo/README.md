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
| **A — Self-echo** | UART1 only | Same GPIO for RX+TX; no jumper | Proves automatic one-wire RX/TX on one pad |
| **B — Cross connection** | UART1 ↔ UART2 | One signal wire between two bus GPIOs | Bidirectional half-duplex request/reply between two UART instances |

Part B runs only when the board has **3+ HP UARTs** and defines `RX2`, `TX1`, and `TX2` (UART0 remains available for Serial Monitor output). UART1 first sends a request to UART2; UART2 then sends a reply to UART1 over the same wire.

Part A is the direct one-wire API demonstration (`RX == TX`). External Part B uses split RX/TX assignments to ensure that only one push-pull TX output is connected to the shared wire. It demonstrates a practical single-wire half-duplex link, but it does not put both UART endpoints in one-wire mode simultaneously.

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
| `0` | No jumper; same-pad self-echo | One signal wire: **ONEWIRE_PIN1 ↔ ONEWIRE_PIN2**; **4.7–10 kΩ pull-up to 3.3 V recommended** |

With `USE_INTERNAL_LOOPBACK=1`, the sketch synchronizes GPIO-matrix output routing before each turn:

- **Turn 1:** both `ONEWIRE_PIN1` and `ONEWIRE_PIN2` carry UART1 TX.
- **Turn 2:** both pins carry UART2 TX.
- Direction changes happen after `flush()`, while UART is idle HIGH.

Consequently, an installed jumper connects two pads carrying the same signal rather than shorting UART1 TX against UART2 TX. The jumper is harmless in this test but electrically redundant because the GPIO matrix already provides the connection.

With `USE_INTERNAL_LOOPBACK=0`, connecting two push-pull TX outputs to one wire would cause contention. The sketch therefore keeps **only one UART TX on the bus** each turn:

- **Driver:** TX on its bus pad; RX is parked on its off-bus parking GPIO.
- **Listener:** RX on its bus pad; TX is parked on its off-bus parking GPIO.

The default parking GPIOs are `TX1` and `TX2`; leave them physically unconnected. You may override them with `ONEWIRE_PARK_PIN1` and `ONEWIRE_PARK_PIN2`. The sketch rejects any duplicate bus/parking GPIO assignment.

Roles swap between Turn 1 and Turn 2. For interactive mode after an external Part B, the sketch **keeps Turn 2 roles** (UART2 drives, UART1 listens). This avoids another pin-role change and uses the direction that the startup test just verified.

The external pull-up is recommended because both UARTs are briefly detached while roles change; the test may work without it because the active TX drives idle HIGH. Never transmit from both UARTs at once.

On USB Serial, the sketch prints **`USE_INTERNAL_LOOPBACK`**, the **GPIO numbers** for `ONEWIRE_PIN1` / `ONEWIRE_PIN2`, and wiring instructions (no jumper vs external bus) before each part runs.

## Pins

| Define | Default | Role |
|--------|---------|------|
| `ONEWIRE_PIN1` | `SDA` (board `pins_arduino.h`) | UART1 one-wire pad in Part A; UART1 bus pad in Part B |
| `ONEWIRE_PIN2` | `RX2` | UART2 bus pad in Part B |
| `ONEWIRE_PARK_PIN1` | `TX1` | UART1 off-bus RX/TX parking GPIO in external Part B |
| `ONEWIRE_PARK_PIN2` | `TX2` | UART2 off-bus RX/TX parking GPIO in external Part B |

Override these defines before their `#ifndef` defaults if the selected pads are unavailable. External Part B requires all four bus/parking GPIOs to be distinct.

## Electrical warning

ESP-IDF documents that routing TX output and RX input to one GPIO can cause **pad conflicts**. Recommended practices:

- On a two-UART external bus: only one TX on the wire; park unused RX/TX off-bus (this demo’s external Part B)
- External pull-up (4.7–10 kΩ to 3.3 V) so idle stays HIGH
- Half-duplex protocol — never transmit from both UARTs at once
- **Not** a replacement for RS485 (which needs separate TX, RX, and RTS to a transceiver)

Internal pull on RX is **disabled** in one-wire mode.

## Why one-wire UART can receive ghost data

In one-wire mode, the pad is deliberately left **floating** (no internal pull), and RX is always listening on the same pin that TX drives. Any brief LOW or noise on that pad looks like a UART start bit, so the receiver produces framing errors, BREAK events, or “ghost” bytes even with no intentional traffic.

Main causes:

1. **No idle bias** — split-pin UART uses RX pull-up so idle stays HIGH. One-wire disables that pull so TX isn’t fighting an internal resistor. When TX is not firmly driving (startup, pin mux change, tri-state between turns, weak/open bus), the line floats and RX samples garbage.
2. **TX/RX pad conflict** — both output and input are on one GPIO. During attach/mux changes the line can glitch LOW, causing a start bit or BREAK.
3. **Self-echo** — TX is also seen on RX. That is expected for intentional transmission; noise on TX looks the same.
4. **External bus without idle bias** — a shared wire can float while UART pins are detached or TX outputs are disconnected between turns.

## How to prevent it

| Approach | Effect |
|---|---|
| **External pull-up** (approximately 4.7–10 kΩ to 3.3 V) on the one-wire pad or shared bus | Holds idle HIGH when nothing drives—the strongest fix |
| **Drain RX after `begin()` or `setPins()`** using `while (available()) read()` | Clears glitches generated during pin attachment |
| **Ignore data until the first valid frame** or require a known preamble | Filters noise before real protocol traffic |
| **Use half-duplex only**—disconnect, reroute, or disable the listener's TX | Prevents two push-pull TX outputs from driving the shared wire |
| **Use shorter wires and reduce nearby electrical noise** | Reduces false start bits on a floating pad |
| **Do not re-enable the internal pull in one-wire mode** | Prevents the pull resistor from fighting TX |

The internal pull is intentionally disabled in one-wire mode. Rely on **TX idle HIGH** while transmitting and use an **external pull-up** whenever the line can float.

## Usage

1. Upload `OneWire_UART_Demo.ino`.
2. Open Serial Monitor at **115200** baud.
3. Read the startup lines: one-wire **GPIO numbers**, `USE_INTERNAL_LOOPBACK` mode, and Part A/B wiring summary.
4. To reproduce the external test, set `USE_INTERNAL_LOOPBACK` to `0`, connect `ONEWIRE_PIN1` directly to `ONEWIRE_PIN2`, and leave both parking GPIOs unconnected.
5. Connect a 4.7–10 kΩ resistor from the signal wire to 3.3 V (recommended), then reset or re-upload.
6. Read Part A/B self-test results on startup.
7. **Part B startup test:** confirm UART2 receives `UART1_TO_UART2`, then UART1 receives `UART2_TO_UART1` over the same connection.
8. **Part B interactive:** type text and press Send — external mode keeps Turn 2 roles, so bytes travel UART2 → wire → UART1 and print in Serial Monitor.
9. **Part A only (boards without Part B):** typed characters self-echo through UART1’s one-wire GPIO.

## Expected serial output

1. **Startup** — `ONEWIRE_PIN1` / `ONEWIRE_PIN2` GPIO numbers, loopback mode banner, Part A wiring line, Part B preview (when compiled).
2. **Part A** — `"ONEWIRE"` received through UART1 same-pad self-echo, or failure text with a GPIO hint.
3. **Part B** — wiring block (internal or one external signal wire), `"UART1_TO_UART2"` received on UART2, then `"UART2_TO_UART1"` received on UART1.
4. **Interactive** — a direction line followed by the text entered in Serial Monitor.

## Wiring (external cross-test, `USE_INTERNAL_LOOPBACK = 0`)

The sketch prints the exact GPIO numbers for your board. Conceptually:

```
UART1 GPIO (ONEWIRE_PIN1) ---- bus ---- UART2 GPIO (ONEWIRE_PIN2)
                                |
                     4.7–10 kΩ pull-up to 3.3 V

UART1 parking GPIO (ONEWIRE_PARK_PIN1) ---- not connected
UART2 parking GPIO (ONEWIRE_PARK_PIN2) ---- not connected
```

Part A needs no jumper because UART1 RX and TX are already routed to the same physical pad. Part B needs the inter-UART signal wire above; the pull-up is recommended but not required while an active TX holds the line HIGH.

### Tested ESP32-S3 wiring

With the generic ESP32-S3 variant defaults, Serial Monitor should print these assignments:

- `ONEWIRE_PIN1`: GPIO 8
- `ONEWIRE_PIN2`: GPIO 19
- `ONEWIRE_PARK_PIN1`: GPIO 16 — leave unconnected
- `ONEWIRE_PARK_PIN2`: GPIO 20 — leave unconnected

Connect GPIO 8 directly to GPIO 19. A tested idle-bias value is **5.6 kΩ from that signal wire to 3.3 V**; the startup and interactive tests can also pass without it while the active TX holds the line HIGH. Set `USE_INTERNAL_LOOPBACK` to `0`, upload, open Serial Monitor at 115200 baud, type text, and press Send.

## Real-world note

For same-pin half-duplex on a **single** UART pad, Part A is the relevant model. For a **bus** between two UART instances, see Part B and keep half-duplex discipline.

For split-pin RX pull behavior, see `RxPull_Demo`.
