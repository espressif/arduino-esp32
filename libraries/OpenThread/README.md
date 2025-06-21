| Supported Targets | ESP32-C6 | ESP32-H2 |
| ----------------- | -------- | -------- |

# General View

This Arduino OpenThread Library allows using ESP OpenThread implementation using CLI and/or Native OpenThread API.

The Library implements 3 C++ Classes:
- `OThread` Class for Native OpenThread API
- `OThreadCLI` Class for CLI OpenThread API
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

It is important to note that the current implementation can only be used with Espressif SoC that has support to IEEE 802.15.4, such as **ESP32-C6** and **ESP32-H2**.

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
