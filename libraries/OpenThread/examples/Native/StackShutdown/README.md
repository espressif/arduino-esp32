# Native StackShutdown — UDP + CoAP teardown and restart

Demonstrates **full application-layer shutdown** before `OThread.end()` using the
Native API, then **restarts the stack without resetting the chip** by calling
`setup()` again from `gracefulShutdown()`:

1. `OThreadCoAPServer.stop()`
2. `OThreadUDP.stop()`
3. Optional `OThread.stop()` / `OThread.networkInterfaceDown()`
4. `OThread.end()`
5. `setup()` — re-run network formation, UDP bind, and CoAP server start

The sketch forms a small Thread network, binds **UDP port 5050**, registers a
CoAP GET resource on **`hello`**, waits **30 seconds**, runs
`gracefulShutdown()` once, then keeps running with Thread active again.
See [Native StackShutdown sketch](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/StackShutdown).

For CLI-only teardown (no UDP/CoAP, no automatic restart), see
[CLI StackShutdown](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/StackShutdown).

## Supported Targets

| SoC      | Thread | Status    |
| -------- | ------ | --------- |
| ESP32-H2 | yes    | Supported |
| ESP32-C6 | yes    | Supported |
| ESP32-C5 | yes    | Supported |

## Required IDF features (sdkconfig)

| Feature                             | Why        |
| ----------------------------------- | ---------- |
| `CONFIG_OPENTHREAD_ENABLED=y`       | Thread stack |
| `CONFIG_SOC_IEEE802154_SUPPORTED=y` | 802.15.4 radio |

## Expected serial output

```text
=== Native StackShutdown (UDP + CoAP) ===
Attached as Leader
UDP listening on port 5050
CoAP server on path 'hello'.
Waiting 30000 ms before graceful shutdown...
Shutting down (CoAP -> UDP -> stack)...
OpenThread ended. Running setup() to start again.
=== Native StackShutdown (UDP + CoAP) ===
Attached as Leader
UDP listening on port 5050
CoAP server on path 'hello'.
Waiting 30000 ms before graceful shutdown...
```

After the first shutdown, `setup()` runs again and the network comes back.
`loop()` uses a static flag so **shutdown runs only once per boot**; the second
run continues until power cycle or reset.

## Restart pattern

Calling `setup()` from application code is valid on Arduino when you need to
re-initialize globals and singletons (`OThreadCoAPServer`, `OThreadUDP`) after
`OThread.end()`. In production firmware you may instead:

- extract shared bring-up into a function and call it from `setup()` and after
  shutdown;
- reset any `loop()` timers/flags when restarting;
- avoid double-registering CoAP paths if your wrapper does not tolerate repeated
  `on()` without an intervening `stop()`.

This demo keeps the pattern minimal: one shutdown cycle, then indefinite run.

## If you use CoAP clients

Destroy `OThreadCoAPClient` / `OThreadCoAPSecureClient` objects **before**
`OThread.end()` so lazy UDP holds are released. This demo has no client object.

## See also

* [Native examples overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/README.md) — Native examples overview.
* [CLI StackShutdown (CLI-only teardown)](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/StackShutdown) — no UDP/CoAP stop, no automatic restart.
* [Shutdown order](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread.html) — full teardown sequence in the OpenThread docs.

## License

Apache License 2.0.
