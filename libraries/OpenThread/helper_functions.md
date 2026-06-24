# OpenThread Helper Functions and Types

The following helper functions and types are designed to simplify writing Arduino sketches for OpenThread.\
They provide useful utilities for managing OpenThread stack behavior and interacting with the Thread network.

### Enumerated Type: `ot_device_role_t`

This enumeration defines the possible roles of a Thread device within the network:

- `OT_ROLE_DISABLED`: The Thread stack is disabled.
- `OT_ROLE_DETACHED`: The device is not currently participating in a Thread network/partition.
- `OT_ROLE_CHILD`: The device operates as a Thread Child.
- `OT_ROLE_ROUTER`: The device operates as a Thread Router.
- `OT_ROLE_LEADER`: The device operates as a Thread Leader.

### Struct: `ot_cmd_return_t`

This structure represents the return status of an OpenThread CLI command:

- `errorCode`: An integer representing the error code (if any).
- `errorMessage`: A string containing an error message (if applicable).

### Function: `otGetDeviceRole()`

- Returns the current role of the device as an `ot_device_role_t` value.
- **Member of `OpenThread`:** call as `OThread.otGetDeviceRole()` (global `OThread` instance).

### Function: `otGetStringDeviceRole()`

- Returns a human-readable string representation of the device role (e.g., "Child," "Router," etc.).
- **Member of `OpenThread`:** call as `OThread.otGetStringDeviceRole()`.

### CLI Helper Functions (`OThreadCLI_Util.h`)

The following are **free functions** for programmatic CLI automation (not methods on `OThread`):

### Function: `otGetRespCmd(const char* cmd, char* resp = NULL, size_t respBufSize = 0, uint32_t respTimeout = 5000)`

- Executes an OpenThread CLI command and retrieves the response.
- Parameters:
  - `cmd`: The OpenThread CLI command to execute.
  - `resp`: Optional buffer to store the response (if provided).
  - `respBufSize`: Size of `resp` in bytes including NUL. Required for raw `char*` buffers; stack arrays should use the two-argument form (size deduced automatically).
  - `respTimeout`: Timeout (in milliseconds) for waiting for the response.
- **Stack buffer (recommended):** `otGetRespCmd("state", resp)` or `otGetRespCmd("state", resp, 10000)` for a custom timeout.
- **Raw pointer:** `otGetRespCmd("state", ptr, len)` or `otGetRespCmd("state", ptr, len, timeout)`.
- When the response exceeds `respBufSize - 1` bytes it is truncated and a warning is logged.

### Function: `otExecCommand(const char* cmd, const char* arg, ot_cmd_return_t* returnCode = NULL, uint32_t respTimeout = 5000)`

- Executes an OpenThread CLI command with an argument.
- Parameters:
  - `cmd`: The OpenThread CLI command to execute.
  - `arg`: The argument for the command.
  - `returnCode`: Optional pointer to an `ot_cmd_return_t` structure to store the return status.
  - `respTimeout`: Timeout (in milliseconds) for Done/Error.

### Function: `otPrintRespCLI(const char* cmd, Stream& output, uint32_t respTimeout)`

- Executes an OpenThread CLI command and prints the response to the specified output stream.
- Parameters:
  - `cmd`: The OpenThread CLI command to execute.
  - `output`: The output stream (e.g., Serial) to print the response.
  - `respTimeout`: Timeout (in milliseconds) for waiting for the response.

### Function: `OpenThread::otPrintNetworkInformation(Stream& output)`

- **Static method on `OpenThread`:** prints role, network name, channel, PAN ID, keys, and IP addresses to the given stream.
- Requires `OThread.begin()` and, for CLI-sourced fields, `OThreadCLI.begin()` if you rely on CLI-backed queries in your build.
- Example: `OThread.otPrintNetworkInformation(Serial);`
- This is **not** a free helper in `OThreadCLI_Util.h` (do not call `otPrintNetworkInformation()` without the `OThread.` prefix).
