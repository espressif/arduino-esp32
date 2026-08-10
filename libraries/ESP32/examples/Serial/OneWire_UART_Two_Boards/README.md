# One-Wire UART Between Two Boards

Simple half-duplex PING/PONG over one signal wire between two ESP32-family boards.

- Each board uses `Serial1` with `RX == TX` on one `BUS_PIN` (open-drain is automatic).
- Connect `BUS_PIN` to `BUS_PIN`, `GND` to `GND`, and a pull-up resistor from the bus to 3.3 V.
- Flash the same sketch on both boards with different `NODE_ROLE` values.
- After every write, discard the local self-echo before reading the peer.
- Optional: run both roles on one board with UART1 and UART2 (see [Single board with UART1 and UART2](#single-board-with-uart1-and-uart2)).

## Wiring

```text
                            3.3 V
                              |
                         pull-up resistor
                              |
ESP32-S3 BUS_PIN  ------------+-------  ESP32 BUS_PIN
ESP32-S3 GND      --------------------  ESP32 GND
```

Do not tie the boards' 3.3 V supplies together. Power both boards and connect the pull-up to one board's 3.3 V.

## Configure

```cpp
// Board A
#define NODE_ROLE ROLE_INITIATOR
#define BUS_PIN   15   // example ESP32-S3 UART1 RX

// Board B
#define NODE_ROLE ROLE_RESPONDER
#define BUS_PIN   26   // example classic ESP32 UART1 RX
```

If `BUS_PIN` is omitted, the sketch uses `RX1`. Override `LINK_BAUD` if needed (default 115200).

## Single board with UART1 and UART2

You can run both roles on **one** board instead of two devices. Wire two open-drain one-wire pads together with the same **external pull-up to 3.3 V** (required — the bus has no valid idle HIGH without it).

```text
                         3.3 V
                           |
                    pull-up resistor
                           |
UART1 BUS_PIN1  -----------+-----------  UART2 BUS_PIN2
```

### Capable boards

Needs **Serial1** and **Serial2** as high-performance (HP) UARTs while USB `Serial` stays free for the monitor — typically **3+ HP UARTs**:

| SoC | Single-board UART1↔UART2 |
|-----|--------------------------|
| ESP32 | Yes |
| ESP32-S3 | Yes |
| ESP32-P4 | Yes |
| ESP32-S2, C2, C3, C5, C6, C61, H2 | Usually no (fewer than three HP UARTs) |

Confirm the board defines usable pins for `Serial1` / `Serial2` (for example `RX1` / `RX2`).

### How to adapt the sketch

This sketch is written for **two flashes**: `NODE_ROLE` selects initiator *or* responder. On one board you must run **both**, so that compile-time role switch has to go.

**Steps:**

1. **Remove the role switch**
   - Delete `ROLE_INITIATOR`, `ROLE_RESPONDER`, and `#define NODE_ROLE ...`
   - Delete every `#if NODE_ROLE == ...` / `#else` / `#endif` in `setup()` and `loop()` (those call only one of `runInitiator()` / `runResponder()`)

2. **Use two one-wire UARTs**
   - `Serial1` = initiator, `Serial2` = responder (USB `Serial` stays the monitor)
   - Two pins, each with `RX == TX` (open-drain is automatic)
   - Wire `BUS_PIN1` to `BUS_PIN2` and add an **external pull-up to 3.3 V** (required)

3. **Call both roles from one `loop()`**
   - Make `sendLine` / `readLine` take `HardwareSerial &` (or keep two thin wrappers)
   - In `loop()`, run the initiator turn on `Serial1`, then service the responder on `Serial2` (half-duplex: only one transmits at a time)
   - Keep discarding local TX self-echo after every write

**Outline:**

```cpp
// Do NOT define NODE_ROLE — both roles run in this firmware.

#ifndef BUS_PIN1
#define BUS_PIN1 RX1
#endif
#ifndef BUS_PIN2
#define BUS_PIN2 RX2
#endif

static HardwareSerial &Initiator = Serial1;
static HardwareSerial &Responder = Serial2;

void setup() {
  Serial.begin(115200);
  Initiator.begin(LINK_BAUD, SERIAL_8N1, BUS_PIN1, BUS_PIN1);  // RX == TX
  Responder.begin(LINK_BAUD, SERIAL_8N1, BUS_PIN2, BUS_PIN2);
  // drain both UARTs
}

void loop() {
  // Same PING/PONG logic as runInitiator() / runResponder(), but:
  // send/read on Initiator for the PING side, on Responder for the PONG side.
}
```

See the capable-boards table above. Without removing `NODE_ROLE`, the sketch still builds only one role and cannot talk UART1 ↔ UART2 by itself.

## What to expect

Initiator:

```text
TX: PING 0
RX: PONG 0 (OK)
```

Responder:

```text
Waiting for PING...
RX: PING 0
TX: PONG 0
```

## Notes

- Half-duplex only: never transmit from both boards at once.
- Same-pin RX always hears local TX; `sendLine()` discards that echo.
- This is a two-node demo, not a multi-drop arbitration protocol.
