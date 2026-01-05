# Hardware Flow Control Demo

This example demonstrates UART hardware flow control using RTS (Request To Send) and CTS (Clear To Send) signals with ESP32's HardwareSerial (UART1).

## Overview

Hardware flow control is a mechanism that prevents data loss by controlling when data can be transmitted and received. It uses two additional signals:

- **RTS (Request To Send)**: Output signal from the UART indicating it's ready to receive data
- **CTS (Clear To Send)**: Input signal to the UART that controls when transmission is allowed

## Configuration Options

The sketch supports two configuration options:

### USE_INTERNAL_MATRIX_PIN_LOOPBACK

**Location in code:** Line 55 in `HardwareFlowControl_Demo.ino`
```cpp
#define USE_INTERNAL_MATRIX_PIN_LOOPBACK 1  // Set to 1 for internal loopback, 0 for external wires
```

- **`USE_INTERNAL_MATRIX_PIN_LOOPBACK = 1`** (Default): Uses ESP32's internal GPIO matrix to create loopback connections. **No external wires needed!**
- **`USE_INTERNAL_MATRIX_PIN_LOOPBACK = 0`**: Requires external wire connections (see Pin Connections section below)

### USE_GPIO_CONTROL

**Location in code:** Line 72 in `HardwareFlowControl_Demo.ino`
```cpp
#define USE_GPIO_CONTROL false  // Set to true or false
```

**Note:** When `USE_INTERNAL_MATRIX_PIN_LOOPBACK = 1`, the `USE_GPIO_CONTROL` setting is overridden and hardware-controlled mode is used.

## Pin Connections

**Important:** Pin connections are only needed when `USE_INTERNAL_MATRIX_PIN_LOOPBACK = 0`.

### Default Pin Assignments

- **RX1**: Uses board default (`RX1` constant - typically GPIO26 for ESP32, varies by board)
- **TX1**: Uses board default (`TX1` constant - typically GPIO27 for ESP32, varies by board)
- **RTS1**: GPIO2 (configurable via `UART1_RTS_PIN`)
- **CTS1**: GPIO4 (configurable via `UART1_CTS_PIN`)
- **GPIO_RTS_MONITOR**: GPIO5 (for GPIO-controlled mode, configurable via `GPIO_RTS_MONITOR`)
- **GPIO_CTS_CTRL**: GPIO13 (for GPIO-controlled mode, configurable via `GPIO_CTS_CTRL`)

**Note:** RX1 and TX1 pin numbers are board-specific. Check your board's pin definitions or use the serial output to see the actual GPIO numbers being used.

### Option 1: Simple Loopback (`USE_GPIO_CONTROL = false`)

For a basic loopback test with automatic flow control:

```
ESP32 Pin Connections:
- TX1 ────┐
          ├──> RX1  (Data loopback)
- GPIO2 (RTS1) ────┐
                   ├──> GPIO4 (CTS1)  (Flow control loopback)
```

**Physical Connections (when USE_INTERNAL_MATRIX_PIN_LOOPBACK = 0):**
1. Connect TX1 pin to RX1 pin with a jumper wire - read the console serial output to know which pins are the default ones
2. Connect GPIO2 (RTS1) to GPIO4 (CTS1) with a jumper wire

### Option 2: GPIO-Controlled Flow Control (`USE_GPIO_CONTROL = true`)

For manual control of flow control signals:

```
ESP32 Pin Connections:
- TX1 ────> RX1  (Data loopback)
- GPIO2 (RTS1) ────> GPIO5 (RTS Monitor)  (Monitor RTS state)
- GPIO4 (CTS1) <─── GPIO13 (CTS Control)  (Control CTS signal)
```

**Physical Connections (when USE_INTERNAL_MATRIX_PIN_LOOPBACK = 0):**
1. Connect TX1 pin to RX1 pin with a jumper wire
2. Connect GPIO2 (RTS1) to GPIO5 with a jumper wire
3. Connect GPIO4 (CTS1) to GPIO13 with a jumper wire

## How Hardware Flow Control Works

### RTS (Request To Send)
- **Output signal** from the UART
- **LOW (0V)**: UART is ready to receive data (RX buffer has space)
- **HIGH (3.3V)**: UART RX buffer is getting full, cannot receive more data

### CTS (Clear To Send)
- **Input signal** to the UART
- **LOW (0V)**: UART is allowed to transmit data
- **HIGH (3.3V)**: UART must pause transmission (transmission is blocked)

