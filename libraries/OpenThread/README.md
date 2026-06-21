| Supported Targets | ESP32-C5 | ESP32-C6 | ESP32-H2 |
| ----------------- | -------- | -------- | -------- |

# General View

This Arduino OpenThread Library allows using ESP OpenThread implementation using CLI and/or Native OpenThread API.

The Library implements 4 C++ Classes:
- `OThread` Class for Native OpenThread API
- `OThreadCLI` Class for CLI OpenThread API
- `OThreadUDP` Class for sending/receiving IPv6 UDP datagrams over the Thread network (raw `otUdpSocket`, no lwIP)
- `DataSet` Class for OpenThread dataset manipulation using Native `OThread` Class

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

    // Print network information (e.g., network name, channel, PAN ID) to the specified stream.
    static void otPrintNetworkInformation(Stream &output);

    OpenThread();
    ~OpenThread();

    // Returns true if the OpenThread stack is running.
    operator bool() const;

    // Initialize the OpenThread stack.
    static void begin(bool OThreadAutoStart = true);

    // Deinitialize the OpenThread stack.
    static void end();

    // Start the Thread network.
    void start();

    // Stop the Thread network.
    void stop();

    // Bring up the Thread network interface (equivalent to "ifconfig up").
    void networkInterfaceUp();

    // Bring down the Thread network interface (equivalent to "ifconfig down").
    void networkInterfaceDown();

    // Commit a dataset to the OpenThread instance.
    void commitDataSet(const DataSet &dataset);

    // Live setters (apply directly to the running instance)
    otError setChannel(uint8_t channel);
    otError setPanId(uint16_t panid);
    otError setExtendedPanId(const uint8_t *extpanid);
    otError setNetworkKey(const uint8_t *key);
    otError setNetworkName(const char *name);

    // Extra read-only getters
    bool   getEui64(uint8_t out[8]) const;
    String getEui64() const;
    const uint8_t *getExtendedAddress() const;
    uint16_t getThreadVersion() const;
    String   getVersionString() const;
    int8_t   getTxPower() const;
    uint32_t getPollPeriod() const;

    // Joiner (Thread Commissioning) - synchronous helper
    otError startJoiner(const char *pskd,
                        const char *provisioningUrl = nullptr,
                        const char *vendorName      = nullptr,
                        const char *vendorModel     = nullptr,
                        const char *vendorSwVersion = nullptr,
                        const char *vendorData      = nullptr,
                        uint32_t    timeoutMs       = 30000);
    void          stopJoiner();
    otJoinerState getJoinerState() const;
    const otExtAddress *getJoinerId() const;

private:
    static otInstance *mInstance; // Pointer to the OpenThread instance.
    DataSet mCurrentDataSet;      // Current dataset being used by the OpenThread instance.
};

extern OpenThread OThread;
```
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
### Network Information
- `otGetDeviceRole()`: Returns the current Thread device role as an `ot_device_role_t` enum (e.g., `OT_ROLE_LEADER`, `OT_ROLE_ROUTER`).
- `otGetStringDeviceRole()`: Returns the current Thread device role as a string (e.g., "Leader", "Router").
- `otPrintNetworkInformation(Stream &output)`: Prints the current network information (e.g., network name, channel, PAN ID) to the specified stream.

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
The  DataSet` class simplifies the management of Thread network datasets by providing intuitive methods for setting, retrieving, and applying network parameters. It abstracts the complexity of the OpenThread dataset APIs and provides Arduino-style methods for common operations.

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

** Notes
- The `DataSet` class is designed to work seamlessly with the `OThread` class for managing Thread network configurations.
- Ensure that the OpenThread stack is initialized (`OThread.begin()`) before applying a dataset.
- The  initNew()``  method provides default values for the dataset, which can be customized using the setter methods.

This documentation provides a comprehensive overview of the  `DataSet` class, its methods, and example usage. It is designed to help developers easily manage Thread network configurations in their Arduino projects.

# OpenThreadCLI Class

The `OpenThreadCLI` class is an Arduino API for interacting with the OpenThread Command Line Interface (CLI). It allows you to send commands to the OpenThread stack and receive responses. This class is designed to simplify the use of OpenThread CLI commands in Arduino sketches.

There is one main class called `OpenThreadCLI` and a global object used to operate OpenThread CLI, called `OThreadCLI`.\
Some [helper functions](helper_functions.md) were made available for working with the OpenThread CLI environment.

