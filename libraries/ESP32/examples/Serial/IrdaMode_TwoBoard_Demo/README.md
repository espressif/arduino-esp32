# IrDA Mode Two-Board Demo

Two-board demonstration of UART IrDA communication with user-selectable TX/RX modes.

## Use Case

- **Two ESP32 boards** each with minimal UARTs (UART0 + UART1)
- **User-controlled mode selection** via Serial Monitor
- **Real infrared communication** between boards
- **Learning** how to configure and use IrDA for peer-to-peer communication

## Supported SoCs

This example runs on **any ESP32 variant** with at least 2 UARTs:

| SoC | Support | Notes |
|-----|---------|-------|
| ESP32 | ✓ Yes | Can use UART0 + UART1 |
| ESP32-S2 | ✓ Yes | Only has UART0, UART1 |
| ESP32-S3 | ✓ Yes | Can use UART0 + UART1 |
| ESP32-C2 | ✓ Yes | Only has UART0, UART1 |
| ESP32-C3 | ✓ Yes | Only has UART0, UART1 |
| ESP32-C6 | ✓ Yes | Only has UART0, UART1 |
| ESP32-H2 | ✓ Yes | Only has UART0, UART1 |
| ESP32-P4 | ✓ Yes | Can use UART0 + UART1 |
| ESP32-C61 | ✓ Yes | Only has UART0, UART1 |

**All ESP32 variants supported** - Each has UART0 (Serial Monitor) and UART1 (IrDA).

## Overview

This example demonstrates **real infrared communication** between **two separate ESP32 boards**:

- **Board 1 (TX mode)**: Transmits IrDA frames continuously
- **Board 2 (RX mode)**: Receives and displays IrDA frames
- **User selects mode** at startup via Serial Monitor (press 'T' or 'R')
- **Real hardware required**: IR LED on TX board, IR receiver on RX board

The communication pattern:
```
Board 1 (TX)  --[IR LED]--→ [IR Space] --[IR Receiver]--  Board 2 (RX)
   "FRAME_0"                                            displays: "FRAME_0"
   "FRAME_1"                                            displays: "FRAME_1"
```

## Hardware Setup

### Requirements

- **Two ESP32 boards** (any variant with UART0 and UART1)
- **Infrared LED** (950 nm wavelength) with current-limiting resistor (~100-330Ω)
- **Infrared receiver** (raw IR photodiode/phototransistor, do not use a demodulating remote-control receiver)
- **USB connections** for each board (for Serial Monitor + programming)
- **Optional**: External power supply for more reliable IR transmission

### Pin Configuration

| UART | RX Pin | TX Pin | Mode | Purpose |
|------|--------|--------|------|---------|
| UART0 | RX0 (GPIO3) | TX0 (GPIO1) | Serial Monitor | Debug output & mode selection |
| UART1 | RX1 (varies) | TX1 (varies) | IrDA TX or RX | IR communication |

**Note:** RX1 and TX1 pin numbers vary by board (e.g., GPIO26/27 on ESP32, GPIO18/19 on ESP32-C3).

### Wiring Diagram

**TX Board:**
```
ESP32 Board 1 (TX mode)
│
├─ UART0 (Serial): USB → Serial Monitor
│
└─ UART1 (IrDA TX)
   └─ TX1 pin ──[IR LED with 100-330Ω resistor]──→ IR Light
```

**RX Board:**
```
ESP32 Board 2 (RX mode)
│
├─ UART0 (Serial): USB → Serial Monitor
│
└─ UART1 (IrDA RX)
   ├─ RX1 pin ←─[IR Receiver + amplifier]←─ IR Light
```

**IR Optical Alignment:**
```
Board 1 (TX)                    Board 2 (RX)
[IR LED] ────→ Line-of-sight ←─ [IR Receiver]
 Pointing                        Pointing
 →→→                             ←←←
```

## How to Use

### Step 1: Program Both Boards

**For TX Board (Transmitter):**
1. Open `IrdaMode_TwoBoard_Demo.ino` in Arduino IDE
2. Upload to first ESP32 board
3. Open Serial Monitor at 115200 baud
4. When prompted, **press 'T'** to select TX mode

**For RX Board (Receiver):**
1. Open `IrdaMode_TwoBoard_Demo.ino` in Arduino IDE (same code!)
2. Upload to second ESP32 board
3. Open Serial Monitor at 115200 baud
4. When prompted, **press 'R'** to select RX mode

### Step 2: Setup Hardware

1. **TX Board**: Connect IR LED with resistor to UART1 TX pin
2. **RX Board**: Connect IR receiver with amplifier to UART1 RX pin
3. **Align** IR optical components for line-of-sight (facing each other)
4. **Separation**: Start with boards 10-20 cm apart

### Step 3: Monitor Communication

**TX Board Serial Monitor:**
```
=== UART IrDA Two-Board Demo ===
This example requires 2 ESP32 boards connected via infrared

Select IrDA mode:
  T - Transmit mode (this board sends IrDA data)
  R - Receive mode (this board receives IrDA data)

Wait for mode selection from Serial Monitor...

✓ Mode selected: TRANSMIT (TX)
UART1 configured in IRDA TX mode
Transmitting IrDA frames...

TX -> Sending frame 0
   (Waiting for response from peer RX mode)
TX -> Sending frame 1
   (Waiting for response from peer RX mode)
```

