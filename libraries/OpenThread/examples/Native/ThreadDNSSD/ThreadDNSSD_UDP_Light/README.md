# ThreadDNSSD UDP Light — Native API

End-to-end lab built with `OThreadDNSSD` + `OThreadUDP` (and a WiFi-only web
board). A Thread **light** advertises `_otlight._udp` via SRP, a Thread
**switch** discovers it on-mesh, and an optional **WiFi** UI reaches the same
light through the OTBR (Advertising Proxy + OMR routing).

| Sketch | Role |
| ------ | ---- |
| [light](light/) | Thread node: SRP advertise + UDP lamp server (port **5051**) |
| [switch](switch/) | Thread node: `queryService` + BOOT → `TOGGLE` / `STATUS` |
| [web](web/) | WiFi-only: `http://otlight-ui.local` → LAN mDNS + UDP to light |

```text
[Thread switch] --OThreadDNSSD+UDP--> [Thread light] --SRP--> [OTBR]
                                                          | Adv Proxy mDNS
[Browser] --HTTP--> [WiFi web] --ESPmDNS+UDP-------------+
```

Unlike [UDP Light Switch](../../UDP/UDP_Light_Switch/), these boards **join an
existing OTBR network** with the **Network Key only** (no on-sketch
Commissioner / Joiner). One light can serve the switch and web clients.

Port **5051** is intentional. Avoid **5683** / **5684** (CoAP) and **61631**
(Thread TMF CoAP).

## Wire protocol

| Request | Reply |
| ------- | ----- |
| `ON` / `OFF` / `TOGGLE` | `ACK ON` / `ACK OFF` |
| `STATUS` | `STATE ON` / `STATE OFF` |

DNS-SD service type: `_otlight._udp` (instance / host `ot-light` by default).

## How to Run

1. Bring up an **OTBR** on the Thread network with **SRP** (and **DNS** for the
   switch). For the web UI, also enable **Advertising Proxy** (or equivalent)
   and keep the WiFi board on the **same LAN** as the OTBR infrastructure
   interface.
2. Set `OT_NETKEY` in [light](light/) and [switch](switch/) to the OTBR Network
   Key. Prefer **Tools → Erase Flash: Sketch Only** so the SRP key in NVS is kept.
3. Flash [light](light/) on a Thread SoC (ESP32-H2 / C6 / C5). Wait for
   `PASS: announced` and `UDP listening`.
4. Flash [switch](switch/) on a second Thread board. Press **BOOT** to toggle.
5. Optional: set WiFi credentials in [web](web/), flash a WiFi SoC, open
   `http://otlight-ui.local`.

Each folder has its own README with configuration, expected output, and
troubleshooting.

## Required IDF features (sdkconfig)

| Feature | Used by |
| --- | --- |
| `CONFIG_OPENTHREAD_ENABLED=y` | `light`, `switch` |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y` | `light`, `switch` |
| `CONFIG_OPENTHREAD_SRP_CLIENT=y` | `light`, `switch` |
| `CONFIG_OPENTHREAD_DNS_CLIENT=y` | `switch` |
| WiFi (`CONFIG_SOC_WIFI_SUPPORTED` or hosted) | `web` |

## Troubleshooting

**Startup order:** OTBR first, then **light** (wait for SRP announce), then
**switch** / **web**.

| Symptom | Likely cause |
| --- | --- |
| Light `not attached` / `FAIL: OThreadDNSSD.begin` | Wrong `OT_NETKEY` vs OTBR, or local DNSSD setup (sketches **halt**) |
| Light `announce timeout` | No SRP server in Network Data (light **halts**; UDP lamp is not started) |
| `OT_ERROR_DUPLICATED` | SRP name held by another key — unique hostname, Sketch Only erase, or clear OTBR SRP soft state |
| Switch finds 0 instances | Light not announced; no DNS on OTBR; wrong network |
| Web `FAIL: WiFi connect` / `FAIL: MDNS.begin` / `FAIL: WiFiUDP begin` | Fatal setup — sketch **halts** so HTTP is not served without a socket |
| Web `Found 0` / empty AAAA | Adv Proxy off or flaky LAN mDNS — set `LIGHT_IPV6_FALLBACK` (see [web/README](web/README.md)) |
| Web `UDP timeout`, PC can ping OMR | ESP Wi‑Fi stack often lacks ND6 **RIO** — see [web/README](web/README.md) |

## See Also

* [ThreadDNSSD examples](../) — advertise / query API demos and flash-erase notes
* [UDP Light Switch](../../UDP/UDP_Light_Switch/) — multicast UDP lamp without DNSSD / OTBR
* [CoAP Light Switch](../../CoAP/CoAP_Light_Switch/) — same lamp pattern over CoAP

## License

Apache License 2.0.
