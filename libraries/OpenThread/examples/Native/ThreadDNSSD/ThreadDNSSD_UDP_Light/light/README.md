# light - Thread SRP advertise + UDP lamp server

Server side of the [ThreadDNSSD UDP Light](../README.md) lab. This sketch:

* joins an existing OTBR Thread network using **Network Key only**,
* advertises `_otlight._udp` on port **5051** via `OThreadDNSSD` (SRP),
* serves unicast UDP `ON` / `OFF` / `TOGGLE` / `STATUS` and drives the on-board
  RGB LED.

It is the counterpart of [switch](../switch/) and [web](../web/).

## Supported Targets

| SoC      | Thread | RGB LED  | Status    |
| -------- | ------ | -------- | --------- |
| ESP32-H2 | yes    | Required | Supported |
| ESP32-C6 | yes    | Required | Supported |
| ESP32-C5 | yes    | Required | Supported |

## Required IDF features (sdkconfig)

| Feature                             | Why                                    |
| ----------------------------------- | -------------------------------------- |
| `CONFIG_OPENTHREAD_ENABLED=y`       | OpenThread stack                       |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y` | 802.15.4 radio                         |
| `CONFIG_OPENTHREAD_SRP_CLIENT=y`    | `OThreadDNSSD` advertise (SRP client)  |

## Prerequisites

An **OTBR** with **SRP** on the same Thread network. Set `OT_NETKEY` to the OTBR
Network Key before flashing.

Prefer **Tools → Erase Flash: Sketch Only** so the SRP ECDSA key in NVS is kept
(avoids `OT_ERROR_DUPLICATED` after reflash).

## What the sketch does

```cpp
OThread.begin(false);
DataSet ds; ds.clear(); ds.setNetworkKey(OT_NETKEY);
OThread.commitDataSet(ds);
OThread.networkInterfaceUp();
OThread.start();
// wait for Child / Router / Leader...

OThreadDNSSD.begin("ot-light");
OThreadDNSSD.addService("otlight", "udp", LIGHT_PORT);
OThreadDNSSD.waitForAnnounce(60000);

OtUdp.begin(LIGHT_PORT);
// loop: parsePacket -> ON/OFF/TOGGLE/STATUS -> unicast ACK/STATE
```

## Wire protocol

| Incoming | Action            | Reply              |
| -------- | ----------------- | ------------------ |
| `ON`     | lamp on           | `ACK ON`           |
| `OFF`    | lamp off          | `ACK OFF`          |
| `TOGGLE` | invert lamp       | `ACK ON` / `ACK OFF` |
| `STATUS` | no change         | `STATE ON` / `STATE OFF` |

Replies are **unicast** to `remoteIP()` / `remotePort()`.

## Expected serial output

```text
ThreadDNSSD_UDP_Light / light
Waiting to attach...
Attached as Child
Waiting for SRP announce...
PASS: announced as ot-light _otlight._udp:5051
UDP listening on port 5051 (MLEID fd..)
RX [fd..]:xxxxx <- 'TOGGLE'
role=Child announce=1 lamp=ON
```

## Customization

| Constant     | Purpose                                      |
| ------------ | -------------------------------------------- |
| `OT_NETKEY`  | Must match the OTBR Network Key              |
| `kHostName`  | SRP hostname (default `ot-light`)            |
| `LIGHT_PORT` | UDP listen port (default 5051)               |

If you change `kHostName` or `LIGHT_PORT`, mirror the service type / port on
[switch](../switch/) and any web/PC clients.

## Troubleshooting

| Symptom | Likely cause |
| ------- | ------------ |
| `FAIL: not attached` | Wrong Network Key vs OTBR |
| `FAIL: announce` / timeout | No SRP server; wait longer; check `srp server service` on OTBR |
| `OT_ERROR_DUPLICATED` | Name conflict — unique hostname, Sketch Only erase, or clear OTBR SRP |
| No `RX` lines when switch presses BOOT | Switch not discovering this host, or different Thread network |

## See also

* [ThreadDNSSD UDP Light — group overview](../README.md)
* [switch](../switch/) — Thread DNSSD client + BOOT
* [ThreadDNSSD category](../../) — flash erase / SRP name conflicts

## License

Apache License 2.0.
