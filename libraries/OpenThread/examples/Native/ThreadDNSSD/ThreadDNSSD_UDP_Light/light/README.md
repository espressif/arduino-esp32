# light - Thread SRP advertise + UDP lamp server

Server side of the [ThreadDNSSD UDP Light](../README.md) lab. This sketch:

* joins an existing OTBR Thread network using **Network Key only**,
* advertises `_otlight._udp` on port **5051** via `OThreadDNSSD` (SRP),
* serves unicast UDP `ON` / `OFF` / `TOGGLE` / `STATUS` and drives the on-board
  RGB LED,
* re-advertises from `loop()` after OTBR restart, lost attach, or lost
  announce (same pattern as
  [ThreadDNSSD_Advertise_Callback](../../ThreadDNSSD_Advertise_Callback/)).
  UDP stays bound across SRP `end()` / `begin()`.

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

OtUdp.begin(LIGHT_PORT);
OThreadDNSSD.onServiceEvent(onDnsEvent);
startAdvertise("initial");  // end + begin + addService; ANNOUNCED via callback
// loop: parsePacket -> ON/OFF/TOGGLE/STATUS; re-advertise on ERROR / lost attach / OTBR restart
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
UDP listening on port 5051 (MLEID fd..)
Advertise (initial) as ot-light...
Waiting for OT_DNSSD_EVENT_ANNOUNCED...
PASS: ANNOUNCED as ot-light _otlight._udp:5051
RX [fd..]:xxxxx <- 'TOGGLE'
role=Child announce=1 needReadvertise=0 lamp=ON
```

UDP starts before SRP completes. After an OTBR restart you may see `EVENT: ERROR`
or `Announce incomplete`, then `Advertise (recovery)...` and another `PASS`.
Name conflicts (`OT_ERROR_DUPLICATED`) are reported and not auto-retried.

## Customization

| Constant     | Purpose                                      |
| ------------ | -------------------------------------------- |
| `OT_NETKEY`  | Must match the OTBR Network Key              |
| `kHostName`  | SRP hostname (default `ot-light`)            |
| `LIGHT_PORT` | UDP listen port (default 5051)               |
| `kReadvertiseCooldownMs` | Wait between recovery advertise attempts (default 15 s) |

If you change `kHostName` or `LIGHT_PORT`, mirror the service type / port on
[switch](../switch/) and any web/PC clients.

## Troubleshooting

| Symptom | Likely cause |
| ------- | ------------ |
| `FAIL: not attached` | Wrong Network Key vs OTBR (sketch **halts**) |
| `FAIL: UDP begin` | Local UDP bind error (sketch **halts**) |
| `FAIL: OThreadDNSSD.begin` / `addService` | Retried from `loop()` after the 15 s cooldown (not a halt) |
| No `PASS: ANNOUNCED` | No SRP server yet — UDP still listens; wait or check `srp server service` |
| `OT_ERROR_DUPLICATED` | Name conflict — unique hostname, Sketch Only erase, or clear OTBR SRP (not auto-retried) |
| Switch finds 0 after OTBR restart | Wait for `Advertise (recovery)` / another `PASS` |

## See also

* [ThreadDNSSD UDP Light — group overview](../README.md)
* [switch](../switch/) — Thread DNSSD client + BOOT
* [ThreadDNSSD category](../../) — flash erase / SRP name conflicts

## License

Apache License 2.0.
