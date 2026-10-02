# Matter Simple Blinds Example

This is a minimal lift-only window covering. The hub writes Target; the sketch simulates a motor at **1% every 200 ms**, reports Current while it moves, and sets Lift Opening / Closing / Stall so the app can show travel.

## Supported Targets

| SoC      | This sketch            | CHIPoBLE | Also in prebuild       |
| -------- | ---------------------- | -------- | ---------------------- |
| ESP32    | Wi-Fi (SSID in sketch) | Off      | Ethernet (EMAC or SPI) |
| ESP32-S2 | Wi-Fi (SSID in sketch) | Off      | Ethernet (SPI)         |
| ESP32-S3 | Wi-Fi (hub)            | On       | Ethernet (SPI)         |
| ESP32-C3 | Wi-Fi (hub)            | On       | Ethernet (SPI)         |
| ESP32-C5 | Wi-Fi (hub)            | On       | Ethernet (SPI)         |
| ESP32-C6 | Wi-Fi (hub, default)   | On       | Thread, Ethernet (SPI) |
| ESP32-H2 | Thread (hub)           | On       | Ethernet (SPI)         |

### Note on Commissioning

This table is what **this sketch** does. It does not call `Matter.selectNetwork()` or start Ethernet.

- **ESP32 / ESP32-S2:** no CHIPoBLE in the Arduino IDE prebuild. The sketch calls `matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD)`.
- **ESP32-C6:** prebuild is dual-stack. Without `selectNetwork()` this sketch uses **Wi-Fi + CHIPoBLE**. Thread stays unused.
- **ESP32-H2:** Thread + CHIPoBLE (no Wi-Fi).
- **ESP32-C5:** Wi-Fi + CHIPoBLE by default (Tools → Matter Network → Wi-Fi). Thread is Tools → Matter Network → Thread.

To change the path, call `Matter.selectNetwork()` **before** any accessory `begin()`. On-network: `selectNetwork(net, true)` (CHIPoBLE off). CHIPoBLE: `selectNetwork(net)` (BLE stays on). Do not also call `setBLECommissioningEnabled()`.

- Wi-Fi + CHIPoBLE: [MatterCHIPoBLEWiFi](../../Commissioning/MatterCHIPoBLEWiFi)
- Wi-Fi on-network (CHIPoBLE off): [MatterOnNetworkWiFi](../../Commissioning/MatterOnNetworkWiFi)
- Thread + CHIPoBLE (ESP32-C5 / ESP32-C6 / ESP32-H2): [MatterCHIPoBLEThread](../../Commissioning/MatterCHIPoBLEThread)
- Thread on-network (ESP32-C5 / ESP32-C6 / ESP32-H2): [MatterOnNetworkThread](../../Commissioning/MatterOnNetworkThread)
- Ethernet (CHIPoBLE off): [MatterOnNetworkEthernet](../../Commissioning/MatterOnNetworkEthernet)

## Features

- Lift-only `ROLLERSHADE` (0 = open, 100 = closed in Matter lift scale)
- Simulated motor: **1 percent every 200 ms** (full travel about 20 s). A new Target mid-move changes direction
- Reports `CurrentPositionLiftPercent100ths` each step and Lift `OperationalState` (Opening / Closing / Stall)
- Single `onGoToLiftPercentage()` callback when Target changes
- Matter commissioning via QR code or manual pairing code
- Integration with Home Assistant, Apple Home, Amazon Alexa, and Google Home

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- Window covering motor/actuator (optional for testing - example simulates movement)

## Software Setup

### Prerequisites

1. Install the Arduino IDE (2.0 or newer recommended)
2. Install ESP32 Arduino Core with Matter support
3. ESP32 Arduino libraries:
   - `Matter`
   - `WiFi` (only for ESP32 and ESP32-S2)

### Configuration

Before uploading the sketch, configure the following:

1. **Wi-Fi Credentials** (for ESP32 and ESP32-S2 only):
   ```cpp
   #define WIFI_SSID "your-ssid"
   #define WIFI_PASSWORD "your-password"
   ```

