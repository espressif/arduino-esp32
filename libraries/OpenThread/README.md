| Supported Targets | ESP32-C5 | ESP32-C6 | ESP32-H2 |
| ----------------- | -------- | -------- | -------- |

# General View

This Arduino OpenThread Library allows using ESP OpenThread implementation using CLI and/or Native OpenThread API.

The Library implements the following C++ classes and helpers:
- `OThread` Class for Native OpenThread API
- `OThreadCLI` Class for CLI OpenThread API
- `OThreadUDP` Class for sending/receiving IPv6 UDP datagrams over the Thread network (raw `otUdpSocket`, no lwIP)
- `OThreadCoAP` Classes for Application CoAP client/server over Thread (plain CoAP on port 5683; optional CoAPS on 5684)
- `DataSet` Class for OpenThread dataset manipulation using Native `OThread` Class

For IPv6 multicast group membership (UDP receivers, CoAP group commands, `subscribeMulticast`, and example mapping), see **[Multicasting.md](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/Multicasting.md)**.

# ESP32 Arduino OpenThread Native

The `OThread` class provides methods for managing the OpenThread instance and controlling the Thread network. It allows you to initialize, start, stop, and manage the Thread network using native OpenThread APIs.

## Class Definition

```cpp
class OpenThread {
  public:
    static bool otStarted; // Indicates whether the OpenThread stack is running.

    // Get the current Thread device role (e.g., Leader, Router, Child, etc.).
    static ot_device_role_t otGetDeviceRole();
    // Get the current Thread device role as a string.
    static const char *otGetStringDeviceRole();
    // Print network information (network name, channel, PAN ID, addresses) to a stream.
    static void otPrintNetworkInformation(Stream &output);

    OpenThread();
    ~OpenThread();
    operator bool() const;            // true when the OpenThread stack is running

    // --- Lifecycle ---
    static void begin(bool OThreadAutoStart = true); // initialize the stack
    static void end();                               // deinitialize the stack
    void start();                                    // start the Thread network
    void stop();                                     // stop the Thread network
    void networkInterfaceUp();                       // "ifconfig up"
    void networkInterfaceDown();                     // "ifconfig down"

    // --- Dataset ---
    void commitDataSet(const DataSet &dataset);
    bool hasActiveDataset() const;          // true if a committed Active Operational Dataset exists
    const DataSet &getCurrentDataSet() const;

    // --- Network parameter getters (from the running instance) ---
    String         getNetworkName() const;
    const uint8_t *getExtendedPanId() const;
    const uint8_t *getNetworkKey() const;
    uint8_t        getChannel() const;
    uint16_t       getPanId() const;

    // --- Live setters (apply directly to the running instance) ---
    otError setChannel(uint8_t channel);
    otError setPanId(uint16_t panid);
    otError setExtendedPanId(const uint8_t *extpanid);
    otError setNetworkKey(const uint8_t *key);
    otError setNetworkName(const char *name);

    // --- Device identity / radio info ---
    bool           getEui64(uint8_t out[8]) const;
    String         getEui64() const;
    const uint8_t *getExtendedAddress() const;
    uint16_t       getThreadVersion() const;
    String         getVersionString() const;
    int8_t         getTxPower() const;
    uint32_t       getPollPeriod() const;

    // --- Joiner (requires CONFIG_OPENTHREAD_JOINER; see "Joiner" section below) ---
    otError startJoiner(const char *pskd,
                        uint32_t    timeoutMs       = 30000,
                        const char *provisioningUrl = nullptr,
                        const char *vendorName      = nullptr,
                        const char *vendorModel     = nullptr,
                        const char *vendorSwVersion = nullptr,
                        const char *vendorData      = nullptr);
    void                stopJoiner();
    otJoinerState       getJoinerState() const;
    const otExtAddress *getJoinerId() const;

    // --- Commissioner (requires CONFIG_OPENTHREAD_COMMISSIONER; see "Commissioner" section) ---
    otError startCommissioner(uint32_t timeoutMs = 30000);
    otError addJoiner(const char *pskd, uint32_t timeoutSec = 120, const otExtAddress *eui64 = nullptr);
    void                stopCommissioner();
    otCommissionerState getCommissionerState() const;

    // --- Instance access ---
    otInstance *getInstance();

    // --- Mesh-local / RLOC addresses ---
    const otMeshLocalPrefix *getMeshLocalPrefix() const;
    IPAddress getMeshLocalEid() const;
    IPAddress getLeaderRloc() const;
    IPAddress getRloc() const;
    uint16_t  getRloc16() const;

    // --- IPv6 address management (cached) ---
    size_t                 getUnicastAddressCount() const;
    IPAddress              getUnicastAddress(size_t index) const;
    std::vector<IPAddress> getAllUnicastAddresses() const;
    size_t                 getMulticastAddressCount() const;
    IPAddress              getMulticastAddress(size_t index) const;
    std::vector<IPAddress> getAllMulticastAddresses() const;

    // --- Address cache management ---
    void clearUnicastAddressCache() const;
    void clearMulticastAddressCache() const;
    void clearAllAddressCache() const;

    // --- Multicast group membership ---
    bool subscribeMulticast(const IPAddress &group);
    bool unsubscribeMulticast(const IPAddress &group);

  private:
    static otInstance *mInstance;   // Pointer to the OpenThread instance.
    static DataSet mCurrentDataset; // Current dataset used by the OpenThread instance.
    // ... plus joiner/commissioner synchronization and IPv6 address caches.
};

extern OpenThread OThread;
```

> The Joiner and Commissioner methods are only compiled when their respective
> Kconfig options (`CONFIG_OPENTHREAD_JOINER` / `CONFIG_OPENTHREAD_COMMISSIONER`)
> are enabled. See the dedicated sections further below.

