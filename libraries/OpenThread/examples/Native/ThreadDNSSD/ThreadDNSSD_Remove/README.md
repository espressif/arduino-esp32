# ThreadDNSSD_Remove

Complete **add / remove / end** demonstration:

1. `begin(hostname)` once  
2. **Two** `addService` → announce → `removeService` cycles (with status checks)  
3. Final `OThreadDNSSD.end()`

Watch OTBR CLI `srp server service` as the instance appears, disappears, and is
fully cleared after `end()`.

## Expected Serial output

```
ThreadDNSSD_Remove — two add/remove cycles, then end()
Waiting to attach...
Attached as Child
OThreadDNSSD.begin("sensor-rm")
  [after begin] announceComplete=0 lastError=0 role=Child

======== add/remove cycle 1 / 2 ========
ADD: addService("ot", "udp", 12347)
Waiting for SRP announce...
  [after add] announceComplete=1 ...
Holding advertised for 8000 ms...
REMOVE: removeService("ot", "udp")
  [after remove] announceComplete=0 ...
Holding removed for 5000 ms before next add...

======== add/remove cycle 2 / 2 ========
ADD: ...
REMOVE: ...

======== final end() ========
Calling OThreadDNSSD.end() ...
  [after end] announceComplete=0 ...
PASS: two add/remove cycles + end() complete
```

## How to test

1. Same OTBR + Network Key as [ThreadDNSSD_Advertise](../ThreadDNSSD_Advertise/README.md).
2. Prefer **Erase Flash: Sketch Only** so the SRP key in NVS is kept.
3. On the OTBR CLI, run `srp server service` during advertised vs removed holds,
   and again after `PASS` — the `sensor-rm` host/service should be gone.
4. Timing knobs in the `.ino`: `kAddRemoveCycles`, `kHoldAdvertisedMs`,
   `kHoldRemovedMs`, `kRemoveSettleMs`.

The OTBR CLI cannot delete another device’s registration; the client must
`removeService` and/or `end()`.

`FAIL: not attached` / `FAIL: OThreadDNSSD.begin` / a failed add or remove cycle
halt the sketch (`while (true)`). Returning from `setup()` would still run
`loop()` and print idle status as if the demo had finished.
