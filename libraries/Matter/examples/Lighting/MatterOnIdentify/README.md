# Matter On Identify Example

This example demonstrates how to implement the Matter Identify cluster callback for an on/off light device using an ESP32 SoC microcontroller.\
The application showcases Matter commissioning, device control via smart home ecosystems, and the Identify feature. `onIdentify(bool)` starts or stops feedback. `getIdentifyRequest()` tells the sketch whether the event was IdentifyTime or TriggerEffect (Blink, Breathe, Okay, ChannelChange) so each can look different.

## Supported Targets

| SoC      | This sketch            | CHIPoBLE | Also in prebuild       | LED      |
| -------- | ---------------------- | -------- | ---------------------- | -------- |
| ESP32    | Wi-Fi (SSID in sketch) | Off      | Ethernet (EMAC or SPI) | Required |
| ESP32-S2 | Wi-Fi (SSID in sketch) | Off      | Ethernet (SPI)         | Required |
| ESP32-S3 | Wi-Fi (hub)            | On       | Ethernet (SPI)         | Required |
| ESP32-C3 | Wi-Fi (hub)            | On       | Ethernet (SPI)         | Required |
| ESP32-C5 | Wi-Fi (hub)            | On       | Ethernet (SPI)         | Required |
| ESP32-C6 | Wi-Fi (hub, default)   | On       | Thread, Ethernet (SPI) | Required |
| ESP32-H2 | Thread (hub)           | On       | Ethernet (SPI)         | Required |

### Note on Commissioning

This table is what **this sketch** does. It does not call `Matter.selectNetwork()` or start Ethernet.

- **ESP32 / ESP32-S2:** no CHIPoBLE in the Arduino IDE prebuild. The sketch calls `WiFi.begin(ssid, password)`.
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

- Matter protocol implementation for an on/off light device
- Default network and CHIPoBLE as in the Supported Targets table (ESP32-C6 dual-stack uses Wi-Fi unless you call `selectNetwork()`)
- On Identify callback plus `getIdentifyRequest()` for effect details
- Visual identification feedback that differs by request (IdentifyTime vs TriggerEffect Blink / Breathe / Okay / ChannelChange)
- Button control for factory reset (decommission)
- Matter commissioning via QR code or manual pairing code
- Integration with Home Assistant, Apple Home, Amazon Alexa, and Google Home

## Hardware Requirements

- ESP32 compatible development board (see supported targets table)
- LED connected to GPIO pin (or using built-in LED) for visual feedback
- Optional: RGB LED for red blinking identification (uses RGB_BUILTIN if available)
- User button for factory reset (uses BOOT button by default)

## Pin Configuration

- **LED**: Uses `LED_BUILTIN` if defined, otherwise pin 2
- **Button**: Uses `BOOT_PIN` by default

## Software Setup

### Prerequisites

1. Install the Arduino IDE (2.0 or newer recommended)
2. Install ESP32 Arduino Core with Matter support
3. ESP32 Arduino libraries:
   - `Matter`
   - `WiFi` (only for ESP32 and ESP32-S2)

### Configuration

Before uploading the sketch, configure the following:

1. **Wi-Fi credentials** (if not using BLE commissioning - mandatory for ESP32 | ESP32-S2):
   ```cpp
   const char *ssid = "your-ssid";         // Change to your Wi-Fi SSID
   const char *password = "your-password"; // Change to your Wi-Fi password
   ```

2. **LED pin configuration** (if not using built-in LED):
   ```cpp
   const uint8_t ledPin = 2;  // Set your LED pin here
   ```

3. **Button pin configuration** (optional):
   By default, the `BOOT` button (GPIO 0) is used for factory reset. You can change this to a different pin if needed.
   ```cpp
   const uint8_t buttonPin = BOOT_PIN;  // Set your button pin here
   ```

## Building and Flashing

1. Open the `MatterOnIdentify.ino` sketch in the Arduino IDE.
2. Select your ESP32 board from the **Tools > Board** menu.
<!-- vale off -->
3. Select **"Huge APP (3MB No OTA/1MB SPIFFS)"** from **Tools > Partition Scheme** menu.
<!-- vale on -->
4. Enable **"Erase All Flash Before Sketch Upload"** option from **Tools** menu.
5. Connect your ESP32 board to your computer via USB.
6. Click the **Upload** button to compile and flash the sketch.

## Expected Output

Once the sketch is running, open the Serial Monitor at a baud rate of **115200**. Wi-Fi connection messages appear only on ESP32 and ESP32-S2. CHIPoBLE targets get the operational network from the hub (Wi-Fi, or Thread on ESP32-C5 / ESP32-C6 / ESP32-H2). You should see output similar to the following, which provides the necessary information for commissioning:

```
Connecting to your-wifi-ssid
.......
Wi-Fi connected

Matter Node is not commissioned yet.
Initiate the device discovery in your Matter environment.
Commission it to your Matter hub with the manual pairing code or QR code
Manual pairing code: 34970112332
QR code URL: https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT%3A6FCJ142C00KA0648G00
Matter Node not commissioned yet. Waiting for commissioning.
Matter Node not commissioned yet. Waiting for commissioning.
...
Matter Node is commissioned and connected to the network. Ready for use.
```

When you trigger Identify from a Matter app, you should see a line such as:
```
Identify Active (IdentifyTime effect=IdentifyTime 0x00 variant=0)
Identify Inactive (IdentifyTime effect=IdentifyTime 0x00 variant=0)
```

The name is `IdentifyTime` whenever `fromTriggerEffect` is false. CHIP may still pass leftover effect id `0x00` (Blink default) on START/STOP; this sketch does not treat that as a Blink effect.

