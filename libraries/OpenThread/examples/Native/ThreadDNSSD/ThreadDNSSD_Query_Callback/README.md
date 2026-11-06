# ThreadDNSSD_Query_Callback

Async discover: `startQueryService` then `startQueryHost`, both via `onQueryEvent`.

Covers the same lab as [ThreadDNSSD_Query](../ThreadDNSSD_Query/) and
[ThreadDNSSD_QueryHost](../ThreadDNSSD_QueryHost/) without blocking `loop()` on DNS.

## Lab

1. OTBR with SRP **and** DNS in Network Data.
2. Board A: [ThreadDNSSD_Advertise](../ThreadDNSSD_Advertise/) (`sensor-1`).
3. Board B: this sketch (`browser-cb`). Same `OT_NETKEY`.
   `begin("browser-cb")` registers that SRP host — keep it unique if several
   query boards share the OTBR.
4. Prefer **Erase Flash: Sketch Only**.

## Expected Serial (Board B)

```
ThreadDNSSD_Query_Callback
Waiting to attach...
Attached as Child

startQueryService("ot", "udp")...
Service browse done: 1 instance(s) (event=0 error=0)
  [0] instance=sensor-1 host=sensor-1 port=12345 addr=fd...
       TXT path=/status

startQueryHost("sensor-1")...
Host resolve done: count=1 (event=0 error=0)
  sensor-1 -> fd...
```

Flags are set in the OpenThread-task callback; printing and the next
`startQuery*` run from `loop()`. Cycle repeats every 10 s.

`FAIL: not attached` / `FAIL: OThreadDNSSD.begin` halt the sketch (`while (true)`).
Returning from `setup()` would still run `loop()`.
