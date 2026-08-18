# ThreadDNSSD_QueryHost

Resolve a Thread DNS-SD host label with `OThreadDNSSD.queryHost()`.

## Lab

1. OTBR with SRP **and** DNS in Network Data.
2. Board A: [ThreadDNSSD_Advertise](../ThreadDNSSD_Advertise/) (`sensor-1`).
3. Board B: this sketch (`resolver`). Same `OT_NETKEY`. Change `kPeerHost` if Board A uses another name.
   `begin("resolver")` registers that SRP host — use a unique name if several
   query boards share the OTBR.

## Expected Serial (Board B)

```
ThreadDNSSD_QueryHost
Waiting to attach...
Attached as Child

queryHost("sensor-1")...
PASS: sensor-1 -> fd...
```

Prefer **Erase Flash: Sketch Only**. Re-runs every 10 s. If Board A is not
announced, expect `FAIL: no address (lastError=...)` — not a success with an
empty IPv6. After OTBR restart, wait for Board A to advertise again.
