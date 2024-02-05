# ESP RainMaker Custom Device

This example demonstrates how to build a custom device to be used with ESP RainMaker using Mode, Range and Toggle Parameters. 

## What to expect in this example?

- This example sketch uses the on board Boot button and GPIOs 16, 17, 18, 19, 21, 22 to demonstrate an ESP RainMaker AirCooler device.
- After compiling and flashing the example, add your device using the [ESP RainMaker phone apps](https://rainmaker.espressif.com/docs/quick-links.html#phone-apps) by scanning the QR code.
- Toggling the power state from the phone app will toggle GPIO 16.
- Pressing the Boot button will toggle the power state (GPIO 16) and the same will reflect on the phone app.
- Toggling the swing state from the phone app will toggle GPIO 17.
- Changing the mode from the phone app will toggle the GPIOs 18 (auto), 19 (cool) and 21 (heat)
- Changing the Speed slider from the phone app will dimming GPIO 22
- You can also change the Level from the phone app and see it reflect on the device as a print message.

### Output

```
Received value = true for Air Cooler - Power
Received value = false for Air Cooler - Power
Received value = true for Air Cooler - Swing
Received value = false for Air Cooler - Swing
Received value = 0 for Air Cooler - Speed
Received value = 255 for Air Cooler - Speed
Received value = Auto for Air Cooler - Mode
Received value = Cool for Air Cooler - Mode
Received value = Heat for Air Cooler - Mode
Toggle power state to false.
Toggle power state to false.
```

### Resetting the device
- Press and Hold the Boot button for more than 3 seconds and then release to reset Wi-Fi configuration.
- Press and Hold the Boot button for more than 10 seconds and then release to reset to factory defaults.
