| Supported Targets | ESP32-C6 | ESP32-H2 |
| ----------------- | -------- | -------- |

# ESP32 Arduino OpenThreadCLI

The `OpenThreadCLI` class is an Arduino API for interacting with the OpenThread Command Line Interface (CLI). It allows you to manage and configure the Thread stack using a command-line interface.

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
  bool otStarted = false;

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
- `startConsole(Stream& otStream, bool echoback = true, const char* prompt = "ot> ")`: Starts the OpenThread console with the specified stream, echoback option, and prompt.
- `stopConsole()`: Stops the OpenThread console.
- `setPrompt(char* prompt)`: Changes the console prompt (set to NULL for an empty prompt).
- `setEchoBack(bool echoback)`: Changes the console echoback option.
- `setStream(Stream& otStream)`: Changes the console Stream object.
- `onReceive(OnReceiveCb_t func)`: Sets a callback function to handle complete lines of output from the OT CLI.
- `begin(bool OThreadAutoStart = true)`: Initializes the OpenThread stack (optional auto-start).
- `end()`: Deinitializes the OpenThread stack.
- `setTxBufferSize(size_t tx_queue_len)`: Sets the transmit buffer size (default is 256 bytes).
- `setRxBufferSize(size_t rx_queue_len)`: Sets the receive buffer size (default is 1024 bytes).
- `write(uint8_t)`, `available()`, `read()`, `peek()`, `flush()`: Standard Stream methods implementation for OpenThread CLI object.
