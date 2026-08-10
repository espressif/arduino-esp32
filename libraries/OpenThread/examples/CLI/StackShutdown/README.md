# StackShutdown — Graceful OpenThread Teardown

Minimal sketch that starts OpenThread with default auto-start (`OThread.begin()`),
runs the CLI console for **30 seconds**, then tears the stack down in the
**documented reverse order**.

Use this as a template when firmware must stop Thread (sleep, mode change, or
factory reset) without rebooting the SoC.

## Supported Targets

| SoC      | Thread | Status    |
| -------- | ------ | --------- |
| ESP32-H2 | yes    | Supported |
| ESP32-C6 | yes    | Supported |
| ESP32-C5 | yes    | Supported |

## Shutdown order (application first)

When your sketch uses CoAP or UDP, extend `gracefulShutdown()` in
[CLI StackShutdown sketch](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/StackShutdown):

1. **CoAP** — `OThreadCoAPServer.stop()`, `OThreadCoAPSecureServer.stop()`; destroy
   `OThreadCoAPClient` / `OThreadCoAPSecureClient` objects (releases lazy UDP holds).
2. **UDP** — `OThreadUDP.stop()` on every socket (`OpenThread::end()` does **not**
   close application UDP for you).
3. **CLI** — `OThreadCLI.stopConsole()`, `OThreadCLI.end()`.
4. **Thread (optional)** — `OThread.stop()`, `OThread.networkInterfaceDown()`.
5. **Stack** — `OThread.end()` (also stops CLI and CoAP server globals if still up).

`OpenThread::end()` always stops the CLI task and CoAP server singletons even if
you skip step 3 — but **UDP sockets must be closed in step 2**.

## Expected serial output

```text
=== StackShutdown demo ===
Thread running. OThread.end() will run after RUN_MS.
OpenThread Network Information:
...
Waiting 30000 ms before graceful shutdown...
Shutting down OpenThread (application layers first)...
OpenThread ended. Call OThread.begin() to start again.
```

After shutdown, call `OThread.begin()` again from `setup()` or `loop()` if you
need to restart Thread without resetting the chip.

## See also

* [CLI examples overview](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/CLI/README.md) — when to use CLI vs Native, and related CLI sketches.
* [OpenThread library README — Shutdown order](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/README.md) — full teardown sequence for CoAP, UDP, CLI, and stack deinit.
* [Shutdown Order (Arduino-ESP32 docs)](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread.html) — official reference for `OThread.end()` and application-layer stop order.

## License

Apache License 2.0.
