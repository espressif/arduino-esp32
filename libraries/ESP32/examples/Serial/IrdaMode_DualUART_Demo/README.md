# IrDA Mode Dual-UART Demo

Single-board demonstration of UART IrDA communication using internal cross-UART loopback.

## Use Case

- **Single ESP32 board with 3+ UARTs** (ESP32, ESP32-S3, ESP32-P4, etc.)
- **Internal loopback testing** - no external hardware needed
- **Development and testing** of IrDA functionality
- **Learning** how IrDA TX and RX modes work

## Supported SoCs

| SoC | Support | Reason |
|-----|---------|--------|
| ESP32 | ✓ Yes | Has UART0, UART1, UART2 |
| ESP32-S2 | ✗ No | Only has UART0, UART1 |
| ESP32-S3 | ✓ Yes | Has UART0, UART1, UART2 |
| ESP32-C2 | ✗ No | Only has UART0, UART1 |
| ESP32-C3 | ✗ No | Only has UART0, UART1 |
| ESP32-C6 | ✗ No | Only has UART0, UART1 |
| ESP32-H2 | ✗ No | Only has UART0, UART1 |
| ESP32-P4 | ✓ Yes | Has 5 UARTs |
| ESP32-C61 | ✗ No | Only has UART0, UART1 |

## Overview

This example demonstrates IrDA communication on a **single ESP32 board** by using two separate UARTs:

- **UART1**: Configured in IrDA TX mode (transmitter)
- **UART2**: Configured in IrDA RX mode (receiver)
- **Internal loopback**: UART1 TX pin internally connected to UART2 RX pin via GPIO matrix

The demo continuously:
1. UART1 transmits "PING {counter}" in IrDA TX mode
2. UART2 receives the same message in IrDA RX mode
3. Displays both transmission and reception on Serial Monitor

## Hardware Setup

### Requirements

- **Single ESP32 board with 3+ UARTs** (e.g., ESP32 or ESP32-S3)
- **USB connection** for Serial Monitor
- **No additional hardware needed** - uses internal GPIO matrix loopback

### Pin Configuration

| UART | RX Pin | TX Pin | Mode |
|------|--------|--------|------|
| UART1 | RX1 (GPIO26 on ESP32) | TX1 (GPIO27 on ESP32) | IrDA TX |
| UART2 | RX2 (GPIO16 on ESP32) | TX2 (GPIO17 on ESP32) | IrDA RX |

**Note:** Actual GPIO numbers vary by board. Example uses `RX1`, `TX1`, `RX2`, `TX2` constants for compatibility.

## How to Use

### Step 1: Verify Board Compatibility

Check if your board has 3+ UARTs:
- Upload the sketch to your ESP32
- Open Serial Monitor at 115200 baud
- If it says "✓ Board has 3+ UARTs: Using internal cross-UART loopback" → Ready to go!
- If it says "✗ ERROR: This example requires 3+ UARTs" → Use `IrdaMode_TwoBoard_Demo.ino` instead

### Step 2: Run the Example

1. Open `IrdaMode_DualUART_Demo.ino` in Arduino IDE
2. Select your board (ESP32, ESP32-S3, ESP32-P4, etc.)
3. Upload the sketch
4. Open Serial Monitor at 115200 baud
5. Watch UART1 transmit and UART2 receive IrDA frames

### Expected Output

```
=== UART IrDA Dual-UART Demo (Single Board) ===
✓ Board has 3+ UARTs: Using internal cross-UART loopback
✓ UART1 initialized in IRDA TX mode
✓ UART2 initialized in IRDA RX mode
✓ Internal loopback configured (UART1 TX → UART2 RX)

Demo will alternate between:
1) UART1 transmits message in IRDA TX mode
2) UART2 receives message in IRDA RX mode

TX (UART1) -> PING 0
RX (UART2) <- PING 0

TX (UART1) -> PING 1
RX (UART2) <- PING 1

TX (UART1) -> PING 2
RX (UART2) <- PING 2
```

## Why No External Hardware?

The ESP32 UART hardware handles all IrDA encoding/decoding automatically. Additionally:

- **Internal GPIO Matrix**: Connects UART1 TX → UART2 RX without leaving the chip
- **No IR optics needed**: Data flows through GPIO matrix, not infrared
- **Perfect for testing**: Validates IrDA mode switching and communication protocol
- **Fast development**: No external component delays or alignment issues

## Understanding the Code

### UART Detection

```cpp
#if SOC_UART_HP_NUM >= 3
  #ifdef RX2
    #define HAS_UART2 1
```

Automatically detects if board has 3+ UARTs and RX2 pin defined.

### Dual-UART Setup

```cpp
// UART1: IrDA TX mode
uart_tx.setMode(UART_MODE_IRDA);
uart_tx.setIrdaDirection(ESP32_UART_IRDA_TX);

// UART2: IrDA RX mode
uart_rx.setMode(UART_MODE_IRDA);
uart_rx.setIrdaDirection(ESP32_UART_IRDA_RX);

// Connect them internally
uart_internal_loopback(1, UART2_RX_PIN);
```

### Main Loop

```cpp
void loopDualUART(uint32_t counter) {
  // TX: transmit on UART1
  uart_tx.printf("PING %lu\n", counter);
  uart_tx.flush();
  delay(100);

  // RX: receive on UART2 with timeout
  uint32_t start = millis();
  while ((millis() - start) < RX_TIMEOUT) {
    while (uart_rx.available()) {
      // Read and display received data
    }
  }
}
```

## Testing Multi-Board Communication

If you want to test **two separate ESP32 boards** communicating via infrared:
- Use `IrdaMode_TwoBoard_Demo.ino` instead
- One board in TX mode, other in RX mode
- Connect actual IR LED (TX side) and IR receiver (RX side)

## Troubleshooting

### "ERROR: This example requires 3+ UARTs"

**Cause:** Your ESP32 board only has UART0 and UART1

**Solution:**
- Use a different board (ESP32, ESP32-S3, or ESP32-P4)
- Or use `IrdaMode_TwoBoard_Demo.ino` with 2 boards

### No data received on UART2

**Cause:**
- Internal loopback not configured
- UART2 not in RX mode
- Baud rate mismatch

**Solution:**
- Verify `uart_internal_loopback(1, UART2_RX_PIN)` is called
- Check Serial Monitor output for setup errors
- Ensure both UARTs use same baud rate (9600)

### Compilation errors with RX2/TX2

**Cause:** Board doesn't have UART2 pins defined

**Solution:**
- Verify your board has 3+ UARTs
- Check board definition file for RX2, TX2 constants
- Use a board with confirmed 3+ UART support

## Real-World IrDA Communication

This example demonstrates IrDA **mode and protocol** handling. For actual infrared transmission:

1. Use `IrdaMode_TwoBoard_Demo.ino`
2. Add external hardware:
   - **TX side**: IR LED (950 nm) + 100-330Ω resistor on TX pin
   - **RX side**: IR photodiode + amplifier circuit on RX pin
3. Align optical components for line-of-sight
4. The UART hardware automatically handles pulse timing and encoding

## Additional Resources

- [HardwareSerial API Reference](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/uart.html)
- [IrDA Specification](https://en.wikipedia.org/wiki/Infrared_Data_Association)
- [ESP32 UART Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/uart.html)
- Related Example: `IrdaMode_TwoBoard_Demo.ino` - Two-board IrDA communication
