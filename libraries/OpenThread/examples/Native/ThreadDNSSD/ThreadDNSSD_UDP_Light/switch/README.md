# switch - Thread DNSSD client + UDP control

Client side of the [ThreadDNSSD UDP Light](../README.md) lab. This sketch:

* joins the OTBR network with **Network Key only**,
* browses `_otlight._udp` with `OThreadDNSSD.queryService`,
* on every **BOOT** press, sends `TOGGLE` then `STATUS` unicast to the resolved
  light address/port and prints the ACK / STATE reply.

It is the counterpart of [light](../light/).

## Supported Targets

| SoC      | Thread | BOOT Button | Status    |
| -------- | ------ | ----------- | --------- |
| ESP32-H2 | yes    | `BOOT_PIN`  | Supported |
| ESP32-C6 | yes    | `BOOT_PIN`  | Supported |
| ESP32-C5 | yes    | `BOOT_PIN`  | Supported |

Override `USER_BUTTON` at the top of the sketch if your board uses another GPIO.

## Required IDF features (sdkconfig)

| Feature                             | Why                                   |
| ----------------------------------- | ------------------------------------- |
| `CONFIG_OPENTHREAD_ENABLED=y`       | OpenThread stack                      |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y` | 802.15.4 radio                        |
| `CONFIG_OPENTHREAD_SRP_CLIENT=y`    | Stack / DNSSD client deps             |
| `CONFIG_OPENTHREAD_DNS_CLIENT=y`    | `queryService` / DNS browse           |

## Prerequisites

[light](../light/) must already be attached and **SRP-announced** on the same
Thread network. Set the same `OT_NETKEY` as the light / OTBR.

## What the sketch does

```cpp
// Join with Network Key (same pattern as light).
OThreadDNSSD.begin(...);  // optional host name for local identity
// discoverLight(): queryService("otlight", "udp") -> address + port

// On BOOT:
OtUdp.beginPacket(s_lightAddr, s_lightPort);
OtUdp.write("TOGGLE"); OtUdp.endPacket();
// wait for ACK, then STATUS -> STATE
```

The resolved OMR in Serial (`Light: ... [fd..]:5051`) can be pasted into
[web](../web/) as `LIGHT_IPV6_FALLBACK` when LAN mDNS has no AAAA — see
[web/README](../web/README.md#how-to-get-the-light-ipv6-omr-address).

## Expected serial output

```text
ThreadDNSSD_UDP_Light / switch
Waiting to attach...
Attached as Child
queryService("otlight", "udp")...
Found 1 instance(s) (lastError=0)
Light: instance=ot-light host=ot-light [fd..:..]:5051
Press BOOT to TOGGLE + STATUS

TX TOGGLE -> ACK ON
TX STATUS -> STATE ON
```

## Customization

| Constant              | Purpose                                      |
| --------------------- | -------------------------------------------- |
| `OT_NETKEY`           | Must match light / OTBR                      |
| `LIGHT_PORT_FALLBACK` | Used if browse returns port 0 (default 5051) |
| `ACK_TIMEOUT_MS`      | Wait for UDP reply                           |
| `REDISCOVER_MS`       | Re-browse interval when light is unknown     |
| `USER_BUTTON`         | BOOT / user button GPIO                      |

## Troubleshooting

| Symptom | Likely cause |
| ------- | ------------ |
| `Found 0 instance(s)` | Light not announced; no DNS on OTBR; wrong Network Key |
| BOOT: no ACK | Wrong address/port, light down, or RF range |
| Discovers then loses light | Re-browse on interval; check light still `announce=1` |

## See also

* [ThreadDNSSD UDP Light — group overview](../README.md)
* [light](../light/) — SRP advertise + UDP lamp
* [ThreadDNSSD_Query](../../ThreadDNSSD_Query/) — browse-only API demo

## License

Apache License 2.0.