### Flow Control Behavior

1. **Receiving Data (RTS)**:
   - When UART1's RX buffer has space, RTS1 is driven LOW
   - When RX buffer fills up (threshold reached), RTS1 is driven HIGH
   - This signals the sender to stop transmitting

2. **Transmitting Data (CTS)**:
   - UART1 checks CTS1 before transmitting
   - If CTS1 is LOW, transmission proceeds normally
   - If CTS1 is HIGH, UART1 pauses transmission until CTS1 goes LOW again

### Understanding the Two Modes

**`USE_GPIO_CONTROL = false` (Default - Simple Loopback)**
- Use this mode when you connect RTS1 directly to CTS1 (hardware loopback)
- Flow control operates automatically - no software intervention needed
- RTS/CTS signals are controlled entirely by the UART hardware
- Best for: Basic testing and understanding automatic flow control behavior
- **Wiring (when USE_INTERNAL_MATRIX_PIN_LOOPBACK = 0):** Connect GPIO2 (RTS1) → GPIO4 (CTS1)

**`USE_GPIO_CONTROL = true` (GPIO-Controlled Mode)**
- Use this mode when you want to manually control or monitor flow control signals
- Software can read RTS state and control CTS signal via GPIO pins
- Demonstrates explicit flow control blocking behavior
- Best for: Testing flow control behavior, interfacing with external logic, or learning how external devices control UART transmission
- **Wiring (when USE_INTERNAL_MATRIX_PIN_LOOPBACK = 0):** 
  - Connect GPIO2 (RTS1) → GPIO5 (to monitor RTS)
  - Connect GPIO4 (CTS1) → GPIO13 (to control CTS)
- **Note:** This mode is overridden when `USE_INTERNAL_MATRIX_PIN_LOOPBACK = 1`

### How to Configure

1. Open `HardwareFlowControl_Demo.ino` in Arduino IDE
2. **For internal loopback (no wires):** Set `USE_INTERNAL_MATRIX_PIN_LOOPBACK` to `1` (default)
3. **For external wires:** Set `USE_INTERNAL_MATRIX_PIN_LOOPBACK` to `0` and configure `USE_GPIO_CONTROL`:
   - `false` for hardware-controlled mode
   - `true` for GPIO-controlled mode
4. Make sure your physical wiring matches the selected mode (only if `USE_INTERNAL_MATRIX_PIN_LOOPBACK = 0`)
5. Upload the sketch

**Important:** 
- When `USE_INTERNAL_MATRIX_PIN_LOOPBACK = 1`, no external connections are needed
- When `USE_INTERNAL_MATRIX_PIN_LOOPBACK = 0`, the wiring configuration must match the `USE_GPIO_CONTROL` setting

## Code Explanation

### Key Functions Used

1. **`begin(baudrate)`**
   - Initializes the UART with the specified baud rate
   - Must be called before setting pins and enabling hardware flow control

2. **`setPins(rxPin, txPin, ctsPin, rtsPin)`**
   - Configures the UART pins
   - Note: The order `begin()` then `setPins()` is important for proper initialization

3. **`uart_internal_loopback(uartNum, rxPin)`** (when `USE_INTERNAL_MATRIX_PIN_LOOPBACK = 1`)
   - Creates internal GPIO matrix connection for TX→RX loopback
   - No external wires needed

4. **`uart_internal_hw_flow_ctrl_loopback(uartNum, ctsPin)`** (when `USE_INTERNAL_MATRIX_PIN_LOOPBACK = 1`)
   - Creates internal GPIO matrix connection for RTS→CTS flow control loopback
   - No external wires needed

2. **`setHwFlowCtrlMode(mode, threshold)`**
   - Enables hardware flow control
   - Modes:
     - `UART_HW_FLOWCTRL_DISABLE`: Disable flow control
     - `UART_HW_FLOWCTRL_RTS`: Enable RX flow control only
     - `UART_HW_FLOWCTRL_CTS`: Enable TX flow control only
     - `UART_HW_FLOWCTRL_CTS_RTS`: Enable full flow control (default)
   - Threshold: Number of bytes in RX FIFO before RTS is asserted (default: 64)

### Example Behavior

The sketch demonstrates:
- Periodic transmission of messages
- Flow control blocking when CTS is HIGH
- Monitoring of RTS/CTS pin states
- Loopback reception of transmitted data

## Expected Output and Behavior

The sketch behavior differs depending on the `USE_GPIO_CONTROL` setting (see Configuration section above for details).

