# ESP RainMaker Switch

This example demonstrates how to build a switch device to be used with ESP RainMaker.

## What to expect in this example?

- This example sketch uses the on board Boot button and GPIO16 to demonstrate an ESP RainMaker switch device.
- After compiling and flashing the example, add your device using the [ESP RainMaker phone apps](https://rainmaker.espressif.com/docs/quick-links.html#phone-apps) by scanning the QR code.
- Toggling the state from the phone app will toggle the switch state (GPIO16).
- Pressing the Boot button will toggle the switch state (GPIO16) and the same will reflect on the phone app.

### Output

```
[    63][I][RMaker.cpp:13] event_handler(): RainMaker Initialised.
[    69][I][WiFiProv.cpp:158] beginProvision(): Already Provisioned
[    69][I][WiFiProv.cpp:162] beginProvision(): Attempting connect to AP: Viking007_2GEXT

Toggle State to false.
[  8182][I][RMakerDevice.cpp:162] updateAndReportParam(): Device : Switch, Param Name : Power, Val : false
Toggle State to true.
[  9835][I][RMakerDevice.cpp:162] updateAndReportParam(): Device : Switch, Param Name : Power, Val : true
Received value = false for Switch - Power
Received value = true for Switch - Power
Toggle State to false.
[ 29937][I][RMakerDevice.cpp:162] updateAndReportParam(): Device : Switch, Param Name : Power, Val : false
```

### Resetting the device
- Press and Hold the Boot button for more than 3 seconds and then release to reset Wi-Fi configuration.
- Press and Hold the Boot button for more than 10 seconds and then release to reset to factory defaults.
