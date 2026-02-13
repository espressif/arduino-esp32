# Baud Rate Detection Demo

## Supported SoCs

| SoC | Support |
|-----|---------|
| ESP32 | ✓ Supported |
| ESP32-S2 | ✓ Supported |
| ESP32-S3 | ✗ Not Supported |
| ESP32-C3 | ✗ Not Supported |
| ESP32-C6 | ✗ Not Supported |
| ESP32-H2 | ✗ Not Supported |
| ESP32-P4 | ✗ Not Supported |
| ESP32-C61 | ✗ Not Supported |

This example demonstrates how to automatically detect and set the baud rate when any available UART has its RX pin connected to an external UART port sending data, using ESP32's HardwareSerial API.

## Overview

Baud rate detection is a feature that automatically determines the communication speed of incoming data. This is useful when the receiving device doesn't know the baud rate of the sender in advance. The sketch will attempt to detect the baud rate by analyzing incoming data and can then configure itself to match.

The example detects the baud rate on UART0 (`Serial` or `Serial0`), but it can detect it for any other available UART port (ESP32/S2 `Serial1` or ESP32 `Serial2`).

## Key Features

- **Automatic detection** of baud rates in the range of 300 to 230400 baud
- **Configurable timeout** (default: 20 seconds) during which the UART listens for incoming data
- **Fallback mechanism** - if detection fails, automatically sets a safe default baud rate (115200)
- **Simple setup** - no external hardware or configuration needed

## Configuration Options

### Detection Timeout

The timeout can be customized by passing the `timeout_ms` parameter:

```cpp
Serial.begin(baud, config, rxPin, txPin, invert, timeout_ms, rxfifo_full_thrhd)
```

**Default timeout:** 20 seconds (20000 ms)

### Baud Rate Range

The baud rate detection works over a wide range of speeds (typically from 300 baud up to high-speed rates such as 921600 baud). The detected value is rounded to the nearest entry in the core’s internal baud-rate table used by `uartDetectBaudrate()`.

Examples of baud rates that can be detected include: 300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 74880, 115200, 230400, 256000, 460800, 921600, and others defined by the core.
## How Baud Rate Detection Works

1. **Detection Phase**:
   - Call `Serial.begin(0)` to start the detection process
   - The UART analyzes incoming data bits to determine the baud rate
   - Detection continues for the specified timeout period (default: 20 seconds)
   - Any data received during this time is analyzed

2. **Result Handling**:
   - If successful: `Serial.baudRate()` returns the detected baud rate
   - If failed: `Serial.baudRate()` returns 0 (timeout reached with no valid detection)

3. **Fallback Configuration**:
   - If detection fails, manually set a safe baud rate (e.g., 115200)
   - This ensures the UART is in a known state

### Detection Logic

The sketch attempts to detect the baud rate by:
- Measuring the timing of incoming data bits
- Calculating the most likely baud rate based on bit timing
- Confirming the detection with received data patterns
- Returning the detected baud rate or 0 if unsuccessful

## Pin Connections

**No external connections are required!** Baud rate detection works on the standard UART0 pins (RX0 and TX0), which are typically:

- **RX0**: GPIO3 (for ESP32 and most variants)
- **TX0**: GPIO1 (for ESP32 and most variants)

**Note:** Pin numbers may vary depending on your specific ESP32 board. Refer to your board's documentation for the actual GPIO assignments.

## How to Use

1. Open the Arduino IDE
2. Open `BaudRateDetect_Demo.ino`
3. Connect your ESP32 to the USB cable
4. Select the correct board and COM port
5. Upload the sketch
6. Open the Serial Monitor
7. Set the Serial Monitor to any baud rate (e.g., 115200)
8. Type and send characters - the sketch will detect the baud rate

### Example Usage Scenario

- ESP32 is connected to an external device sending data at an unknown baud rate
- Upload the sketch to start listening
- The ESP32 will automatically detect the baud rate of the incoming data
- Once detected, the sketch prints the detected baud rate to the console
- If detection fails after 20 seconds, the sketch falls back to 115200 baud

## Code Explanation

### Key Functions Used

1. **`begin(baud, config, rxPin, txPin, invert, timeout_ms, rxfifo_full_thrhd)`**
   - Initializes the UART with the specified baud rate
   - When `baud = 0`, starts the automatic baud rate detection process
   - `timeout_ms`: Time (in milliseconds) to wait for detection (default: 20000 ms)
   - Returns after timeout expires or detection is successful

2. **`baudRate()`**
   - Returns the current baud rate of the UART
   - Returns 0 if no valid baud rate is set or detection failed

3. **`end()`**
   - Closes/deinitializes the UART
   - Must be called before reconfiguring the UART with a new baud rate

### Typical Code Pattern

```cpp
void setup() {
  Serial.begin(0);  // Start baud rate detection
  
  Serial.print("The baud rate is ");
  Serial.println(Serial.baudRate());
  
  // Check if detection was successful
  if (Serial.baudRate() == 0) {
    // Detection failed, set a safe default
    Serial.end();
    Serial.begin(115200);  // Fallback to 115200 baud
    Serial.println("Baud rate detection failed. Set to 115200.");
  }
}
```

## Expected Output

**Successful Detection**

```
==>The baud rate is approximately 115200 (for example: 115200 or 115201)

[Detection successful - sketch continues normally]
```

**Failed Detection**

```
[20-second timeout period with no valid data]

==>The baud rate is 0

[error] Baud rate detection failed.

[Falls back to 115200 baud rate]
```

## Troubleshooting

1. **Detection times out (returns 0)**:
   - Ensure the external device is actually sending data during the 20-second window
   - Verify the external device's baud rate settings
   - Check the RX0 pin connection for signal integrity
   - Make sure data is being sent continuously during the detection period

2. **Wrong baud rate detected**:
   - Verify the external device is using a standard baud rate (listed in Baud Rate Range section)
   - Ensure the data pattern is clear enough for detection
   - Try increasing the amount of data sent during detection

3. **Sketch not responding after upload**:
   - Open Serial Monitor at 115200 baud
   - Try typing characters to trigger a response
   - Check the console output to see what baud rate was detected

4. **Compilation errors**:
   - Ensure you're using ESP32 Arduino Core 2.0.0 or later
   - Verify that the ESP32 board package is properly installed

## Advanced: Custom Timeout

To change the detection timeout from the default 20 seconds, modify the `Serial.begin()` call:

```cpp
// 10-second timeout for detection using UART0
Serial.begin(0, SERIAL_8N1, RX0, TX0, false, 10000);

// 5-second timeout for faster detection using UART1
Serial1.begin(0, SERIAL_8N1, RX1, TX1, false, 5000);

// 30-second timeout for slower or intermittent data using UART2 (ESP32 only)
Serial2.begin(0, SERIAL_8N1, RX2, TX2, false, 30000);
```

**Note:** The timeout applies only to the detection phase. After the timeout expires or detection completes, the function returns and the sketch continues.

## Notes

- Baud rate detection is most reliable when the external device sends continuous data with clear signal patterns
- The detection process consumes CPU cycles; consider this if your sketch has real-time requirements
- Once the baud rate is detected, communication proceeds normally
- For applications requiring specific baud rates, you can still use `Serial.begin(baudrate)` with an explicit baud rate instead of relying on auto-detection
- The sketch demonstrates graceful fallback behavior, which is a best practice for production code

