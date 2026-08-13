# ThreadDNSSD Native examples

Arduino-style Thread DNS-SD using `OThreadDNSSD` (SRP advertise + DNS discover).

## Lab setup

1. A **Thread Border Router** (OTBR) with **SRP** (and **DNS** for Query examples)
   on the same Thread network. Without an SRP server in Network Data,
   `waitForAnnounce()` times out. Without DNS, `queryService` / `queryHost` fail.
2. An ESP32-C5 / C6 / H2 board with IEEE 802.15.4.
3. Set `OT_NETKEY` in the sketch to the OTBR **Network Key** only (channel, PAN
   ID, and other dataset fields are obtained when the node attaches).

## sdkconfig (Arduino defaults)

| Capability | Device Kconfig | Default | Network |
|------------|----------------|---------|---------|
| Advertise | `CONFIG_OPENTHREAD_SRP_CLIENT` | On | OTBR SRP |
| Discover | `CONFIG_OPENTHREAD_DNS_CLIENT` | On | OTBR DNS/SRP |
| On-device SRP server | `CONFIG_OPENTHREAD_BORDER_ROUTER` | Off | Custom BR build |

Each example has a `ci.yml` that requires the matching Kconfig options so CI
skips builds when those options are not enabled for a target.

## Sketches

| Sketch | What it tests |
|--------|----------------|
| [ThreadDNSSD_Advertise](ThreadDNSSD_Advertise/) | Simple blocking `waitForAnnounce` |
| [ThreadDNSSD_Advertise_Callback](ThreadDNSSD_Advertise_Callback/) | `onServiceEvent` + re-advertise on error / lost attach / OTBR restart |
| [ThreadDNSSD_Remove](ThreadDNSSD_Remove/) | Two add/remove cycles, then `end()` (full unregister demo) |
| [ThreadDNSSD_Query](ThreadDNSSD_Query/) | `queryService` browse (pair with Advertise) |
| [ThreadDNSSD_QueryHost](ThreadDNSSD_QueryHost/) | `queryHost` resolve (pair with Advertise) |
| [ThreadDNSSD_Query_Callback](ThreadDNSSD_Query_Callback/) | `onQueryEvent` + `startQueryService` / `startQueryHost` (both in one sketch) |
| [ThreadDNSSD_UDP_Light](ThreadDNSSD_UDP_Light/) | App lab: Thread light + switch + WiFi web (SRP + UDP + OTBR Adv Proxy mDNS) |

API demos (Advertise / Query / …) each have a short README. The UDP Light lab has
a group README plus per-role folders (`light`, `switch`, `web`).

### ThreadDNSSD_UDP_Light (app lab)

| Piece | Role |
| ----- | ---- |
| [light](ThreadDNSSD_UDP_Light/light/) | Thread: SRP `_otlight._udp` + UDP lamp |
| [switch](ThreadDNSSD_UDP_Light/switch/) | Thread: `queryService` + BOOT control |
| [web](ThreadDNSSD_UDP_Light/web/) | ESP WiFi: `otlight-ui.local` → LAN mDNS + UDP |

## Check registrations on the OTBR

On the OTBR OpenThread CLI (or `ot-ctl`):

```text
srp server service
srp server host
```

You should see instance names such as `sensor-1._ot._udp.default.service.arpa.`
(or `ot-light._otlight._udp…` for the UDP Light lab) with `deleted: false`,
port, host, and addresses. Stock OTBR CLI can **list** these entries; it does
**not** delete another device’s registration.

## Flash erase, SoC reset, OTBR reset

| Situation | Effect |
|-----------|--------|
| Arduino IDE **Tools → Erase Flash: "Sketch Only"** | Keeps NVS (SRP ECDSA key). Same hostname usually re-registers OK. |
| **"All Flash Contents"** (or any erase that clears NVS) | New SRP key. OTBR may still hold the old name → `OT_ERROR_DUPLICATED` until key-lease expires, you use a new hostname, or the OTBR SRP DB is cleared. |
| **SoC reset** (button / power cycle, no full erase) | NVS kept; same key; refresh/re-register with the same name is OK. |
| **OTBR / `otbr-agent` restart** | Typical OTBR drops in-memory SRP entries (`srp server service` empty). Sketch must advertise again (`ThreadDNSSD_Advertise_Callback` does this). After a prior `DUPLICATED`, poll `isAnnounceComplete()` if OpenThread retries once the name is free. Discover boards see empty until re-advertise. |
| **Second board with the same hostname** | Name conflict (`OT_ERROR_DUPLICATED`). Use unique names (e.g. include MAC). |
| **`end()` / Remove example** on the device that registered | Proper client-side unregister; confirm with `srp server service`. |

Prefer unique hostnames when multiple nodes share one OTBR. The library reports
conflicts; it does not rename for you.

## Discover results

Copy browse/resolve results when `queryService` / `queryHost` returns, or on
`OT_DNSSD_QUERY_DONE` for async APIs. After a **timeout**, `end()`, or before the
next query, do not treat getters (`queryResultCount`, `instanceName`,
`resolvedAddress`, …) as stable — start a new query for updated data. Use
`lastError()` to tell timeout (`OT_ERROR_RESPONSE_TIMEOUT`) from an empty
successful browse (`OT_ERROR_NONE` with count 0).

## Pass / fail

- **Pass:** Serial prints that the service was announced (`ANNOUNCED` / `announced OK`), or Query finds instances / resolves an address.
- **Fail / timeout:** not attached (wrong Network Key), or no SRP/DNS server on the network.
- **Name conflict:** `OT_ERROR_DUPLICATED` — change hostname, keep NVS on reflash, or clear OTBR soft state (restart OTBR) / wait for key-lease.
- After a conflict fail, if you clear the OTBR and OpenThread retries successfully, `isAnnounceComplete()` (live local `Registered` state) can become true without resetting the board — poll it from `loop()`.

## See also

* [Native examples overview](../README.md)
* [ThreadDNSSD_UDP_Light](ThreadDNSSD_UDP_Light/) — light / switch / web lab
* [UDP Light Switch](../UDP/UDP_Light_Switch/) — multicast UDP lamp without DNSSD / OTBR

## License

Apache License 2.0.