## Build-time options (Kconfig → OpenThread macros)

Arduino OpenThread builds use ESP-IDF **sdkconfig** symbols (`CONFIG_OPENTHREAD_*`). Many map to OpenThread core macros (`OPENTHREAD_CONFIG_*`) via `openthread-core-esp32x-ftd-config.h`. CoAPS requires a custom OpenThread header — not a direct menu toggle.

| ESP-IDF Kconfig | OpenThread macro | Library impact |
| --- | --- | --- |
| `CONFIG_OPENTHREAD_ENABLED` | (stack on) | Entire library |
| `CONFIG_OPENTHREAD_JOINER` | `OPENTHREAD_CONFIG_JOINER_ENABLE` | `startJoiner()` |
| `CONFIG_OPENTHREAD_COMMISSIONER` | `OPENTHREAD_CONFIG_COMMISSIONER_ENABLE` | `startCommissioner()`, `addJoiner()` |
| `CONFIG_OPENTHREAD_NUM_MESSAGE_BUFFERS` | `OPENTHREAD_CONFIG_NUM_MESSAGE_BUFFERS` | UDP / CoAP message pool |
| (FTD default) | `OPENTHREAD_CONFIG_COAP_API_ENABLE=1` | Plain `OThreadCoAP*` |
| Custom header | `OPENTHREAD_CONFIG_COAP_SECURE_API_ENABLE=1` | `OThreadCoAPSecure*` |
| `CONFIG_OPENTHREAD_HEADER_CUSTOM` + header path | User `OPENTHREAD_CONFIG_*` overrides | CoAPS, resource limits, advanced tuning |

Full table: [`docs/en/openthread/openthread.rst`](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread.html).

## Class Overview

The `OThread` class provides a simple and intuitive interface for managing the OpenThread stack and Thread network. It abstracts the complexity of the OpenThread APIs and provides Arduino-style methods for common operations.

## Public Methods
### Initialization and Deinitialization
- `begin(bool OThreadAutoStart = true)`: Initializes the OpenThread stack. If `OThreadAutoStart` is `true`, the Thread network will start automatically using NVS data.
- `end()`: Deinitializes the OpenThread stack and releases resources.
### Thread Network Control
- `start()`: Starts the Thread network. This is equivalent to the CLI command "thread start".
- `stop()`: Stops the Thread network. This is equivalent to the CLI command "thread stop".
### Network Interface Control
- `networkInterfaceUp()`: Brings up the Thread network interface. This is equivalent to the CLI command "ifconfig up".
- `networkInterfaceDown()`: Brings down the Thread network interface. This is equivalent to the CLI command "ifconfig down".
### Dataset Management
- `commitDataSet(const DataSet &dataset)`: Commits a dataset to the OpenThread instance. This is used to configure the Thread network with specific parameters (e.g., network name, channel, PAN ID).
- `hasActiveDataset()`: Returns `true` when a committed Active Operational Dataset exists (loaded from NVS during `begin()` or committed by the application). Useful to decide whether to resume an existing network or provision a new one.
- `getCurrentDataSet()`: Returns a reference to the current `DataSet` tracked by the instance.
### Network Parameters
- Getters from the running instance: `getNetworkName()`, `getExtendedPanId()`, `getNetworkKey()`, `getChannel()`, `getPanId()`.
- Live setters applied directly to the running instance: `setChannel()`, `setPanId()`, `setExtendedPanId()`, `setNetworkKey()`, `setNetworkName()`. Useful when joining via Commissioner/Joiner where the full dataset is not built locally. Stop the Thread protocol before changing these on an already-attached device.
### Device Identity and Radio
- `getEui64(uint8_t out[8])` / `getEui64()`: Factory-assigned EUI-64 (raw bytes or hex `String`).
- `getExtendedAddress()`: Currently configured 802.15.4 extended address (may differ from the EUI-64).
- `getThreadVersion()`: Thread protocol version (e.g. `4` == 1.3).
- `getVersionString()`: Human-readable OpenThread stack version.
- `getTxPower()`: Radio transmit power in dBm (`INT8_MIN` on failure).
- `getPollPeriod()`: Sleepy End Device data poll period in milliseconds.
### Addressing
- `getMeshLocalPrefix()`, `getMeshLocalEid()`: Mesh-local prefix and EID.
- `getLeaderRloc()`, `getRloc()`, `getRloc16()`: Leader RLOC, node RLOC, and RLOC16.
- IPv6 address management (cached): `getUnicastAddressCount()` + `getUnicastAddress(index)` or `getAllUnicastAddresses()`; the same trio exists for multicast.
- Cache management: `clearUnicastAddressCache()`, `clearMulticastAddressCache()`, `clearAllAddressCache()` — call when the role changes to force re-resolution.
- Multicast group membership: `subscribeMulticast(group)` / `unsubscribeMulticast(group)` — reference-counted join/leave shared by `OThreadUDP.beginMulticast()`, `OThreadCoAPServer.joinMulticastGroup()`, and direct API calls. See **[Multicasting.md](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/Multicasting.md)** for usage, scopes, and examples.
### Network Information
- `otGetDeviceRole()`: Returns the current Thread device role as an `ot_device_role_t` enum (e.g., `OT_ROLE_LEADER`, `OT_ROLE_ROUTER`).
- `otGetStringDeviceRole()`: Returns the current Thread device role as a string (e.g., "Leader", "Router").
- `otPrintNetworkInformation(Stream &output)`: Prints the current network information (e.g., network name, channel, PAN ID) to the specified stream.
- `getInstance()`: Returns the underlying `otInstance *` for direct native OpenThread API calls.