### Mode 1: Hardware-Controlled Flow Control (`USE_GPIO_CONTROL = false`)

**Behavior:**
- RTS and CTS signals are automatically controlled by the UART hardware
- RTS1 is directly connected to CTS1 (hardware loopback)
- Flow control operates automatically without software intervention
- RTS goes LOW when ready to receive, HIGH when buffer is full
- CTS must be LOW for transmission to proceed (automatically controlled by RTS)

**Expected Output:**

```
========================================
ESP32 Hardware Flow Control Demo
========================================

Initializing UART1...
Using hardware-controlled flow control (simple loopback)
UART1 initialized successfully!
Hardware flow control: ENABLED (RTS + CTS)
RX Pin: GPIO26 (for the ESP32 RX1 or board-specific RX1 default)
TX Pin: GPIO27 (for the ESP32 TX1 or board-specific TX1 default)
RTS Pin: GPIO2 (output from UART)
CTS Pin: GPIO4 (input to UART)

NO EXTERNAL PIN CONNECTIONS ARE REQUIRED:
-------------------------
Internal GPIO Matrix connection with flow control mode:
  1. Automatic Internal Connection of TX1 to RX1 - Loopback (via GPIO matrix)
  2. Automatic Internal Connection of GPIO2 (RTS1) to GPIO4 (CTS1) - Flow control loopback

  Note: In this mode, RTS/CTS are automatically controlled by hardware.
  RTS goes LOW when ready to receive, HIGH when buffer is full.
  CTS must be LOW for transmission to proceed.

Starting demonstration in 2 seconds...

=== UART1 Pin Status ===
RX Pin (GPIO26): ESP32 Receiving data  (or board-specific RX1)
TX Pin (GPIO27): ESP32 Transmitting data  (or board-specific TX1)
RTS Pin (GPIO2): Hardware controlled (LOW = ready to receive)
CTS Pin (GPIO4): Hardware controlled (LOW = can transmit)
Note: RTS/CTS pins are hardware-controlled. Connect RTS1 to CTS1 for loopback.
Available for write: 128 bytes
Available to read: 0 bytes
========================

Sending: Message #1: Hello from UART1! Time: 1000 ms
  -> Written: 45 bytes
Received: Message #1: Hello from UART1! Time: 1000 ms

=== UART1 Pin Status ===
RX Pin (GPIO26): ESP32 Receiving data  (or board-specific RX1)
TX Pin (GPIO27): ESP32 Transmitting data  (or board-specific TX1)
RTS Pin (GPIO2): Hardware controlled (LOW = ready to receive)
CTS Pin (GPIO4): Hardware controlled (LOW = can transmit)
Note: RTS/CTS pins are hardware-controlled. Connect RTS1 to CTS1 for loopback.
Available for write: 128 bytes
Available to read: 45 bytes
========================

Sending: Message #2: Hello from UART1! Time: 2000 ms
  -> Written: 45 bytes
Received: Message #2: Hello from UART1! Time: 2000 ms
```

**Key Characteristics:**
- No manual CTS toggling messages
- Flow control happens automatically based on RX buffer state
- RTS/CTS states are not directly readable (hardware-controlled)
- Transmission is always allowed (CTS follows RTS automatically)

### Mode 2: GPIO-Controlled Flow Control (`USE_GPIO_CONTROL = true`)

**Behavior:**
- RTS signal is monitored via GPIO5 (connected to RTS1)
- CTS signal is controlled via GPIO13 (connected to CTS1)
- Software can manually block/allow transmission by controlling GPIO13
- Demonstrates explicit flow control blocking behavior
- Shows how external devices can control UART transmission
- **Note:** This mode only works when `USE_INTERNAL_MATRIX_PIN_LOOPBACK = 0`

**Expected Output:**

