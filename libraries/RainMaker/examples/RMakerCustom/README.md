# ESP RainMaker Custom Device

This example demonstrates how to build a custom device to be used with ESP RainMaker. 

## What to expect in this example?

- This example sketch uses the on board Boot button and GPIO16 to demonstrate an ESP RainMaker AC dimmer device.
- After compiling and flashing the example, add your device using the [ESP RainMaker phone apps](https://rainmaker.espressif.com/docs/quick-links.html#phone-apps) by scanning the QR code.
- Toggling the state from the phone app will toggle the dimmer state (GPIO16).
- Pressing the Boot button will toggle the dimmer state (GPIO16) and the same will reflect on the phone app.
- You can also change the Level from the phone app and see it reflect on the device as a print message.

### Output

```
[    87][I][RMaker.cpp:13] event_handler(): RainMaker Initialised.
[    94][I][WiFiProv.cpp:158] beginProvision(): Already Provisioned
[    95][I][WiFiProv.cpp:162] beginProvision(): Attempting connect to AP: Viking007_2GEXT

Received value = false for Dimmer - Power
Toggle State to true.
[ 22532][I][RMakerDevice.cpp:162] updateAndReportParam(): Device : Dimmer, Param Name : Power, Val : true

Received value = 73 for Dimmer - Level
```

### Resetting the device
- Press and Hold the Boot button for more than 3 seconds and then release to reset Wi-Fi configuration.
- Press and Hold the Boot button for more than 10 seconds and then release to reset to factory defaults.
