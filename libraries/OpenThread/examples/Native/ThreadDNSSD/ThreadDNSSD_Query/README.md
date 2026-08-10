# ThreadDNSSD_Query

Browse for `_ot._udp` service instances via `OThreadDNSSD.queryService()`.

## Lab

1. OTBR with SRP **and** DNS in Network Data (typical OTBR).
2. Board A: flash [ThreadDNSSD_Advertise](../ThreadDNSSD_Advertise/) (`sensor-1`).
3. Board B: this sketch (`browser`). Same `OT_NETKEY` on both.
4. Prefer **Erase Flash: Sketch Only**.

## Expected Serial (Board B)

```
ThreadDNSSD_Query
Waiting to attach...
Attached as Child

queryService("ot", "udp")...
Found 1 instance(s) (lastError=0)
  [0] instance=sensor-1 host=sensor-1 port=12345 addr=fd...
       TXT path=/status
```

With OTBR up but nothing advertising `_ot._udp`, expect `Found 0 instance(s) (lastError=0)`
after a few seconds (Discovery Proxy empty answer) — not a timeout error.

Re-run queries every 10 s. After OTBR restart, results may be empty until
Board A re-advertises.