```
========================================
ESP32 Hardware Flow Control Demo
========================================

Initializing UART1...
Using GPIO-controlled flow control mode
UART1 initialized successfully!
Hardware flow control: ENABLED (RTS + CTS)
RX Pin: GPIO26 (or board-specific RX1 default)
TX Pin: GPIO27 (or board-specific TX1 default)
RTS Pin: GPIO2 (output from UART)
CTS Pin: GPIO4 (input to UART)

PIN CONNECTIONS REQUIRED:
-------------------------
GPIO-controlled flow control mode:
  1. Connect TX1 to RX1 - Loopback (board-specific pins)
  2. Connect GPIO2 (RTS1) to GPIO5 - Monitor RTS state
  3. Connect GPIO4 (CTS1) to GPIO13 - Control CTS signal

Starting demonstration in 2 seconds...

=== UART1 Pin Status ===
RX Pin (GPIO26): Receiving data  (or board-specific RX1)
TX Pin (GPIO27): Transmitting data  (or board-specific TX1)
RTS Pin (GPIO2): LOW (Ready) (LOW = ready to receive)
CTS Pin (GPIO4): LOW (Clear) (LOW = can transmit)
Available for write: 128 bytes
Available to read: 0 bytes
========================

Sending: Message #1: Hello from UART1! Time: 1000 ms
  -> Written: 45 bytes
Received: Message #1: Hello from UART1! Time: 1000 ms

=== UART1 Pin Status ===
RX Pin (GPIO26): Receiving data  (or board-specific RX1)
TX Pin (GPIO27): Transmitting data  (or board-specific TX1)
RTS Pin (GPIO2): LOW (Ready) (LOW = ready to receive)
CTS Pin (GPIO4): LOW (Clear) (LOW = can transmit)
Available for write: 128 bytes
Available to read: 45 bytes
========================

>>> Allowing transmission (CTS LOW)...
Sending: Message #2: Hello from UART1! Time: 2000 ms
  -> Written: 45 bytes
Received: Message #2: Hello from UART1! Time: 2000 ms

=== UART1 Pin Status ===
RX Pin (GPIO26): Receiving data  (or board-specific RX1)
TX Pin (GPIO27): Transmitting data  (or board-specific TX1)
RTS Pin (GPIO2): LOW (Ready) (LOW = ready to receive)
CTS Pin (GPIO4): HIGH (Blocked) (LOW = can transmit)
Available for write: 128 bytes
Available to read: 45 bytes
========================

>>> Blocking transmission (CTS HIGH)...
!!! Transmission blocked - CTS is HIGH !!!

=== UART1 Pin Status ===
RX Pin (GPIO26): Receiving data  (or board-specific RX1)
TX Pin (GPIO27): Transmitting data  (or board-specific TX1)
RTS Pin (GPIO2): LOW (Ready) (LOW = ready to receive)
CTS Pin (GPIO4): HIGH (Blocked) (LOW = can transmit)
Available for write: 128 bytes
Available to read: 45 bytes
========================

>>> Allowing transmission (CTS LOW)...
Sending: Message #3: Hello from UART1! Time: 3000 ms
  -> Written: 45 bytes
Received: Message #3: Hello from UART1! Time: 3000 ms
```

**Key Characteristics:**
- Explicit messages showing CTS state changes ("Allowing transmission" / "Blocking transmission")
- Transmission blocking messages when CTS is HIGH
- RTS/CTS pin states are readable via GPIO5 and GPIO13
- Demonstrates manual flow control
- Useful for testing flow control behavior or interfacing with external flow control logic
- Only works when `USE_INTERNAL_MATRIX_PIN_LOOPBACK = 0`

## Troubleshooting

1. **No data received**: 
   - If `USE_INTERNAL_MATRIX_PIN_LOOPBACK = 1`: Check that the internal loopback functions are being called
   - If `USE_INTERNAL_MATRIX_PIN_LOOPBACK = 0`: Check that TX1 is connected to RX1
2. **Transmission always blocked**: Verify CTS pin connection and state (only applies when `USE_INTERNAL_MATRIX_PIN_LOOPBACK = 0`)
3. **RTS always HIGH**: RX buffer may be full, try reading data
4. **Compilation errors**: Ensure you're using ESP32 Arduino Core 2.0.0 or later
5. **GPIO-controlled mode not working**: Make sure `USE_INTERNAL_MATRIX_PIN_LOOPBACK = 0` (internal loopback overrides GPIO control)

## Notes

- **Internal Loopback Mode** (`USE_INTERNAL_MATRIX_PIN_LOOPBACK = 1`): No external connections needed! The ESP32 GPIO matrix handles all connections internally. This is the easiest way to test hardware flow control.
- **External Wire Mode** (`USE_INTERNAL_MATRIX_PIN_LOOPBACK = 0`): Requires physical connections between RTS and CTS pins (and TX/RX for data loopback)
- The threshold parameter controls when RTS is asserted (default: 64 bytes = half of 128-byte FIFO)
- Flow control is most useful when communicating with devices that support it (modems, some sensors, etc.)
- For simple point-to-point communication without flow control support, you can disable it