## Building and Flashing

1. Open the `MatterSimpleBlinds.ino` sketch in the Arduino IDE.
2. Select your ESP32 board from the **Tools > Board** menu.
<!-- vale off -->
3. Select **"Huge APP (3MB No OTA/1MB SPIFFS)"** from **Tools > Partition Scheme** menu.
<!-- vale on -->
4. Enable **"Erase All Flash Before Sketch Upload"** option from **Tools** menu.
5. Connect your ESP32 board to your computer via USB.
6. Click the **Upload** button to compile and flash the sketch.

## Expected Output

```
============================
Matter Simple Blinds Example
============================

Connecting to your-ssid
.......
Wi-Fi connected
IP address: 192.168.1.100

Matter Node is not commissioned yet.
Commission it using the pairing code or QR code.
Manual pairing code: 34970112332
QR code URL: https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT%3A6FCJ142C00KA0648G00
[ready] net=wifi commissioned=N connected=N controller=N
[ready] net=wifi commissioned=Y connected=Y controller=N
...
[ready] net=wifi commissioned=Y connected=Y controller=Y
Controller CASE session is up.
Matter started
```

When the hub sets a new lift target (starts closed at 100%):
```
Window Covering change request: Lift=50%
Opening: 100% -> 50%
Lift reached 50%
```

Alexa shows Opening or Closing and the moving lift position until Stall. A new Target while moving retargets the simulation.

## Usage

1. **Commissioning**: Use the QR code or manual pairing code to commission the device to your Matter hub (Home Assistant, Apple Home, Google Home, or Amazon Alexa).

2. **Control**: Set lift 0-100% from the app. The callback accepts Target and starts Opening or Closing; `loop()` steps Current by 1% every 200 ms until Stall.

## Code Structure

- **`onBlindsLift()`**: Accepts the new Target (PRE_UPDATE: `liftPercent` / `getTargetLiftPercent100ths()` are the request) and sets Lift Opening or Closing. Does not snap Current.
- **`simulateLiftStep()`**: From `loop()`, 1% every 200 ms via `setCurrentLiftPercent100ths()`. On arrival, copies Target 100ths and sets Stall.
- **`setup()`**: Wi-Fi if needed, `ROLLERSHADE` at 100% closed, callback, Matter, `matterWaitUntilReady()`.
- **`loop()`**: `matterRestartIfNoFabric()` and the lift simulation.

## Customization

### Adding Motor Control

Keep the same reporting contract as the simulation. Drive the motor toward `liftPercent`, set Opening/Closing when it starts, write Current as it moves, and on arrival:

```cpp
WindowBlinds.setCurrentLiftPercent100ths(WindowBlinds.getTargetLiftPercent100ths());
WindowBlinds.setOperationalState(MatterWindowCovering::LIFT, MatterWindowCovering::STALL);
```

Change `kSimStepPercent` / `kSimStepMs` to match a slower or faster shade.

## Troubleshooting

1. **Device not discoverable**: Ensure Wi-Fi is connected (for ESP32/ESP32-S2) or BLE is enabled (for other chips).

2. **Lift percentage not updating**: Current is reported from `loop()`, not from the callback. Confirm `simulateLiftStep()` runs and that the last step writes `getTargetLiftPercent100ths()` plus Lift `STALL`.

3. **Alexa stays on "Opening" after the shade should have arrived**: Current 100ths must equal Target. The last step uses `getTargetLiftPercent100ths()` (cached new request; a cluster read in PRE_UPDATE is still the previous Target).

4. **Motor not responding**: Replace `simulateLiftStep()` with your motor, but keep the same Current and Stall reports when it arrives.

## Notes

- `ROLLERSHADE` (lift only). 0 = open, 100 = closed in Matter lift scale. Boot position is 100 percent closed.
- The sketch simulates travel (1%/200 ms). A real motor should report Current while moving and Stall when it arrives.
- `onGoToLiftPercentage()` accepts Target only. Current is updated from `loop()`.
