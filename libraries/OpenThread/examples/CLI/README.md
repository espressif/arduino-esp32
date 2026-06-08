# OpenThread CLI Examples (Arduino)

This folder contains sketches that use the **OpenThread CLI interface** from
Arduino code.

## What is OpenThread CLI?

OpenThread CLI is the command-line interface provided by OpenThread itself.
It exposes networking features through textual commands such as:

- `dataset ...`
- `ifconfig up`
- `thread start`
- `state`
- `ipaddr`
- `udp ...`
- `coap ...`

In the Arduino OpenThread Library, these commands are sent from sketches using
`OThreadCLI` (and helper functions such as `otExecCommand` from
`OThreadCLI_Util`).

## Arduino Stream interface in `OThreadCLI`

`OThreadCLI` is implemented as an Arduino `Stream`, which is a key design
choice. It means the CLI is not limited to interactive terminal use
(`startConsole(Serial)` in `SimpleCLI`) and can also be driven
programmatically.

Because it behaves like a stream, sketches can:

- write commands with `print/println/write`,
- read responses with `available/read/readBytesUntil`,
- control blocking behavior with `setTimeout`,
- register asynchronous handling with `onReceive(...)`.

In practice, this turns OpenThread CLI into an automation channel for your
application logic, not just a debug console.

### Why this matters for complex flows

The same stream primitives allow you to orchestrate multi-step procedures such
as:

- deterministic bring-up sequences (`dataset ...` -> `ifconfig up` -> `thread start`),
- command + response validation loops (`Done` vs `Error ...`),
- event-driven processing of network traffic lines (CoAP/UDP notifications),
- retries, backoff, and fallback behavior in pure sketch code.

This is the mechanism used by examples like `COAP/*` and `UDP/*`, where the
sketch actively drives command flows and parses responses to implement complete
application behavior.

### Console mode vs automation mode

- **Console mode**: user types commands in Serial Monitor; sketch acts like a
  CLI bridge.
- **Automation mode**: sketch generates commands and parses responses itself.
- **Hybrid mode**: same sketch can expose a console for diagnostics while also
  running automated command flows in the background.

## What is the OpenThread Native API?

The Native API is the typed C++ API exposed by Arduino wrappers such as:

- `OThread`
- `DataSet`
- `OThreadUDP` (when available/enabled)

Typical Native usage is function-based, for example:

- `OThread.begin(false)`
- `OThread.commitDataSet(ds)`
- `OThread.networkInterfaceUp()`
- `OThread.start()`

## CLI vs Native: quick comparison

| Topic | CLI approach | Native approach |
| --- | --- | --- |
| Interface style | String commands | Typed methods/functions |
| Error handling | Parse CLI responses (`Done`, `Error ...`) | Return values (`otError`, booleans) |
| Best for | Feature parity with OT shell, quick prototyping, command-driven flows | Stronger compile-time checks, cleaner app structure, easier refactoring |
| Debug visibility | Very explicit command/response logs | Usually cleaner runtime logic, less text parsing |
| Dependency on wrappers | Low: can use features directly if CLI supports them | Depends on wrapper coverage for each feature |

## When should I use CLI?

Use CLI when you want to:

- mirror or reuse known OpenThread shell commands directly,
- validate behavior quickly with command-like flows,
- use a capability exposed in CLI before a Native wrapper exists,
- keep behavior close to COP/manual operational procedures.

## When should I use Native API?

Use Native API when you want to:

- build production-oriented logic with stronger typing,
- reduce string parsing and command formatting code,
- simplify long-term maintenance and refactoring,
- keep the application logic in structured functions/classes.

## Can I mix CLI and Native in the same sketch?

**Yes.** The Arduino OpenThread Library supports mixed usage, and many sketches
benefit from that.

Common mixed pattern:

- use **CLI** for setup steps you want to mirror from OT shell commands,
- use **Native** getters/state checks for logic and telemetry,
- or use **Native** setup plus occasional **CLI** commands for diagnostics.

This flexibility is intentional and useful during development, migration, and
feature bring-up.

## Basic mixed example (CLI + Native in one sketch)

The snippet below shows a very simple and practical pattern:

- use CLI commands for explicit network provisioning/startup,
- use Native API for role checks and application logic in `loop()`.

```cpp
#include <Arduino.h>
#include "OThread.h"
#include "OThreadCLI.h"
#include "OThreadCLI_Util.h"

static bool runCli(const char *cmd) {
  ot_cmd_return_t rc;
  bool ok = otExecCommand(cmd, nullptr, &rc);
  if (!ok) {
    Serial.printf("CLI failed: %s -> %d %s\n", cmd, rc.errorCode, rc.errorMessage.c_str());
  }
  return ok;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  // Start OpenThread stack without auto-start.
  // This lets us define exactly how the network is configured.
  OThread.begin(false);
  OThreadCLI.begin();

  // --- CLI side ---
  // Use familiar OpenThread shell commands for dataset provisioning.
  runCli("thread stop");
  runCli("ifconfig down");
  runCli("dataset clear");
  runCli("dataset init new");
  runCli("dataset networkname MIXED_DEMO");
  runCli("dataset channel 15");
  runCli("dataset networkkey 00112233445566778899aabbccddeeff");
  runCli("dataset commit active");
  runCli("ifconfig up");
  runCli("thread start");

  // --- Native side ---
  // Native getters are convenient for structured app logs/telemetry.
  Serial.printf("Role now: %s\n", OThread.otGetStringDeviceRole());
}

void loop() {
  // Keep application decisions in typed Native API calls.
  if (OThread.otGetDeviceRole() >= OT_ROLE_CHILD) {
    Serial.printf("Attached. Mesh-Local EID: %s\n", OThread.getMeshLocalEid().toString().c_str());
  } else {
    Serial.printf("Not attached yet. Role=%s\n", OThread.otGetStringDeviceRole());
  }

  delay(5000);
}
```

Why this is a good starter pattern:

- CLI setup remains easy to compare with `ot-ctl` / shell docs.
- Native runtime code stays typed, cleaner, and easier to maintain.

## Practical guidance

- Prefer one dominant style per sketch for readability.
- If you mix both, document which layer owns each operation.
- For CLI-heavy sketches, always handle `Error ...` responses robustly.
- For Native-heavy sketches, prefer explicit return-code checks.

## Related examples in this folder

- `SimpleCLI` - basic CLI console pass-through.
- `onReceive` - asynchronous CLI response callback usage.
- `SimpleThreadNetwork/*` - CLI-driven network bring-up examples.
- `COAP/*` - command-driven CoAP workflows via CLI.
- `UDP/*` - command-driven UDP telemetry workflow via CLI.

## License

Apache License 2.0.
