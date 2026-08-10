# ThreadDNSSD_Advertise

Simple advertise path: `begin` + `addService` + blocking `waitForAnnounce`.

For event-driven watching and re-advertise after OTBR restart, see
[ThreadDNSSD_Advertise_Callback](../ThreadDNSSD_Advertise_Callback/).

## Hardware / lab

- ESP32-C5 / C6 / H2
- OpenThread Border Router with **SRP enabled** on the same Thread network
- Set `OT_NETKEY` in the `.ino` to the OTBR Network Key (no other dataset fields required)

## Expected Serial output

```
ThreadDNSSD_Advertise
Waiting to attach...
Attached as Child
Waiting for SRP announce (need OTBR SRP server)...
PASS: announced OK as sensor-1
```

## How to test

1. Confirm OTBR SRP is running; flash with `OT_NETKEY` set.
2. Prefer **Tools → Erase Flash: "Sketch Only"** so the SRP key in NVS is kept.
3. Pass = `PASS: announced OK`.
4. Optional: OTBR CLI `srp server service` — look for `sensor-1._ot._udp...`.

See the [category README](../README.md) for flash-erase vs SoC reset vs OTBR reset, and name conflicts.

## Troubleshooting

| Symptom | Likely cause |
|---------|----------------|
| `not attached` | Wrong Network Key vs OTBR |
| `announce timeout` | No SRP server in Network Data |
| `OT_ERROR_DUPLICATED` | Name held by another SRP key — unique hostname, Sketch Only erase, or restart OTBR / wait for key-lease |
| `announceComplete` later becomes `1` after a fail | OpenThread retried and reached `Registered` (e.g. OTBR was cleared). `isAnnounceComplete()` reads live SRP client state. |
