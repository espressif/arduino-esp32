# ESP RainMaker 8 Switches using Array

This example demonstrates how to build a device with 8 light switches to be used with ESP RainMaker.


## What to expect in this example?

- This virtual switches will work in parallel with physical two state light switch.
- The physical switch will change the state of relay while is pressed or when released and the same will reflect on the phone app.
- After compiling and flashing the example, add your device using the [ESP RainMaker phone apps](https://rainmaker.espressif.com/docs/quick-links.html#phone-apps) by scanning the QR code.

### Libraries dependencies
 - [AceButton](https://github.com/bxparks/AceButton)

### Output

```
[    80][I][RMaker.cpp:17] event_handler(): RainMaker Initialised.
[    96][I][WiFiProv.cpp:149] beginProvision(): Already Provisioned
[    96][I][WiFiProv.cpp:153] beginProvision(): Attempting connect to AP: XXXXXX

[   199][I][RMakerDevice.cpp:163] updateAndReportParam(): Device : Relay1, Param Name : Power, Val : false
[   199][I][RMakerDevice.cpp:163] updateAndReportParam(): Device : Relay2, Param Name : Power, Val : false
[   207][I][RMakerDevice.cpp:163] updateAndReportParam(): Device : Relay3, Param Name : Power, Val : false
[   223][I][RMakerDevice.cpp:163] updateAndReportParam(): Device : Relay4, Param Name : Power, Val : false
[   232][I][RMakerDevice.cpp:163] updateAndReportParam(): Device : Relay5, Param Name : Power, Val : false
[   241][I][RMakerDevice.cpp:163] updateAndReportParam(): Device : Relay6, Param Name : Power, Val : false
[   251][I][RMakerDevice.cpp:163] updateAndReportParam(): Device : Relay7, Param Name : Power, Val : false
[   260][I][RMakerDevice.cpp:163] updateAndReportParam(): Device : Relay8, Param Name : Power, Val : false
Light switch 1 released, relayState: 1
[359585][I][RMakerDevice.cpp:163] updateAndReportParam(): Device : Relay1, Param Name : Power, Val : true
Light switch 1 pressed, relayState: 0
[363695][I][RMakerDevice.cpp:163] updateAndReportParam(): Device : Relay1, Param Name : Power, Val : false
[390384][I][RMakerDevice.cpp:163] updateAndReportParam(): Device : Relay1, Param Name : Power, Val : true
[392021][I][RMakerDevice.cpp:163] updateAndReportParam(): Device : Relay1, Param Name : Power, Val : false
```

### Factory reset
- Press and Hold the Boot button for more than 10 seconds and then release to reset to factory defaults.