## Key Features
- **Initialization and Cleanup**: Easily initialize and deinitialize the OpenThread stack.
- **Network Control**: Start and stop the Thread network with simple method calls.
- **Dataset Management**: Configure the Thread network using the `DataSet` class and commit it to the OpenThread instance.
- **Network Information**: Retrieve and print the current network information and device role.

## Notes
- The `OThread` class is designed to simplify the use of OpenThread APIs in Arduino sketches.
- It works seamlessly with the DataSet class for managing Thread network configurations.
- Ensure that the OpenThread stack is initialized (`OThread.begin()`) before calling other methods.

This documentation provides a comprehensive overview of the `OThread` class, its methods, and example usage. It is designed to help developers quickly integrate OpenThread functionality into their Arduino projects.

# DataSet Class

The `DataSet` class provides a structured way to manage and configure Thread network datasets using native OpenThread APIs. It allows you to set and retrieve network parameters such as the network name, channel, PAN ID, and more. The `DataSet` class works seamlessly with the `OThread` class to apply these configurations to the OpenThread instance.

## Class Definition

```cpp
class DataSet {
public:
    DataSet();
    void clear();
    void initNew();
    const otOperationalDataset &getDataset() const;

    // Setters
    void setNetworkName(const char *name);
    void setExtendedPanId(const uint8_t *extPanId);
    void setNetworkKey(const uint8_t *key);
    void setChannel(uint8_t channel);
    void setPanId(uint16_t panId);

    // Getters
    const char *getNetworkName() const;
    const uint8_t *getExtendedPanId() const;
    const uint8_t *getNetworkKey() const;
    uint8_t getChannel() const;
    uint16_t getPanId() const;

    // Apply the dataset to the OpenThread instance
    void apply(otInstance *instance);

private:
    otOperationalDataset mDataset; // Internal representation of the dataset
};
```

## Class Overview
The `DataSet` class simplifies the management of Thread network datasets by providing intuitive methods for setting, retrieving, and applying network parameters. It abstracts the complexity of the OpenThread dataset APIs and provides Arduino-style methods for common operations.

## Public Methods
### Initialization
- `DataSet()`: Constructor that initializes an empty dataset.
- `void clear()`: Clears the dataset, resetting all fields to their default values.
- `void initNew()`: Initializes a new dataset with default values (equivalent to the CLI command dataset init new).
### Setters
- `void setNetworkName(const char *name)`: Sets the network name.
- `void setExtendedPanId(const uint8_t *extPanId)`: Sets the extended PAN ID.
- `void setNetworkKey(const uint8_t *key)`: Sets the network key.
- `void setChannel(uint8_t channel)`: Sets the channel.
- `void setPanId(uint16_t panId)`: Sets the PAN ID.
### Getters
- `const char *getNetworkName() const`: Retrieves the network name.
- `const uint8_t *getExtendedPanId() const`: Retrieves the extended PAN ID.
- `const uint8_t *getNetworkKey() const`: Retrieves the network key.
- `uint8_t getChannel() const`: Retrieves the channel.
- `uint16_t getPanId() const`: Retrieves the PAN ID.
### Dataset Application
- `void apply(otInstance *instance)`: Applies the dataset to the specified OpenThread instance.

## Key Features
- **Dataset Initialization**: Easily initialize a new dataset with default values using initNew().
- **Custom Configuration**: Set custom network parameters such as the network name, channel, and PAN ID using setter methods.
- **Dataset Application**: Apply the configured dataset to the OpenThread instance using apply().

## Notes
- The `DataSet` class is designed to work seamlessly with the `OThread` class for managing Thread network configurations.
- Ensure that the OpenThread stack is initialized (`OThread.begin()`) before applying a dataset.
- The `initNew()` method provides default values for the dataset, which can be customized using the setter methods.

This documentation provides a comprehensive overview of the `DataSet` class, its methods, and example usage. It is designed to help developers easily manage Thread network configurations in their Arduino projects.

# OpenThreadCLI Class

The `OpenThreadCLI` class is an Arduino API for interacting with the OpenThread Command Line Interface (CLI). It allows you to send commands to the OpenThread stack and receive responses. This class is designed to simplify the use of OpenThread CLI commands in Arduino sketches.

There is one main class called `OpenThreadCLI` and a global object used to operate OpenThread CLI, called `OThreadCLI`.\
Some [helper functions](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/helper_functions.md) were made available for working with the OpenThread CLI environment.

