# sensor_server - Thread Leader + CoAP Telemetry Server

Server side of the [CoAP Sensor demo](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor/README.md). This sketch:

* forms a Thread network `ESP_OT_CoAP_Sensor` with a hard-coded `DataSet`,
* becomes the **Leader** and serves GET `sensor/temp` via `OThreadCoAPServer.serve()`,
* updates the reading every second with `setResourceValue()` (thread-safe),
* simulates temperature between 20.00 and 27.99 °C using `random()`.

It is the counterpart of [CoAP Sensor — sensor_client (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor/sensor_client).
Flash this sketch **before** the client.

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

Flash this sketch **before** [CoAP Sensor — sensor_client (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor/sensor_client). The client
only needs the same `NETKEY`; channel, PAN ID, and Extended PAN ID are defined
here on the Leader.

The client polls the Leader RLOC every 10 s — wait for
`Ready. Serving GET sensor/temp` before flashing the client.

## What the sketch does

```cpp
// 1) Form the Thread network and wait for attach.
DataSet ds;
ds.initNew();
ds.setNetworkName("ESP_OT_CoAP_Sensor");
ds.setChannel(CHANNEL);
ds.setPanId(PAN_ID);
ds.setExtendedPanId(EXTPANID);
ds.setNetworkKey(NETKEY);
OThread.commitDataSet(ds);
OThread.networkInterfaceUp();
OThread.start();

// 2) Bind a read-only resource and start CoAP.
OThreadCoAPServer.serve("sensor/temp", &temperature);
OThreadCoAPServer.begin();

// 3) In loop(): publish new readings safely.
OThreadCoAPServer.setResourceValue("sensor/temp", reading);
```

## Expected serial output

```text
=== CoAP Sensor — server ===
Forming Thread network...
Waiting for attach..
Attached as Leader.
Leader RLOC: fdde:ad00:beef:0:0:ff:fe00:0
Ready. Serving GET sensor/temp
```

If attach fails:

```text
=== CoAP Sensor — server ===
Forming Thread network...
Waiting for attach.........
Attach timeout.
Attach failed.
```

## Customization

All tunables are at the top of the `.ino` file:

| Constant   | Purpose                                                      |
| ---------- | ------------------------------------------------------------ |
| `CHANNEL`  | 802.15.4 channel (default 15).                               |
| `PAN_ID`   | 16-bit PAN ID (default `0xBEEF`).                            |
| `EXTPANID` | 64-bit Extended PAN ID — fixed; must not change between runs. |
| `NETKEY`   | 128-bit network key — must match the client.                 |

CoAP listens on port **5683**. Update the bound `temperature` String only through
`setResourceValue()` after `begin()` — do not assign the backing String directly
from `loop()` (race with the OpenThread task).

## Troubleshooting

**Startup order:** Flash this **sensor_server (Leader)** first. Wait for
`Ready. Serving GET sensor/temp`, then flash [CoAP Sensor — sensor_client (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor/sensor_client).

| Symptom | Likely cause |
| --- | --- |
| `Attach failed` | RF issue or stale NVS — erase flash and retry. |
| `CoAP server failed` | OpenThread not attached. |
| Client GET times out | Server not running, or client formed its own network (check for `Started as Leader` on client). |
| Stale readings | Server `loop()` not calling `setResourceValue()` each second. |

## See also

* [CoAP Sensor — group overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor/README.md) — telemetry resource and polling pattern.
* [CoAP Sensor — sensor_client (client)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor/sensor_client) — NON GET polling client.
* [UDP Sensor Network](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork) — UDP telemetry with sequence ACKs.

## License

Apache License 2.0.
