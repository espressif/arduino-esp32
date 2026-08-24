# web - Wi-Fi UI + LAN mDNS + UDP proxy

Wi-Fi-only side of the [ThreadDNSSD UDP Light](../README.md) lab. This sketch:

* joins Wi-Fi on the **same LAN** as the OTBR infrastructure interface,
* advertises **http://otlight-ui.local** via ESPmDNS,
* discovers `_otlight._udp` via LAN mDNS (OTBR **Advertising Proxy**),
* proxies browser On / Off / Toggle / Status to UDP on the light.

This board does **not** run OpenThread. Thread attach + SRP are handled by
[light](../light/).

## Supported Targets

Any Arduino-ESP32 board with Wi-Fi (or hosted Wi-Fi). Thread radio is not used.

IPv6 setup follows [WiFiIPv6](../../../../../WiFi/examples/WiFiIPv6/WiFiIPv6.ino):
`WiFi.enableIPv6()` **before** `WiFi.begin()`, then wait for
`ARDUINO_EVENT_WIFI_STA_GOT_IP6`.

## Required IDF features (sdkconfig)

| Feature | Why |
| --- | --- |
| WiFi (`CONFIG_SOC_WIFI_SUPPORTED` or `CONFIG_ESP_HOSTED_ENABLED`) | STA + HTTP |
| `CONFIG_LWIP_IPV6` | Talk to Thread OMR addresses |

CI uses `requires_any` for Wi-Fi (same pattern as ESPmDNS examples).

## Prerequisites

1. [light](../light/) announced on the OTBR (SRP).
2. OTBR **Advertising Proxy** (or equivalent) so `_otlight._udp` appears on LAN mDNS.
3. Set `WIFI_SSID` / `WIFI_PASS` in `web.ino`.
4. Optional: set `LIGHT_IPV6_FALLBACK` to **your** light OMR if AAAA never
   appears (see below).

## How to get the light IPv6 (OMR) address

The light’s LAN-reachable address is its Thread **OMR** (usually a ULA
`fdxx:…`). It is **unique per lab / device** and can change after SRP
re-registration or OMR prefix changes. Do **not** copy an address from another
user’s log.

| Source | How |
| ------ | --- |
| [switch](../switch/) Serial | After discover: `Light: ... [fd..]:5051` — use the address inside `[…]` |
| OTBR CLI | `srp server host` / `srp server service` — host `ot-light` IPv6 under the OMR prefix from `netdata show` |
| Light board | OpenThread `ipaddr` — pick the address on the OMR prefix (not `fe80::` only) |

Optional check from a PC on the same Wi-Fi: `ping <the-omr-ipv6>`.

## What the sketch does

```cpp
WiFi.enableIPv6();
WiFi.begin(WIFI_SSID, WIFI_PASS);
MDNS.begin("otlight-ui");
Udp.begin(IPAddress(IPv6), 0);   // AF_INET6 required for OMR destinations
// discover: MDNS.queryService("otlight","udp") + mdns_query_aaaa / fallback
// HTTP /api/on|off|toggle|status -> UDP ON|OFF|TOGGLE|STATUS
```

## Expected serial output

```text
ThreadDNSSD_UDP_Light / web
WiFi IPv4: 192.168.x.x
WiFi IPv6 LL: fe80::...
Open http://otlight-ui.local
UDP socket: AF_INET6 (required for Thread OMR)
MDNS.queryService("otlight", "udp")...
Found 1 mDNS instance(s)
  [0] ot-light host=ot-light v4=0.0.0.0 v6=:: port=5051
mdns_query_aaaa(ot-light) err=0
  -> fd..
Using light [fd..]:5051
HTTP server on port 80
UDP RX 'STATE ON'
```

Empty `v4`/`v6` after browse is common; the sketch then queries AAAA or uses
`LIGHT_IPV6_FALLBACK`.

## Customization

| Constant               | Purpose                                      |
| ---------------------- | -------------------------------------------- |
| `WIFI_SSID` / `WIFI_PASS` | Same LAN as OTBR                           |
| `LIGHT_IPV6_FALLBACK`  | Your light OMR if mDNS AAAA missing          |
| `LIGHT_PORT_FALLBACK`  | Default 5051                                 |
| `UDP_TIMEOUT_MS`       | Wait for light reply                         |
| `REDISCOVER_MS`        | Re-browse when target unknown                |

## Troubleshooting

| Symptom | Likely cause |
| ------- | ------------ |
| `FAIL: WiFi connect` / `FAIL: MDNS.begin` / `FAIL: WiFiUDP begin` | Fatal setup — sketch **halts** (`while (true)`); `loop()` does not serve HTTP |
| `FAIL: CONFIG_LWIP_IPV6 is off` | Rebuild with IPv6 (needed for Thread OMR) |
| `Found 0` | Adv Proxy off; not on OTBR Wi-Fi; flaky mDNS |
| `err=261` / no AAAA | Adv Proxy without address records — set `LIGHT_IPV6_FALLBACK` |
| `endPacket failed` | IPv4-only UDP socket — use latest sketch (`Udp.begin(IPAddress(IPv6), 0)`) |
| `UDP timeout`, PC can `ping` OMR | Arduino lwIP often has **RIO off** (`CONFIG_LWIP_IPV6_ND6_ROUTE_INFO_OPTION_SUPPORT`) — a PC may reach the light while this Wi‑Fi board cannot |
| NAT64 | Not required for native OMR IPv6 |

## See also

* [ThreadDNSSD UDP Light — group overview](../README.md)
* [light](../light/) — Thread lamp server
* [switch](../switch/) — Thread DNSSD client

## License

Apache License 2.0.