The available OpenThread Commands are documented in the [OpenThread CLI Reference Page](https://openthread.io/reference/cli/commands)

It is important to note that the current implementation can only be used with Espressif SoCs that have IEEE 802.15.4 radio support, such as **ESP32-C5**, **ESP32-C6** and **ESP32-H2**.

Below are the details of the class:

## Class Definition

```cpp
class OpenThreadCLI : public Stream {
private:
  static size_t setBuffer(QueueHandle_t &queue, size_t len);
  static bool otCLIStarted;

public:
  OpenThreadCLI();
  ~OpenThreadCLI();
  operator bool() const;

  // Starts a task to read/write otStream. Default prompt is "ot> ". Set it to NULL to make it invisible.
  void startConsole(Stream& otStream, bool echoback = true, const char* prompt = "ot> ");
  void stopConsole();
  void setPrompt(char* prompt);      // Changes the console prompt. NULL is an empty prompt.
  void setEchoBack(bool echoback);   // Changes the console echoback option
  void setStream(Stream& otStream);  // Changes the console Stream object
  void onReceive(OnReceiveCb_t func);  // Called on a complete line of output from OT CLI, as OT Response

  void begin();
  void end();

  // Default size is 256 bytes
  size_t setTxBufferSize(size_t tx_queue_len);
  // Default size is 1024 bytes
  size_t setRxBufferSize(size_t rx_queue_len);

  size_t write(uint8_t);
  int available();
  int read();
  int peek();
  void flush();
};

extern OpenThreadCLI OThreadCLI;
```

## Class Overview
- The `OpenThreadCLI` class inherits from the `Stream` class, making it compatible with Arduino's standard I/O functions.
- It provides methods for managing the OpenThread CLI, including starting and stopping the console, setting prompts, and handling received data.
- You can customize the console behavior by adjusting parameters such as echoback and buffer sizes.

## Public Methods
### Initialization and Deinitialization
- `begin()`: Initializes the OpenThread CLI layer. Call after `OThread.begin()`.
- `end()`: Deinitializes the OpenThread CLI and releases resources.
### Console Management
- `startConsole(Stream& otStream, bool echoback = true, const char* prompt = "ot> ")`: Starts the OpenThread console with the specified stream, echoback option, and prompt.
- `stopConsole()`: Stops the OpenThread console.
- `setPrompt(char* prompt)`: Changes the console prompt (set to NULL for an empty prompt).
- `setEchoBack(bool echoback)`: Changes the console echoback option.
- `setStream(Stream& otStream)`: Changes the console Stream object.
- `onReceive(OnReceiveCb_t func)`: Sets a callback function to handle complete lines of output from the OT CLI.
### Buffer Management
- `setTxBufferSize(size_t tx_queue_len)`: Sets the transmit buffer size (default is 256 bytes).
- `setRxBufferSize(size_t rx_queue_len)`: Sets the receive buffer size (default is 1024 bytes).
### Stream Methods
- `write(uint8_t)`: Writes a byte to the CLI.
- `available()`: Returns the number of bytes available to read.
- `read()`: Reads a byte from the CLI.
- `peek()`: Returns the next byte without removing it from the buffer.
- `flush()`: Flushes the CLI buffer.

## Key Features
- **Arduino Stream Compatibility**: Inherits from the Stream class, making it compatible with Arduino's standard I/O functions.
- **Customizable Console**: Allows customization of the CLI prompt, echoback behavior, and buffer sizes.
- **Callback Support**: Provides a callback mechanism to handle CLI responses asynchronously.
- **Seamless Integration**: Designed to work seamlessly with the OThread and DataSet classes

## Notes
- The `OThreadCLI` class is designed to simplify the use of OpenThread CLI commands in Arduino sketches.
- It works seamlessly with the `OThread` and `DataSet` classes for managing Thread networks.
- Call `OThread.begin()` before `OThreadCLI.begin()` and before `OThreadCLI.startConsole()`.
- The `onReceive()` callback runs on the **OpenThread worker task**. Keep it short: do not block, call `otGetRespCmd()` / `otExecCommand()`, or perform heavy work there (defer to `loop()`).

This documentation provides a comprehensive overview of the `OThreadCLI` class, its methods, and example usage. It is designed to help developers easily integrate OpenThread CLI functionality into their Arduino projects.

# Joiner (Thread Commissioning)

The `OThread` class exposes a synchronous Thread Joiner helper that drives the OpenThread commissioning state machine. Use it when the device should obtain its operational dataset (network key, ext-PAN-ID, channel, ...) from an external Commissioner via the PSKd shared secret instead of being preconfigured with a `DataSet`.

## Required order

1. `OThread.begin(false)` - initialize the stack without auto-starting Thread (do **not** commit a DataSet).
2. (optional) Pre-configure the radio with `setChannel()`, `setPanId()`, `setExtendedPanId()` so the joiner only listens on the expected channel/PAN.
3. `OThread.networkInterfaceUp()` - the IPv6 stack must be up.
4. `OThread.startJoiner(PSKD)` - blocks until commissioning completes (or `timeoutMs` elapses). Do **not** call `start()` before this step.
5. On success, call `OThread.start()` to enable Thread with the provisioned dataset.

## Return values

`startJoiner()` returns an `otError`:

- `OT_ERROR_NONE` - successfully joined the network.
- `OT_ERROR_SECURITY` - PSKd mismatch.
- `OT_ERROR_NOT_FOUND` - no joinable network was discovered.
- `OT_ERROR_RESPONSE_TIMEOUT` - commissioner did not respond within `timeoutMs`.
- `OT_ERROR_INVALID_STATE` - IPv6 stack not up, or Thread already enabled.

## Joiner status helpers

- `OThread.getJoinerState()` returns the live `otJoinerState` (idle / discover / connect / connected / entrust / joined).
- `OThread.getJoinerId()` returns the device's joiner ID (used as IEEE 802.15.4 extended address during commissioning).

## Timeouts at a glance

Three independent timeouts are at play during a join. Tune them in pairs so the joiner side does not give up before the commissioner side does:

| Timeout                                        | Owner            | Default | Purpose                                                                                              |
| ---------------------------------------------- | ---------------- | ------- | ---------------------------------------------------------------------------------------------------- |
| `startJoiner(..., timeoutMs)`                  | Joiner sketch    | 30 s    | How long the wrapper blocks on its FreeRTOS semaphore. On expiry the wrapper calls `otJoinerStop()`. |
| Internal MeshCoP / DTLS timers                 | OpenThread stack | various | Not configurable from Arduino. Causes the callback to eventually fire with `OT_ERROR_NOT_FOUND` or `OT_ERROR_RESPONSE_TIMEOUT`. |
| `addJoiner(..., timeoutSec)`                   | Commissioner     | 120 s   | How long the joiner entry remains valid on the commissioner. After this it must be re-added.        |

For best interoperability use `startJoiner(..., timeoutSec_for_commissioner * 1000 + 10000)`.

# Commissioner (Thread Commissioning - Leader Side)

The `OThread` class also exposes the **other** side of the commissioning flow: petitioning the Commissioner role and authorizing a remote joiner.

The Commissioner must already be **attached** to the Thread network (typically as the Leader of a freshly-formed partition).

## Required order

1. Form or resume the network (`commitDataSet()` or NVS resume).
2. `OThread.networkInterfaceUp()` + `OThread.start()` - bring up Thread until the device is no longer Detached / Disabled.
3. `OThread.startCommissioner()` - blocks until the Commissioner is `ACTIVE` (default timeout: 30 s).
4. `OThread.addJoiner(PSKD, /*timeoutSec=*/120)` - whitelists any joiner that knows `PSKD` for 120 s.
5. The companion Joiner device now runs `startJoiner(PSKD)` on its side; the dataset is delivered to it over an authenticated DTLS handshake.
6. (Optional) Call `OThread.stopCommissioner()` once you no longer want to admit new joiners.

**Join-window patterns in examples:** most Native lamp/CoAP/UDP servers call `addJoiner()` immediately after `startCommissioner()` succeeds. `ThreadCommissioning/CommissionerNode` opens the window only when the user presses a button — see [`examples/Native/README.md`](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/README.md).

## API

```cpp
otError startCommissioner(uint32_t timeoutMs = 30000);
otError addJoiner(const char *pskd,
                  uint32_t timeoutSec = 120,
                  const otExtAddress *eui64 = nullptr);
void                stopCommissioner();
otCommissionerState getCommissionerState() const;
```

## Return values

- `OT_ERROR_NONE` - success.
- `OT_ERROR_REJECTED` - petition rejected (another commissioner already active in the partition, or local state fell back to `DISABLED`).
- `OT_ERROR_RESPONSE_TIMEOUT` - petition did not resolve within `timeoutMs` (the wrapper auto-calls `otCommissionerStop`).
- `OT_ERROR_INVALID_STATE` - the device is not attached to a network yet.
- `OT_ERROR_NO_BUFS` / `OT_ERROR_INVALID_ARGS` - for `addJoiner`: commissioner table full or invalid PSKd.

## PSKd format

The PSKd is an ASCII string, **6 to 32 characters**, using the base32-thread alphabet: digits and uppercase letters, **excluding** `I`, `O`, `Q` and the digit `0`. Both sides must use the exact same value.

## Required Kconfig

- `CONFIG_OPENTHREAD_COMMISSIONER=y` on the leader side.
- `CONFIG_OPENTHREAD_JOINER=y` on the joiner side.

## Working example

`examples/Native/ThreadCommissioning/` contains a ready-to-run pair:

- [`CommissionerNode`](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/ThreadCommissioning/CommissionerNode/CommissionerNode.ino) builds a fresh DataSet, forms the network, then runs the Commissioner.
- [`JoinerNode`](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/ThreadCommissioning/JoinerNode/JoinerNode.ino) has **no** local DataSet, only the PSKd, and uses `startJoiner()` to obtain the dataset over the air.

# OThreadUDP Class - IPv6 UDP over Thread

`OThreadUDP` is an Arduino `UDP` implementation backed directly by the `otUdpSocket` API. It does not go through lwIP, so it is the lightest path to send/receive IPv6 UDP datagrams over the Thread mesh. For multicast receivers use `beginMulticast()`; see **[Multicasting.md](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/Multicasting.md)**.

## Class Definition

```cpp
class OThreadUDP : public UDP {
public:
  OThreadUDP();
  ~OThreadUDP();
  operator bool() const;

  uint8_t begin(IPAddress addr, uint16_t port);
  uint8_t begin(uint16_t port);              // bind to OT_IN6ADDR_ANY
  uint8_t beginMulticast(IPAddress group, uint16_t port);
  void    stop();

  int    beginPacket(IPAddress ip, uint16_t port);
  int    beginPacket(const char *host, uint16_t port);  // textual IPv6
  int    endPacket();
  size_t write(uint8_t);
  size_t write(const uint8_t *buf, size_t size);

  int       parsePacket();
  int       available();
  int       read();
  int       read(unsigned char *buf, size_t len);
  int       read(char *buf, size_t len);
  int       peek();
  void      flush();
  IPAddress remoteIP();
  uint16_t  remotePort();
};

extern const IPAddress OT_IN6ADDR_ANY;
```

## Tunables

These build-time defines control the per-instance RX queue. Override them on the command line if your application sends/receives larger or more bursty traffic:

| Macro | Default | Meaning |
| ----- | ------- | ------- |
| `OT_UDP_MAX_PACKET_SIZE` | 512  | Maximum payload bytes of a single received datagram. Larger packets are truncated. |
| `OT_UDP_RX_QUEUE_DEPTH`  | 4    | Number of datagrams that may be queued before the oldest is dropped. |

## Example (UDP Light + Switch)

`examples/Native/UDP/UDP_Light_Switch/` contains a complete two-board demo that combines the Joiner / Commissioner flow with `OThreadUDP` to build a controllable IoT lamp:

- [`light`](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/light/light.ino) is the **server**. It boots as Thread Leader, resumes the persisted dataset when available, runs the **Commissioner** role (accepts joiners that present PSKd `J01NME`) and subscribes to the realm-local multicast group `ff03::abcd` on UDP port `5051`. For every `ON` / `OFF` / `TOGGLE` / `STATUS` command it receives, it updates the on-board RGB LED ("lamp") and unicasts an `ACK ON` / `ACK OFF` confirmation back to the sender.
- [`switch`](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/UDP/UDP_Light_Switch/switch/switch.ino) is the **client**. It boots **without** any local DataSet, runs the **Joiner** state machine, and once attached opens a UDP socket on the same port. Pressing the BOOT button sends `TOGGLE` to `ff03::abcd` on UDP port `5051` and waits for the light's confirmation packet within 1 s.

One light can serve **many switches** in parallel - just flash `switch.ino` on additional H2 / C6 / C5 boards within the joining window. The pair demonstrates an end-to-end IoT pattern (commissioning + many-to-one multicast control + per-command acknowledgment) using only the Native OpenThread API. Port `5051` is used to avoid OpenThread-reserved CoAP ports such as `5683`, `5684`, and `61631`.

For a telemetry-oriented deployment with many sleepy sensors, see:

- [`examples/Native/UDP/UDP_SensorNetwork/sensor_collector/sensor_collector.ino`](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork/sensor_collector/sensor_collector.ino): leader + commissioner + UDP sink that tracks up to 256 unique node IDs and prints periodic fleet status.
- [`examples/Native/UDP/UDP_SensorNetwork/sensor_node/sensor_node.ino`](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/examples/Native/UDP/UDP_SensorNetwork/sensor_node/sensor_node.ino): joiner sensor client with optional Sleepy End Device behavior (`otLinkSetRxOnWhenIdle(false)`, poll period, child timeout) that periodically uploads readings over UDP.

# OThreadCoAP — Application CoAP over Thread

`OThreadCoAP*` classes wrap the OpenThread Application CoAP API (`otCoap*`) in an
Arduino-style surface inspired by `HTTPClient` and `WebServer`. Plain CoAP uses
UDP port **5683**; CoAPS (DTLS) uses port **5684** when enabled at build time.

## Headers and classes

```cpp
#include <OThreadCoAP.h>
```

| Class / global | Role |
| --- | --- |
| `OThreadCoAP` | Static helpers (`isAttached()`, `plainServer()`, `secureServer()`, …) |
| `OThreadCoAPClient` | Blocking GET/PUT/POST/DELETE client (create as many as you need) |
| `OThreadCoAPServerClass` | Plain resource server API type — **do not construct** |
| **`OThreadCoAPServer`** | **Global singleton** plain server (port 5683): `on()`, `onNotFound`, `serve`, multicast |
| `OThreadCoAPSecureClient` | CoAPS client (one active DTLS session per device) |
| `OThreadCoAPSecureServerClass` | CoAPS server API type — **do not construct** |
| **`OThreadCoAPSecureServer`** | **Global singleton** CoAPS server (port 5684): `on()` + credentials |
| `OThreadCoAPResourceStore` | REST CRUD helper — attaches to `OThreadCoAPServer` only |
| `OThreadCoAPRequest` / `OThreadCoAPResponse` | Handler arguments |

**Singleton servers (like `OThread` / `Wi-Fi`).** OpenThread exposes one plain CoAP UDP
bind and one CoAPS bind per device. The library provides **`OThreadCoAPServer`** and
**`OThreadCoAPSecureServer`** as pre-defined globals — include `OThreadCoAP.h` and call
`OThreadCoAPServer.on(...)` / `OThreadCoAPServer.begin()` directly. Do not declare a
local `OThreadCoAPServer` variable. The underlying types are `OThreadCoAPServerClass` and
`OThreadCoAPSecureServerClass`; `OThreadCoAP::plainServer()` and
`OThreadCoAP::secureServer()` return the same instances. Plain and secure **servers** may
run together on one device (`CoAP_Greenhouse`). Do **not** run `OThreadCoAPSecureClient.connect()`
on the same device as `OThreadCoAPSecureServer` — both directions are rejected while the
other role is active. Use the client on a peer node (`CoAP_Secure`, `CoAP_Greenhouse`).
Plain clients (`OThreadCoAPClient`) are fine on any node. `OThreadCoAPServer.begin()` is
idempotent and re-binds resources after `OThread.end()` or a failed `stop()`. It returns
`bool`; use `if (OThreadCoAPServer)` (`operator bool()`) to check whether the server
wrapper is running. If `begin()` fails during resync (e.g. after `OThread.end()`),
`running` is cleared so the return value and `operator bool()` stay consistent.

Register paths with `OThreadCoAPServer.on(path, methodMask, handler)`; pass **`nullptr`**
for `handler` to unregister a path (Arduino `WebServer` uses a separate `removeRoute()`).
The same applies to `OThreadCoAPSecureServer.on()`. Use `onNotFound(nullptr)` to disable
the default unknown-path handler.

If `OThreadCoAPServer.stop()` fails to acquire the OpenThread lock, wrapper state resets
but the CoAP UDP service may still be active in the stack — call `begin()` again to recover,
or `OThread.end()`. Each server supports up to `OT_COAP_MAX_RESOURCES` (default 4) `on()`
slots; unregister unused paths or raise the limit at build time.

Server handlers run in **callback mode** (OpenThread invokes the handler when a
request arrives — no `loop()` polling). Call `resp.send()` before returning.

## Confirmable vs non-confirmable (CON / NON)

CoAP (RFC 7252) tags every message as **confirmable (CON)** or **non-confirmable
(NON)**. That choice drives reliability, blocking, and retransmission.

| Type | CLI | Behavior |
|------|-----|----------|
| **CON** | `con` | Expects a CoAP-layer **ACK**. Plain client: OpenThread **retransmits** if no response within the timeout window. Blocking `GET`/`PUT`/`POST`/`DELETE` wait for a reply or `OT_COAP_ERROR_TIMEOUT`. |
| **NON** | `non-con` | Fire-and-forget at the message layer (no ACK for the request). Lower overhead; fine when a missed sample is acceptable. The server may still answer. |

**Default:** `OThreadCoAPClient` and `OThreadCoAPSecureClient` use **CON**
(`setConfirmable(true)`).

### Client

```cpp
OThreadCoAPClient client;
client.setConfirmable(true);   // CON — reliable unicast (default)
int code = client.GET(serverIp, "hello");

client.setConfirmable(false);  // NON — telemetry polling
code = client.GET(serverIp, "sensor/temp");

// Always NON, never blocks (multicast or unicast):
client.sendNonBlocking(group, OT_COAP_REQ_PUT, "Lamp", (const uint8_t *)"1", 1);
```

- `setConfirmable(bool)` applies to subsequent blocking requests on that client
  until changed again.
- `sendNonBlocking()` is always **NON** regardless of `setConfirmable()`.
- **Multicast** destinations (`0xFF…`): blocking methods **force NON** (RFC 7252
  §8.1) even if `setConfirmable(true)`. Prefer `sendNonBlocking()` for group
  commands; use unicast CON when you need one confirmed answer. See
  [Multicasting.md](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/Multicasting.md).

### Timeouts and CON retransmission (plain client)

- **CON only:** `setTimeout(ms)` blocks the sketch and, by default, aligns wire
  retransmission so `OT_COAP_ERROR_TIMEOUT` means no answer within that window.
  Minimum effective timeout is **1500 ms** (`OT_COAP_MIN_ALIGNED_TIMEOUT_MS`).
- `useDefaultCoapRetransmit()` (before `setTimeout()`) keeps RFC 7252 wire
  retries (~93 s) while the sketch still waits only `setTimeout(ms)`.
- **NON** requests are not CON-retried.
- **CoAPS:** `setConfirmable()` still selects CON/NON; `setTimeout()` is only a
  sketch wait cap — wire retries are not tunable per request.

### Multicast and CON

Multicast CoAP and confirmable (CON) messages **do not combine**. The library
enforces RFC 7252 when sending:

| Situation | Library behavior |
|-----------|------------------|
| `setConfirmable(true)` + destination `0xFF…` | Sent as **NON** anyway (§8.1). Confirmable is ignored for multicast. |
| CON retransmission / `setTimeout()` tuning | **Never applies** to multicast — those sends are always NON, not CON-retried. |
| Blocking `GET`/`PUT`/`POST`/`DELETE` to a group | Still **NON** on the wire; may block for the first reply or timeout. Prefer `sendNonBlocking()` for group commands. |
| `sendNonBlocking()` to multicast | Always **NON**, never blocks — use for lamp/actuator groups (`CoAP_Light_Switch`). |
| Server receives group request | Join with `joinMulticastGroup()` first. Request is **NON**; skip `resp.send()` when `req.isMulticast()` (§8.2). |
| Confirmed command to **one** device | **Unicast CON** to that node's IPv6 address, not the group. |

CLI `coap_switch` uses CON to multicast as a demo; Native `CoAP_Light_Switch`
uses NON via `sendNonBlocking()`. Group membership: [Multicasting.md](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/Multicasting.md).

### Server

In handlers, use `OThreadCoAPRequest`:

- `req.isConfirmable()` — incoming CON vs NON.
- `req.isMulticast()` — group request; usually **skip** `resp.send()` (RFC 7252
  §8.2).
- **Response type mirrors the request:** CON in → ACK out; NON in → NON out
  (handled inside the library when you call `resp.send()`).

All `on()` paths accept both CON and NON. `serve()` answers unicast only.

### When to use which

| Use case | Setting |
|----------|---------|
| Command to one known server (CRUD, config, actuators) | **CON** + blocking `PUT`/`POST` |
| Periodic telemetry poll | **NON** — `setConfirmable(false)` |
| Multicast command to many devices | `sendNonBlocking()`; server `joinMulticastGroup()` + no reply on multicast |
| CoAPS secured control | **CON** in `CoAP_Secure` / `CoAP_Greenhouse` actuator paths |

### Examples

| Example | CON / NON |
|---------|-----------|
| [`CoAP_SimpleGet/client`](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet/client) | CON GET `hello` |
| [`CoAP_Sensor/client`](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor/sensor_client) | NON periodic GET `sensor/temp` |
| [`CoAP_CRUD/notes_client`](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD/notes_client) | CON CRUD |
| [`CoAP_Light_Switch/switch`](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch/switch) | NON multicast `sendNonBlocking()` |
| [`CoAP_Greenhouse/client`](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse/greenhouse_client) | NON telemetry + CON CoAPS actuators |
| CLI [`coap_switch`](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/COAP/coap_switch) | CON multicast PUT (CLI demo; Native switch uses NON) |

**Multicast.** A request to an IPv6 multicast group (e.g. `ff03::abcd`) is always
sent non-confirmable (RFC 7252 §8.1). The recommended pattern is fire-and-forget:
`client.sendNonBlocking(group, method, path, payload)` sends one NON request without
blocking, and the server **does not reply** to it (RFC 7252 §8.2 — otherwise every
node sharing the group answers the same datagram). Server-side, the auto-response is
suppressed for multicast and handlers should check `req.isMulticast()` before calling
`resp.send()`. Call `OThreadCoAPServer.begin()` before `joinMulticastGroup(group)`
(required — returns `false` if the server is not running). Groups are left automatically
on `stop()`, including after a failed `stop()` that cleared `running`. Use
`OThread.subscribeMulticast(group)` for interface-level join without the CoAP server.
To confirm a single server, address it by unicast and use the blocking `client.PUT(...)`.
Full guide: **[Multicasting.md](https://github.com/espressif/arduino-esp32/blob/master/libraries/OpenThread/Multicasting.md)**.

## Example (minimal server + client)

```cpp
#include <OThread.h>
#include <OThreadCoAP.h>

static void onHello(OThreadCoAPRequest &req, OThreadCoAPResponse &resp, void *ctx) {
  (void)ctx;
  resp.setCode(OT_COAP_RESP_OK);
  resp.setPayload("Hello from CoAP!");
  resp.send();
}

void setup() {
  OThread.begin(false);
  // ... attach to Thread network ...
  OThreadCoAPServer.on("hello", OT_COAP_METHOD_GET, onHello);
  OThreadCoAPServer.begin();
}
```

## Client options

Plain client (`OThreadCoAPClient`) methods used in most examples:

```cpp
OThreadCoAPClient client;
client.setTimeout(5000);                          // ms — blocking wait cap (CON tuning)
client.setConfirmable(true);                      // CON (default) or NON — see section above
client.setContentFormat(OT_COAP_FORMAT_JSON);     // POST/PUT payload type
client.setPort(5683);                             // remote destination port
```

- **`setConfirmable(bool)`** — CON (`true`, default) or NON (`false`). See
  **Confirmable vs non-confirmable (CON / NON)** above.
- **`setTimeout(ms)`** — For confirmable plain CoAP, CON retransmission is tuned to
  match, so `OT_COAP_ERROR_TIMEOUT` means no answer within this window. Values below
  **1500 ms** (`OT_COAP_MIN_ALIGNED_TIMEOUT_MS`) are raised automatically. CoAPS:
  sketch wait cap only — wire retries are not tunable per request.
- **`useDefaultCoapRetransmit()`** — Call **before** `setTimeout()` to keep RFC 7252
  wire retries (~93 s) while the sketch still blocks only for `setTimeout(ms)`.
- **`setContentFormat(format)`** — IANA Content-Format on the next request with a
  body (omitted on GET/DELETE without payload). Common values: `OT_COAP_FORMAT_TEXT`
  (0), `OT_COAP_FORMAT_JSON` (50), `OT_COAP_FORMAT_OCTET_STREAM` (42). Full
  `OT_COAP_FORMAT_*` list is in `OThreadCoAP.h`.

Check **`OThreadCoAP::secureApiEnabled()`** before using CoAPS classes; when the
secure API is disabled at build time, `begin()` / `connect()` fail with
`OT_COAP_ERROR_NOT_CONNECTED`.

## Error returns

Blocking client methods return a CoAP response code (≥ 0) or a negative
`OT_COAP_ERROR_*` value. Use `OThreadCoAP::errorToString(code)` in logs.

| Code | Meaning |
| --- | --- |
| `OT_COAP_ERROR_TIMEOUT` (-11) | No response within `setTimeout()` |
| `OT_COAP_ERROR_NO_RESPONSE` (-12) | No CoAP response received |
| `OT_COAP_ERROR_NOT_ATTACHED` (-13) | Device not on Thread yet (role &lt; Child) |
| `OT_COAP_ERROR_NO_BUFS` (-14) | Out of message buffers / allocation failed |
| `OT_COAP_ERROR_INVALID_ARGS` (-15) | Bad arguments (e.g. invalid IPv6 host) |
| `OT_COAP_ERROR_NOT_CONNECTED` (-16) | CoAPS session not up, or secure API unavailable |
| `OT_COAP_ERROR_TLS_FAILED` (-17) | DTLS handshake or secure send failed |
| `OT_COAP_ERROR_INVALID_STATE` (-18) | CoAP service or OT lock not ready — **not** the same as “not attached” |

Call `server.begin()` and run client requests only after attach (role ≥ Child).
For a custom server listen port (not 5683), call `server.begin()` **before** the
first plain client send — a client-only lazy start binds `OT_COAP_DEFAULT_PORT` (5683)
first. When the last `OThreadCoAPClient` object is destroyed, its lazy-start hold on
the shared UDP service is released if no server still needs it.

## Native CoAP examples

| Folder | Sketches | Topic |
| --- | --- | --- |
| [`examples/Native/CoAP/CoAP_SimpleGet/`](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_SimpleGet) | `server`, `client` | Minimal GET |
| [`examples/Native/CoAP/CoAP_Light_Switch/`](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Light_Switch) | `light`, `switch` | Multicast lamp (CLI `coap_lamp` rewrite) |
| [`examples/Native/CoAP/CoAP_Sensor/`](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Sensor) | `sensor_server`, `sensor_client` | Telemetry polling (`serve()` + NON GET) |
| [`examples/Native/CoAP/CoAP_CRUD/`](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_CRUD) | `notes_server`, `notes_client` | REST collection via `OThreadCoAPResourceStore` |
| [`examples/Native/CoAP/CoAP_Secure/`](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Secure) | `secure_server`, `secure_client` | CoAPS with PSK |
| [`examples/Native/CoAP/CoAP_Greenhouse/`](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/CoAP/CoAP_Greenhouse) | `greenhouse_server`, `greenhouse_client` | Plain + CoAPS; NON telemetry + CON commands |

CLI-based CoAP demos remain under [`examples/CLI/COAP/`](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/COAP) for
reference; new application code should prefer the Native API examples above.

Full API details: [OpenThread CoAP documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/openthread/openthread_coap.html).

## Shutdown order

Before `OpenThread::end()`, stop application layers in reverse order of startup:
CoAP servers/clients (destroy client objects to release lazy UDP holds) →
`OThreadUDP.stop()` → `OThreadCLI.end()` → optional
`OThread.stop()` / `networkInterfaceDown()`. `OpenThread::end()` always stops
CLI and CoAP servers even if you skip those steps.

Examples: [`examples/CLI/StackShutdown/`](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/CLI/StackShutdown) (CLI-only)
and [`examples/Native/StackShutdown/`](https://github.com/espressif/arduino-esp32/tree/master/libraries/OpenThread/examples/Native/StackShutdown) (Native UDP +
CoAP `stop()` before `OThread.end()`, then `setup()` restart without chip reset).

Additional client/server options documented in the CoAP RST but not shown in every
example: `setPort()`, `useDefaultCoapRetransmit()`, and
`OThreadCoAPServer.leaveMulticastGroup()`.