A `TriggerEffect` Blink looks like:
```
Identify Active (TriggerEffect effect=Blink 0x00 variant=0)
```

Okay is a single short flash (`TriggerEffect effect=Okay`). Breathe is a slower pulse for about 15 seconds. ChannelChange is a faster blink for about 8 seconds. One-shot TriggerEffect has no later STOP; the sketch times those animations out.

## Using the Device

### Manual Control

The user button (BOOT button by default) provides factory reset functionality:

- **Long press (>5 seconds)**: Factory reset the device (decommission)

### Identify Feature

The Identify feature allows you to visually identify a specific device from your Matter app.

`onIdentify(bool)` still means start (`true`) or stop (`false`). Call `OnOffLight.getIdentifyRequest()` in that callback for the last event:

| Request | How this sketch behaves |
| --- | --- |
| Identify / IdentifyTime (`fromTriggerEffect == false`) | Blink every 500 ms until the controller sends STOP |
| TriggerEffect Blink | Fast blink for about 2 s, then the sketch stops itself |
| TriggerEffect Breathe | Slow pulse for about 15 s |
| TriggerEffect Okay | One short flash |
| TriggerEffect ChannelChange | Fast blink for about 8 s |
| TriggerEffect Stop / Finish | `onIdentify(false)` — stop immediately |

RGB LED (RGB_BUILTIN) blinks red. A regular LED toggles. When identify ends, the LED returns to its previous on/off state.

Which command the app sends depends on the controller. Apple Home often uses IdentifyTime. Some apps send TriggerEffect. Watch the Serial line to see which request arrived.

### How to Trigger Identify

#### Home Assistant

1. Open Home Assistant
2. Open the Matter device
3. Use Identify if the controller exposes it
4. The LED will start blinking

#### Apple Home

1. Open the Home app on your iOS device
2. Find your device in the device list
3. Long press on the device
4. Tap "Identify" or look for the identify option in device settings
5. The LED will start blinking

#### Amazon Alexa

1. Open the Alexa app
2. Navigate to your device
3. Look for "Identify" or "Find Device" option in device settings
4. The LED will start blinking

#### Google Home

1. Open the Google Home app
2. Select your device
3. Look for "Identify" or "Find Device" option
4. The LED will start blinking

### Smart Home Integration

Use a Matter-compatible hub (like a Home Assistant server, Apple HomePod, Google Nest Hub, or Amazon Echo) to commission the device.

#### Home Assistant

1. Open Home Assistant
2. Go to Settings > Devices & services > Add integration > Matter
3. Scan the QR code from the Serial Monitor, or enter the manual pairing code
4. Follow the prompts to complete setup

#### Apple Home

1. Open the Home app on your iOS device
2. Tap the "+" button > Add Accessory
3. Scan the QR code displayed in the Serial Monitor, or
4. Tap "I Don't Have a Code or Cannot Scan" and enter the manual pairing code
5. Follow the prompts to complete setup
6. The device will appear as an on/off light in your Home app
7. Use the Identify feature to visually locate the device

#### Amazon Alexa

1. Open the Alexa app
2. Tap More > Add Device > Matter
3. Select "Scan QR code" or "Enter code manually"
4. Complete the setup process
5. The light will appear in your Alexa app
6. Use the Identify feature to visually locate the device

#### Google Home

1. Open the Google Home app
2. Tap "+" > Set up device > New device
3. Choose "Matter device"
4. Scan the QR code or enter the manual pairing code
5. Follow the prompts to complete setup
6. Use the Identify feature to visually locate the device

## Code Structure

The MatterOnIdentify example consists of the following main components:

1. **`setup()`**: Initializes hardware (button, LED), configures Wi-Fi (if needed), initializes the Matter on/off light endpoint, registers the on/off callback and the Identify callback, and starts the Matter stack.

2. **`loop()`**: Times Identify animations with `millis()` (period and optional deadline from the request), handles button input for factory reset, and allows the Matter stack to process events.

3. **Callbacks**:
   - `onOffLightCallback()`: Controls the physical LED based on on/off state from Matter controller.
   - `onIdentifyLightCallback()`: `onIdentify(bool)` start/stop. Calls `getIdentifyRequest()` to choose IdentifyTime vs TriggerEffect Blink / Breathe / Okay / ChannelChange.

4. **Identify feedback**:
   - RGB LEDs: red (brightness 32); regular LEDs: on/off
   - IdentifyTime: 500 ms blink until STOP
   - TriggerEffect: sketch applies the duration in the table above (no CHIP STOP)

## Troubleshooting

- **Device not visible during commissioning**: Ensure Wi-Fi or Thread connectivity is properly configured
- **LED not responding**: Verify pin configurations and connections
- **Identify feature not working**: Ensure the device is commissioned and you're using a Matter app that supports the Identify cluster. Some apps may not have a visible Identify button
- **LED not blinking during identify**: Check Serial Monitor for `Identify Active`. If you don't see it, the Identify command may not be reaching the device
- **LED state not restored after identify**: The code uses a double-toggle to restore state. If this doesn't work, ensure the light state is properly tracked
- **Failed to commission**: Try factory resetting the device by long-pressing the button. Other option would be to erase the SoC Flash Memory by using `Arduino IDE Menu` -> `Tools` -> `Erase All Flash Before Sketch Upload: "Enabled"` or directly with `esptool.py --port <PORT> erase_flash`
- **No serial output**: Check baudrate (115200) and USB connection

## Related Documentation

- [Matter Overview](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter.html)
- [Matter Endpoint Base Class](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/matter_ep.html)
- [Matter On/Off Light Endpoint](https://docs.espressif.com/projects/arduino-esp32/en/latest/matter/ep_on_off_light.html)

## License

This example is licensed under the Apache License, Version 2.0.
