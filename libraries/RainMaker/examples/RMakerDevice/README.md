# Switch Example

## Compile n Flash firmware

### ESP32 Board
- Assisted Claiming + BLE Provisioning

## What to expect in this sketch ?
### It demonstartes the toggling of power of devices using phone app and BOOT button.

- Switch and Fan are the dummy device. Switch connected at gpio(16) and Fan connected at gpio(17).
- Toggling the power button for any device(switch/fan) on phone app will change the status(on/off) for the device connected at gpio(16/17) and also update the status to the ESP RainMaker cloud if `param.updateAndReport(val)` API is called.
- Pressing the BOOT button toggles the status(on/off) of both device(switch & fan) connected at gpio(16/17) and report values to the cloud.

### Output

```
[I][WiFiProv.cpp:179] beginProvision(): Already Provisioned
[I][WiFiProv.cpp:183] beginProvision(): Attempting connect to AP: Wce*****

Received value = false for Switch - Power

Received value = false for Fan - Power

Received value = true for Switch - Power

[I][RMakerDevice.cpp:161] updateAndReportParam(): Device : Switch, Param Name : Power, Val : false

[I][RMakerDevice.cpp:161] updateAndReportParam(): Device : Fan, Param Name : Power, Val : true

Received value = true for Switch - Power

Received value = false for Fan - Power
```
