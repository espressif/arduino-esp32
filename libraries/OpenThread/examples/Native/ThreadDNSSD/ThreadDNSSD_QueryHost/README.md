# ThreadDNSSD_QueryHost

Resolve a Thread DNS-SD host label with `OThreadDNSSD.queryHost()`.

## Lab

1. OTBR with SRP **and** DNS in Network Data.
2. Board A: [ThreadDNSSD_Advertise](../ThreadDNSSD_Advertise/) (`sensor-1`).
3. Board B: this sketch. Same `OT_NETKEY`. Change `kPeerHost` if Board A uses another name.

## Expected Serial (Board B)

```
ThreadDNSSD_QueryHost
Waiting to attach...
Attached as Child

queryHost("sensor-1")...
PASS: sensor-1 -> fdde:ad00:...
```
