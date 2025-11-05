# Matter Thread Light Example

This example demonstrates a Matter Light device that uses Thread network commissioning exclusively. Thread is a low-power, mesh networking protocol that's ideal for Matter smart home devices.

## Features

- **Thread-only commissioning**: Device can only be commissioned over Thread network
- **On/Off Light control**: Simple light that can be controlled via Matter
- **Automatic fallback**: On non-Thread capable devices, falls back to Wi-Fi
- **Status monitoring**: Displays network and commissioning status
- **Decommissioning support**: Long-press boot button to decommission

## Hardware Requirements

### Thread-Capable ESP32 Variants
- **ESP32-H2**: Native Thread support
- **ESP32-C6**: Native Thread support
- **ESP32-C5**: Native Thread support

### Other ESP32 Variants
- **ESP32, ESP32-S2, ESP32-S3, ESP32-C3**: Will automatically fall back to Wi-Fi commissioning

## Hardware Setup

### Basic Setup
```
ESP32-H2/C6/C5 Board
├── LED (GPIO 2 or LED_BUILTIN) - Visual feedback
└── Boot Button (GPIO 0) - Decommissioning
```

### Pin Configuration
- **LED Pin**: Uses `LED_BUILTIN` if available, otherwise GPIO 2
- **Button Pin**: Uses `BOOT_PIN` (typically GPIO 0)

## Network Requirements

### Thread Border Router
To use Thread commissioning, you need a Thread Border Router in your network:

**Apple HomePod/Apple TV (tvOS 15+)**
```
- Supports Thread Border Router functionality
- Automatically discovered by Matter controllers
```

**Google Nest Hub Max (2nd gen)**
```
- Built-in Thread Border Router
- Works with Google Home ecosystem
```

**OpenThread Border Router**
```
- Raspberry Pi with Thread radio
- Custom Thread Border Router setup
```

## Usage

### 1. Upload and Monitor
```bash
# Upload the sketch to your ESP32-H2/C6
# Open Serial Monitor at 115200 baud
```

### 2. Check Thread Status
The device will display:
```
=== Matter Thread Light Status ===
Device commissioned: No
Device connected: No
Thread connected: No
Thread commissioning: Enabled
Wi-Fi commissioning: Disabled
Light state: OFF
================================
```

### 3. Commission the Device
Use a Matter controller that supports Thread:

**Apple Home**
1. Open Home app on iPhone/iPad
2. Tap "Add Accessory"
3. Scan QR code or enter manual pairing code
4. Follow commissioning steps

**Google Home**
1. Open Google Home app
2. Tap "Add" → "Set up device"
3. Choose "Works with Google"
4. Scan QR code or enter pairing code

### 4. Control the Light
Once commissioned, you can:
- Turn the light on/off from your Matter controller
- Monitor status in Serial Monitor
- See LED respond to commands

### 5. Decommission (if needed)
1. Hold the boot button for 5+ seconds
2. Device will reset and become available for commissioning again

## Serial Output

### Successful Thread Commissioning
```
Starting Thread-only Matter Light...
✓ Thread-only commissioning mode enabled
Matter Node is not commissioned yet.
Manual pairing code: 12345678901
QR code URL: https://...

This device is configured for Thread commissioning only.
Make sure your Matter controller supports Thread commissioning.

Waiting for Matter commissioning...
Looking for Thread Border Router...
Thread connected: Yes
Device commissioned: Yes
Light ON
```

### Non-Thread Device Fallback
```
Starting Thread-only Matter Light...
⚠ Thread support not compiled in, using Wi-Fi commissioning
Matter Node is not commissioned yet.
Wi-Fi commissioning: Enabled
```

## Troubleshooting

### Device Not Commissioning
**Problem**: Device stuck on "Waiting for Matter commissioning..."

**Solutions**:
1. **Check Thread Border Router**:
   - Ensure Thread Border Router is online
   - Verify controller supports Thread commissioning

2. **Verify Network**:
   - Check Thread network is operational
   - Ensure Matter controller is on same network as Border Router

3. **Reset and Retry**:
   - Hold boot button for 5+ seconds to decommission
   - Try commissioning again

### Thread Connection Issues
**Problem**: "Thread connected: No" in status

**Solutions**:
1. **Border Router Setup**:
   - Verify Thread Border Router is functioning
   - Check Border Router connectivity

2. **Device Placement**:
   - Move device closer to Thread Border Router
   - Ensure no interference with Thread radio

3. **Thread Network**:
   - Verify Thread network credentials
   - Check Thread network topology

### Compilation Issues
**Problem**: Compilation errors related to Thread

**Solutions**:
1. **ESP-IDF Version**:
   ```bash
   # Ensure ESP-IDF supports Thread for your board
   # Update to latest ESP-IDF if needed
   ```

2. **Board Configuration**:
   ```bash
   # Make sure you've selected the correct board
   # ESP32-H2/C6/C5 for Thread support
   ```

## API Reference

### Network Commissioning Control
```cpp
// Enable Thread-only commissioning
Matter.setNetworkCommissioningMode(false, true);

// Check Thread commissioning status
bool enabled = Matter.isThreadNetworkCommissioningEnabled();

// Check Thread connection
bool connected = Matter.isThreadConnected();
```

### Device Status
```cpp
// Check if device is commissioned
bool commissioned = Matter.isDeviceCommissioned();

// Check if device has network connectivity
bool connected = Matter.isDeviceConnected();
```

### Light Control
```cpp
// Set light state
OnOffLight.setOnOff(true);  // Turn on
OnOffLight.setOnOff(false); // Turn off

// Get current state
bool isOn = OnOffLight.getOnOff();

// Toggle light
OnOffLight.toggle();
```

## Thread vs Wi-Fi Comparison

| Feature | Thread | Wi-Fi |
|---------|--------|------|
| **Power Consumption** | Low | Higher |
| **Range** | Mesh network, self-healing | Single point to router |
| **Setup** | Requires Border Router | Direct to Wi-Fi router |
| **Reliability** | High (mesh redundancy) | Depends on Wi-Fi quality |
| **Device Limit** | 250+ devices per network | Limited by router |
| **Security** | Built-in mesh security | WPA2/WPA3 |

## Related Examples

- **[SimpleNetworkCommissioning](../SimpleNetworkCommissioning/)**: Basic Wi-Fi/Thread selection
- **[MatterNetworkCommissioning](../MatterNetworkCommissioning/)**: Interactive commissioning control
- **[MatterMinimum](../MatterMinimum/)**: Simplest Wi-Fi-only setup
- **[MatterOnOffLight](../MatterOnOffLight/)**: Wi-Fi-based light with persistence

## Further Reading

- [Thread Group Specification](https://www.threadgroup.org/)
- [OpenThread Documentation](https://openthread.io/)
- [Matter Thread Integration Guide](https://github.com/project-chip/connectedhomeip/blob/master/docs/guides/thread_primer.md)
- [ESP32 Thread Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/thread.html)