The available OpenThread Commands are documented in the [OpenThread CLI Reference Page](https://openthread.io/reference/cli/commands)

It is important to note that the current implementation can only be used with Espressif SoCs that have IEEE 802.15.4 radio support, such as **ESP32-C5**, **ESP32-C6** and **ESP32-H2**.

Below are the details of the class:

## Class Definition

```cpp
class OpenThreadCLI : public Stream {
private:
  static size_t setBuffer(QueueHandle_t &queue, size_t len);
  static bool otCLIStarted = false;

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

  void begin(bool OThreadAutoStart = true);
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
- `begin()`: Initializes the OpenThread stack (optional auto-start).
- `end()`: Deinitializes the OpenThread stack and releases resources.
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
- Ensure that the OpenThread stack is initialized (`OThreadCLI.begin()`) before starting the CLI console.

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
4. `OThread.addJoiner(PSKD, /*eui64=*/nullptr, /*timeoutSec=*/120)` - whitelists any joiner that knows `PSKD` for 120 s.
5. The companion Joiner device now runs `startJoiner(PSKD)` on its side; the dataset is delivered to it over an authenticated DTLS handshake.
6. (Optional) Call `OThread.stopCommissioner()` once you no longer want to admit new joiners.

## API

```cpp
otError startCommissioner(uint32_t timeoutMs = 30000);
otError addJoiner(const char *pskd,
                  const otExtAddress *eui64 = nullptr,
                  uint32_t timeoutSec = 120);
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

- [`CommissionerNode`](examples/Native/ThreadCommissioning/CommissionerNode/CommissionerNode.ino) builds a fresh DataSet, forms the network, then runs the Commissioner.
- [`JoinerNode`](examples/Native/ThreadCommissioning/JoinerNode/JoinerNode.ino) has **no** local DataSet, only the PSKd, and uses `startJoiner()` to obtain the dataset over the air.

# OThreadUDP Class - IPv6 UDP over Thread

`OThreadUDP` is an Arduino `UDP` implementation backed directly by the `otUdpSocket` API. It does not go through lwIP, so it is the lightest path to send/receive IPv6 UDP datagrams over the Thread mesh.

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

`examples/Native/UDP_Light_Switch/` contains a complete two-board demo that combines the Joiner / Commissioner flow with `OThreadUDP` to build a controllable IoT lamp:

- [`light`](examples/Native/UDP_Light_Switch/light/light.ino) is the **server**. It boots as Thread Leader, resumes the persisted dataset when available, runs the **Commissioner** role (accepts joiners that present PSKd `J01NME`) and subscribes to the realm-local multicast group `ff03::abcd` on UDP port `5051`. For every `ON` / `OFF` / `TOGGLE` / `STATUS` command it receives, it updates the on-board RGB LED ("lamp") and unicasts an `ACK ON` / `ACK OFF` confirmation back to the sender.
- [`switch`](examples/Native/UDP_Light_Switch/switch/switch.ino) is the **client**. It boots **without** any local DataSet, runs the **Joiner** state machine, and once attached opens a UDP socket on the same port. Pressing the BOOT button sends `TOGGLE` to `ff03::abcd` on UDP port `5051` and waits for the light's confirmation packet within 1 s.

One light can serve **many switches** in parallel - just flash `switch.ino` on additional H2 / C6 / C5 boards within the joining window. The pair demonstrates an end-to-end IoT pattern (commissioning + many-to-one multicast control + per-command acknowledgment) using only the Native OpenThread API. Port `5051` is used to avoid OpenThread-reserved CoAP ports such as `5683`, `5684`, and `61631`.

For a telemetry-oriented deployment with many sleepy sensors, see:

- [`examples/Native/UDP_SensorNetwork/sensor_collector/sensor_collector.ino`](examples/Native/UDP_SensorNetwork/sensor_collector/sensor_collector.ino): leader + commissioner + UDP sink that tracks up to 256 unique node IDs and prints periodic fleet status.
- [`examples/Native/UDP_SensorNetwork/sensor_node/sensor_node.ino`](examples/Native/UDP_SensorNetwork/sensor_node/sensor_node.ino): joiner sensor client with optional Sleepy End Device behavior (`otLinkSetRxOnWhenIdle(false)`, poll period, child timeout) that periodically uploads readings over UDP.