**RX Board Serial Monitor:**
```
=== UART IrDA Two-Board Demo ===
This example requires 2 ESP32 boards connected via infrared

Select IrDA mode:
  T - Transmit mode (this board sends IrDA data)
  R - Receive mode (this board receives IrDA data)

Wait for mode selection from Serial Monitor...

✓ Mode selected: RECEIVE (RX)
UART1 configured in IRDA RX mode
Waiting for IrDA frames...

RX <- Received: FRAME_0
RX <- Received: FRAME_1
RX <- Received: FRAME_2
```

## Understanding the Code

### Mode Selection via Serial

```cpp
void selectMode() {
  Serial.println("Select IrDA mode: T or R");

  while (irda_mode == MODE_NOT_SET) {
    if (Serial.available()) {
      char cmd = Serial.read();
      if (cmd == 'T' || cmd == 't') {
        irda_mode = MODE_TX;
        setupTXMode();
      } else if (cmd == 'R' || cmd == 'r') {
        irda_mode = MODE_RX;
        setupRXMode();
      }
    }
    delay(10);
  }
}
```

Users select mode by typing in Serial Monitor, no code changes needed!

### TX Mode Operation

```cpp
void loopTXMode() {
  // Transmit IrDA frame
  irda_uart.printf("FRAME_%lu\n", counter);
  irda_uart.flush();

  // Wait for optional response
  // (could receive acknowledgment from peer)

  counter++;
  delay(1000);  // Send new frame every 1 second
}
```

### RX Mode Operation

```cpp
void loopRXMode() {
  // Receive IrDA data
  while (irda_uart.available()) {
    char c = (char)irda_uart.read();
    if (c == '\n') {
      // Complete frame received
      Serial.printf("RX <- Received: %s\n", received.c_str());
    }
  }
}
```

## Testing Procedure

### Basic Test

1. Upload to both boards (same sketch)
2. Program Board 1 as TX, Board 2 as RX
3. Verify both show confirmation in Serial Monitor
4. RX board should start receiving "FRAME_X" messages

### Troubleshooting Tests

**No data received on RX board:**
- Check IR LED is lighting up on TX board (should glow faintly)
- Align IR receiver to face IR LED (line-of-sight)
- Reduce distance between boards (start with 5-10 cm)
- Verify RX board is in RX mode (check Serial Monitor)
- Check baud rates match (both set to 9600)

**Garbled data received:**
- Verify both boards use same baud rate
- Check IR receiver amplifier circuit (op-amp gain may need adjustment)
- Look for IR interference (avoid sunlight, other IR sources)

**TX board not sending:**
- Verify TX board is in TX mode (Serial Monitor should confirm)
- Check IR LED is connected properly (check with camera - IR is invisible)
- Verify TX pin is connected to IR LED (not swapped with RX)

## Advanced Usage

### Bidirectional Communication

Modify code to allow TX board to receive responses:

```cpp
// In loopTXMode(), after transmitting
irda_uart.setIrdaDirection(ESP32_UART_IRDA_RX);  // Switch to RX
delay(100);

// Wait for acknowledgment
// ...

irda_uart.setIrdaDirection(ESP32_UART_IRDA_TX);  // Switch back to TX
```

**Note:** Only one direction at a time. This requires switching modes.

### Custom Frame Format

Modify frame format in `loopTXMode()`:

```cpp
// Current: "FRAME_0\n"
irda_uart.printf("FRAME_%lu\n", counter);

// Custom example: Add timestamp
irda_uart.printf("TS=%lu,DATA=test_%lu\n", millis(), counter);
```

## Comparing with Dual-UART Demo

| Feature | Dual-UART Demo | Two-Board Demo |
|---------|---|---|
| **Boards Required** | 1 | 2 |
| **Hardware** | None (internal loopback) | IR LED + Receiver |
| **Supported SoCs** | 3+ UARTs only | All SoCs |
| **User Interaction** | None | Mode selection via Serial |
| **Distance** | Same board | Up to 1 meter (with amplifier) |
| **Use Case** | Testing/Development | Real-world communication |

## Real-World Considerations

### IR Optical Components

**TX Side (IR LED):**
- Wavelength: 950 nm (standard for IrDA)
- Forward voltage: ~1.5 V typical
- Do **not** assume an ESP32 GPIO can directly drive a high-current IR LED. Check the current limits in the datasheet for your specific ESP32 SoC/module/board.
- For simple, short-range experiments, use a resistor sized for **low GPIO current only** if driving the LED directly.
- For stronger output or longer range, drive the IR LED through a **transistor or logic-level MOSFET** (and use an external LED supply if needed, with a shared ground).
- Size the series resistor for your actual LED forward voltage and target current in the chosen drive circuit; avoid treating ~100 mA as a typical direct-drive GPIO target.
- Practical: 100-220Ω can be a conservative starting point for low-current testing, but verify the actual current for your LED and supply voltage.

**RX Side (IR Receiver):**
- Photodiode: Detects 940-960 nm IR light
- Requires amplification (op-amp TL072, LM358, etc.)
- Gain: ~1000x typical
- Output: TTL-compatible signal to UART RX pin

### Environmental Factors

- **Sunlight**: High IR content, may cause interference
- **Distance**: 1-5 meters typical for low-power setup
- **Angle**: Best performance with direct alignment (±30 degrees)
- **Obstacles**: IR light doesn't penetrate walls/obstacles

## Additional Resources

- [HardwareSerial API Reference](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/uart.html)
- [IrDA Specification](https://en.wikipedia.org/wiki/Infrared_Data_Association)
- [ESP32 GPIO Matrix](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html)
- Related Example: `IrdaMode_DualUART_Demo.ino` - Single-board with internal loopback
