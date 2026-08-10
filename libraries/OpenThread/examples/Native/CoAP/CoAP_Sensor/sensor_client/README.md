# sensor_client - Thread Child + CoAP Telemetry Client

Client side of the [CoAP Sensor demo](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor/README.md). This sketch:

* joins the Leader started by [CoAP Sensor — sensor_server (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor/sensor_server) using only
  the shared **network key** (RouterNode pattern — no `initNew()` on the client),
* resolves the **Leader RLOC** as its CoAP destination,
* polls GET `sensor/temp` every **10 seconds** using **non-confirmable** CoAP
  (`setConfirmable(false)`),
* prints the temperature string and response code on Serial.

It is the counterpart of [CoAP Sensor — sensor_server (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor/sensor_server).

## Supported Targets

| SoC      | Thread | Status    |
| -------- | ------ | --------- |
| ESP32-H2 | yes    | Supported |
| ESP32-C6 | yes    | Supported |
| ESP32-C5 | yes    | Supported |

## Required IDF features (sdkconfig)

| Feature                              | Why                                              |
| ------------------------------------ | ------------------------------------------------ |
| `CONFIG_OPENTHREAD_ENABLED=y`        | Build the OpenThread stack.                      |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y`  | Ensure the SoC has the 802.15.4 radio.           |

## Prerequisites

[CoAP Sensor — sensor_server (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor/sensor_server) must already be running as Leader with
`Ready. Serving GET sensor/temp` on the same RF range.

The client only needs the same `NETKEY` as the server. Flash the server first,
then this client. Reset the client if it booted before the server was Leader.

## What the sketch does

```cpp
// 1) Join using the network key only (channel/PAN/ExtPAN learned at attach).
DataSet ds;
ds.clear();
ds.setNetworkKey(NETKEY);
OThread.commitDataSet(ds);
OThread.networkInterfaceUp();
OThread.start();
serverIp = OThread.getLeaderRloc();

// 2) Configure NON GET client.
CoapClient.setTimeout(3000);
CoapClient.setConfirmable(false);

// 3) In loop(): poll every POLL_MS.
int code = CoapClient.GET(serverIp, "sensor/temp");
Serial.printf("Temperature: %s C\n", CoapClient.getString().c_str());
```

## Expected serial output

```text
=== CoAP Sensor — client ===
Joining Thread network...
Waiting for attach..
Attached as Child. Sensor server: fdde:ad00:beef:0:0:ff:fe00:0
Ready. Polling GET sensor/temp every 10 s
Temperature: 23.45 C (code 205)
Temperature: 24.12 C (code 205)
```

If the server is not running first:

```text
=== CoAP Sensor — client ===
Joining Thread network...
Waiting for attach..
Started as Leader — is sensor_server running first?
Attach timeout.
Attach failed.
```

## Customization

All tunables are at the top of the `.ino` file:

| Constant  | Purpose                                           |
| --------- | ------------------------------------------------- |
| `NETKEY`  | 128-bit network key — must match the server.      |
| `POLL_MS` | Poll interval in milliseconds (default 10000).    |

CoAP client timeout is set via `CoapClient.setTimeout(3000)` (ms).

## Troubleshooting

**Startup order:** Start [CoAP Sensor — sensor_server (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor/sensor_server) first. Wait for
`Ready. Serving GET sensor/temp`, then flash or reset this client.

| Symptom | Likely cause |
| --- | --- |
| `Attach failed` | Server not Leader yet, or wrong `NETKEY`. |
| `Started as Leader` | Server not running — client formed its own network. |
| `GET failed: timeout` | Client on a separate partition — start server first, erase client flash, retry. |
| No new readings | Server not updating via `setResourceValue()` in `loop()`. |

## See also

* [CoAP Sensor — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor/README.md) — telemetry resource and polling pattern.
* [CoAP Sensor — sensor_server (server)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor/sensor_server) — companion telemetry server.
* [UDP Sensor Network — sensor_node (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork/sensor_node) — UDP telemetry with ACKs.

## License

Apache License 2.0.
