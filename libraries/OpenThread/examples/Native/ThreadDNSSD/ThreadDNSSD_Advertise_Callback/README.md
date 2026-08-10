# ThreadDNSSD_Advertise_Callback

Event-driven advertise with **re-advertise recovery** when registration is lost
(`OT_DNSSD_EVENT_ERROR`, lost Thread attach, or announce flag cleared — e.g.
after an OTBR restart).

For a minimal blocking example, see [ThreadDNSSD_Advertise](../ThreadDNSSD_Advertise/).

## Behavior

1. Attach to Thread, register `sensor-cb` / `_ot._udp` on port 12346.
2. Completion via `onServiceEvent` (`OT_DNSSD_EVENT_ANNOUNCED`).
3. Keep watching and schedule recovery from `loop()` (not from the SRP callback):
   - non-conflict `OT_DNSSD_EVENT_ERROR`
   - lost attach → re-advertise when attached again
   - `isAnnounceComplete()` (live OT `Registered` state) goes false after success
4. Recovery: `end()` + `begin()` + `addService()`, with a 15 s cooldown.
5. `OT_ERROR_DUPLICATED` / `OT_ERROR_SECURITY` are reported and **not** auto-retried with the same hostname.

## Expected Serial output

```
ThreadDNSSD_Advertise_Callback
Waiting to attach...
Attached as Child
Advertise (initial) as sensor-cb...
Waiting for OT_DNSSD_EVENT_ANNOUNCED...
PASS: ANNOUNCED as sensor-cb
```

After an OTBR restart you may see `EVENT: ERROR`, then later `Advertise (recovery)...` and another `PASS`.

## How to test

1. Same OTBR + Network Key as Advertise; prefer **Erase Flash: Sketch Only**.
2. Pass = `PASS: ANNOUNCED`.
3. Restart OTBR; within tens of seconds the sketch should recover; confirm with `srp server service`.

Do not call other `OThreadDNSSD` APIs from inside the callback (flags only).

## Troubleshooting

| Symptom | Likely cause |
|---------|----------------|
| `not attached` | Wrong Network Key vs OTBR |
| No ANNOUNCED | No SRP server in Network Data |
| Name conflict | Unique hostname, keep NVS, or clear OTBR soft state |
| Rapid ERROR then recovery spam | Cooldown is 15 s; wait for OTBR SRP to come back |
