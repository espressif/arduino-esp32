#############
Serial (UART)
#############

About
-----
The Serial (UART - Universal Asynchronous Receiver-Transmitter) peripheral provides asynchronous serial communication,
allowing the ESP32 to communicate with other devices such as computers, sensors, displays, and other microcontrollers.

UART is a simple, two-wire communication protocol that uses a TX (transmit) and RX (receive) line for full-duplex communication.
The ESP32 Arduino implementation provides a HardwareSerial class that is compatible with the standard Arduino Serial API,
with additional features for advanced use cases.

**Key Features:**

* **Full-duplex communication**: Simultaneous transmission and reception
* **Configurable baud rates**: From 300 to 5,000,000+ baud
* **Multiple data formats**: Configurable data bits, parity, and stop bits
* **Hardware flow control**: Support for RTS/CTS signals
* **RS485 support**: Half-duplex RS485 communication mode
* **Low-power UART**: Some SoCs support LP (Low-Power) UART for ultra-low power applications
* **Baud rate detection**: Automatic baud rate detection (ESP32, ESP32-S2 only)
* **Event callbacks**: Receive and error event callbacks
* **Configurable buffers**: Adjustable RX and TX buffer sizes

.. note::
   In case that both pins, RX and TX are detached from UART, the driver will be stopped.
   Detaching may occur when, for instance, starting another peripheral using RX and TX pins, such as Wire.begin(RX0, TX0).

UART Availability
-----------------

The number of UART peripherals available varies by ESP32 SoC:

========= ======== ========
ESP32 SoC HP UARTs LP UARTs
========= ======== ========
ESP32     3        0
ESP32-S2  2        0
ESP32-S3  3        0
ESP32-C3  2        0
ESP32-C5  2        1
ESP32-C6  2        1
ESP32-H2  2        0
ESP32-P4  5        1
========= ======== ========

**Note:**
* HP (High-Performance) UARTs are the standard UART peripherals
* LP (Low-Power) UARTs are available on some SoCs for ultra-low power applications
* UART0 is typically used for programming and debug output (Serial Monitor)
* Additional UARTs (Serial1, Serial2, etc.) are available for general-purpose communication, including LP UARTs when available. The ESP32 Arduino Core automatically creates HardwareSerial objects for all available UARTs:

  * ``Serial0`` (or ``Serial``) - UART0 (HP UART, typically used for programming and debug output)
  * ``Serial1``, ``Serial2``, etc. - Additional HP UARTs (numbered sequentially)
  * Additional Serial objects - LP UARTs, when available (numbered after HP UARTs)

  **Example:** The ESP32-C6 has 2 HP UARTs and 1 LP UART. The Arduino Core creates ``Serial0`` and ``Serial1`` (HP UARTs) plus ``Serial2`` (LP UART) HardwareSerial objects.

  **Important:** LP UARTs can be used as regular UART ports, but they have fixed GPIO pins for RX, TX, CTS, and RTS. It is not possible to change the pins for LP UARTs using ``setPins()``.

Arduino-ESP32 Serial API
------------------------

begin
*****

Initializes the Serial port with the specified baud rate and configuration.

.. code-block:: arduino

    void begin(unsigned long baud, uint32_t config = SERIAL_8N1, int8_t rxPin = -1, int8_t txPin = -1, bool invert = false, unsigned long timeout_ms = 20000UL, uint8_t rxfifo_full_thrhd = 120);

* ``baud`` - Baud rate (bits per second). Common values: 9600, 115200, 230400, etc.

  **Special value:** ``0`` enables baud rate detection (ESP32, ESP32-S2 only). The function will attempt to detect the baud rate for up to ``timeout_ms`` milliseconds. See the :ref:`Baud Rate Detection Example <baud-rate-detection-example>` for usage details.
* ``config`` - Serial configuration (data bits, parity, stop bits):

  * ``SERIAL_8N1`` - 8 data bits, no parity, 1 stop bit (default)
  * ``SERIAL_8N2`` - 8 data bits, no parity, 2 stop bits
  * ``SERIAL_8E1`` - 8 data bits, even parity, 1 stop bit
  * ``SERIAL_8E2`` - 8 data bits, even parity, 2 stop bits
  * ``SERIAL_8O1`` - 8 data bits, odd parity, 1 stop bit
  * ``SERIAL_8O2`` - 8 data bits, odd parity, 2 stop bits
  * ``SERIAL_7N1``, ``SERIAL_7N2``, ``SERIAL_7E1``, ``SERIAL_7E2``, ``SERIAL_7O1``, ``SERIAL_7O2`` - 7 data bits variants
  * ``SERIAL_6N1``, ``SERIAL_6N2``, ``SERIAL_6E1``, ``SERIAL_6E2``, ``SERIAL_6O1``, ``SERIAL_6O2`` - 6 data bits variants
  * ``SERIAL_5N1``, ``SERIAL_5N2``, ``SERIAL_5E1``, ``SERIAL_5E2``, ``SERIAL_5O1``, ``SERIAL_5O2`` - 5 data bits variants

* ``rxPin`` - RX pin number. Use ``-1`` to keep the default pin or current pin assignment.

* ``txPin`` - TX pin number. Use ``-1`` to keep the default pin or current pin assignment.

* ``invert`` - If ``true``, inverts the RX and TX signal polarity.

* ``timeout_ms`` - Timeout in milliseconds for baud rate detection (when ``baud = 0``). Default: 20000 ms (20 seconds).

* ``rxfifo_full_thrhd`` - RX FIFO full threshold (1-127 bytes). When the FIFO reaches this threshold, data is copied to the RX buffer. Default: 120 bytes.

**Example:**

.. code-block:: arduino

    // Basic initialization with default pins
    Serial.begin(115200);

    // Initialize with custom pins
    Serial1.begin(9600, SERIAL_8N1, 4, 5);

    // Initialize with baud rate detection (ESP32, ESP32-S2 only)
    Serial.begin(0, SERIAL_8N1, -1, -1, false, 20000);

end
***

Stops the Serial port and releases all resources.

.. code-block:: arduino

    void end(void);

This function disables the UART peripheral and frees all associated resources.

available
*********

Returns the number of bytes available for reading from the Serial port.

.. code-block:: arduino

    int available(void);

**Returns:** The number of bytes available in the RX buffer, or ``0`` if no data is available.

**Example:**

.. code-block:: arduino

    if (Serial.available() > 0) {
        char data = Serial.read();
    }

availableForWrite
*****************

Returns the number of bytes that can be written to the Serial port without blocking.

.. code-block:: arduino

    int availableForWrite(void);

**Returns:** The number of bytes that can be written to the TX buffer without blocking.

read
****

Reads a single byte from the Serial port.

.. code-block:: arduino

    int read(void);

**Returns:** The byte read (0-255), or ``-1`` if no data is available.

**Example:**

.. code-block:: arduino

    int data = Serial.read();
    if (data != -1) {
        Serial.printf("Received: %c\n", data);
    }

read (buffer)
*************

Reads multiple bytes from the Serial port into a buffer.

.. code-block:: arduino

    size_t read(uint8_t *buffer, size_t size);
    size_t read(char *buffer, size_t size);

* ``buffer`` - Pointer to the buffer where data will be stored
* ``size`` - Maximum number of bytes to read

**Returns:** The number of bytes actually read.

**Example:**

.. code-block:: arduino

    uint8_t buffer[64];
    size_t bytesRead = Serial.read(buffer, sizeof(buffer));
    Serial.printf("Read %d bytes\n", bytesRead);

readBytes
*********

Reads multiple bytes from the Serial port, blocking until the specified number of bytes is received or timeout occurs.

.. code-block:: arduino

    size_t readBytes(uint8_t *buffer, size_t length);
    size_t readBytes(char *buffer, size_t length);

* ``buffer`` - Pointer to the buffer where data will be stored
* ``length`` - Number of bytes to read

**Returns:** The number of bytes actually read (may be less than ``length`` if timeout occurs).

**Note:** This function overrides ``Stream::readBytes()`` for better performance using ESP-IDF functions.

write
*****

Writes data to the Serial port.

.. code-block:: arduino

    size_t write(uint8_t);
    size_t write(const uint8_t *buffer, size_t size);
    size_t write(const char *buffer, size_t size);
    size_t write(const char *s);
    size_t write(unsigned long n);
    size_t write(long n);
    size_t write(unsigned int n);
    size_t write(int n);

* Single byte: ``write(uint8_t)`` - Writes a single byte
* Buffer: ``write(buffer, size)`` - Writes multiple bytes from a buffer
* String: ``write(const char *s)`` - Writes a null-terminated string
* Number: ``write(n)`` - Writes a number as a single byte

**Returns:** The number of bytes written.

**Example:**

.. code-block:: arduino

    Serial.write('A');
    Serial.write("Hello");
    Serial.write(buffer, 10);
    Serial.write(65);  // Writes byte value 65

peek
****

Returns the next byte in the RX buffer without removing it.

.. code-block:: arduino

    int peek(void);

**Returns:** The next byte (0-255), or ``-1`` if no data is available.

**Note:** Unlike ``read()``, ``peek()`` does not remove the byte from the buffer.

flush
*****

Waits for all data in the TX buffer to be transmitted.

.. code-block:: arduino

    void flush(void);
    void flush(bool txOnly);

* ``txOnly`` - If ``true``, only flushes the TX buffer. If ``false`` (default), also clears the RX buffer.

**Note:** This function blocks until all data in the TX buffer has been sent.

baudRate
********

Returns the current baud rate of the Serial port.

.. code-block:: arduino

    uint32_t baudRate(void);

**Returns:** The configured baud rate in bits per second.

**Note:** When using baud rate detection (``begin(0)``), this function returns the detected baud rate, which may be slightly rounded (e.g., 115200 may return 115201).

updateBaudRate
**************

Updates the baud rate of an already initialized Serial port.

.. code-block:: arduino

    void updateBaudRate(unsigned long baud);

* ``baud`` - New baud rate

**Note:** This function can be called after ``begin()`` to change the baud rate without reinitializing the port.

setPins
*******

Sets or changes the RX, TX, CTS, and RTS pins for the Serial port.

.. code-block:: arduino

    bool setPins(int8_t rxPin, int8_t txPin, int8_t ctsPin = -1, int8_t rtsPin = -1);

* ``rxPin`` - RX pin number. Use ``-1`` to keep current pin.
* ``txPin`` - TX pin number. Use ``-1`` to keep current pin.
* ``ctsPin`` - CTS (Clear To Send) pin for hardware flow control. Use ``-1`` to keep current pin or disable.
* ``rtsPin`` - RTS (Request To Send) pin for hardware flow control. Use ``-1`` to keep current pin or disable.

**Returns:** ``true`` if pins are set successfully, ``false`` otherwise.

**Note:** This function can be called before or after ``begin()``. When pins are changed, the previous pins are automatically detached.

setRxBufferSize
***************

Sets the size of the RX buffer.

.. code-block:: arduino

    size_t setRxBufferSize(size_t new_size);

* ``new_size`` - New RX buffer size in bytes

**Returns:** The actual buffer size set, or ``0`` on error.

**Note:** This function must be called **before** ``begin()`` to take effect. Default RX buffer size is 256 bytes.

setTxBufferSize
***************

Sets the size of the TX buffer.

.. code-block:: arduino

    size_t setTxBufferSize(size_t new_size);

* ``new_size`` - New TX buffer size in bytes

**Returns:** The actual buffer size set, or ``0`` on error.

**Note:** This function must be called **before** ``begin()`` to take effect. Default TX buffer size is 0 (no buffering).

setRxTimeout
************

Sets the RX timeout threshold in UART symbol periods.

.. code-block:: arduino

    bool setRxTimeout(uint8_t symbols_timeout);

* ``symbols_timeout`` - Timeout threshold in UART symbol periods. Setting ``0`` disables timeout-based callbacks.

  The timeout is calculated based on the current baud rate and serial configuration. For example:

  * For ``SERIAL_8N1`` (10 bits per symbol), a timeout of 3 symbols at 9600 baud = 3 / (9600 / 10) = 3.125 ms
  * Maximum timeout is calculated automatically by ESP-IDF based on the serial configuration

**Returns:** ``true`` if timeout is set successfully, ``false`` otherwise.

**Note:**
* When RX timeout occurs, the ``onReceive()`` callback is triggered
* For ESP32 and ESP32-S2, when using REF_TICK clock source (baud rates ≤ 250000), RX timeout is limited to 1 symbol
* To use higher RX timeout values on ESP32/ESP32-S2, set the clock source to APB using ``setClockSource(UART_CLK_SRC_APB)`` before ``begin()``

setRxFIFOFull
*************

Sets the RX FIFO full threshold that triggers data transfer from FIFO to RX buffer.

.. code-block:: arduino

    bool setRxFIFOFull(uint8_t fifoBytes);

* ``fifoBytes`` - Number of bytes (1-127) that will trigger the FIFO full interrupt

  When the UART FIFO reaches this threshold, data is copied to the RX buffer and the ``onReceive()`` callback is triggered.

**Returns:** ``true`` if threshold is set successfully, ``false`` otherwise.

**Note:**
* Lower values (e.g., 1) provide byte-by-byte reception but consume more CPU time
* Higher values (e.g., 120) provide better performance but introduce latency
* Default value depends on baud rate: 1 byte for ≤ 115200 baud, 120 bytes for > 115200 baud

onReceive
*********

Sets a callback function that is called when data is received.

.. code-block:: arduino

    void onReceive(OnReceiveCb function, bool onlyOnTimeout = false);

* ``function`` - Callback function to call when data is received. Use ``NULL`` to disable the callback.
* ``onlyOnTimeout`` - If ``true``, callback is only called on RX timeout. If ``false`` (default), callback is called on both FIFO full and RX timeout events.

**Callback Signature:**

.. code-block:: arduino

    typedef std::function<void(void)> OnReceiveCb;

**Note:**
* When ``onlyOnTimeout = false``, the callback is triggered when FIFO reaches the threshold (set by ``setRxFIFOFull()``) or on RX timeout
* When ``onlyOnTimeout = true``, the callback is only triggered on RX timeout, ensuring all data in a stream is available at once
* Using ``onlyOnTimeout = true`` may cause RX overflow if the RX buffer size is too small for the incoming data stream
* The callback is executed in a separate task, allowing non-blocking data processing

**Example:**

.. code-block:: arduino

    void onReceiveCallback() {
        while (Serial1.available()) {
            char c = Serial1.read();
            Serial.print(c);
        }
    }

    void setup() {
        Serial1.begin(115200);
        Serial1.onReceive(onReceiveCallback);
    }

onReceiveError
**************

Sets a callback function that is called when a UART error occurs.

.. code-block:: arduino

    void onReceiveError(OnReceiveErrorCb function);

* ``function`` - Callback function to call when an error occurs. Use ``NULL`` to disable the callback.

**Callback Signature:**

.. code-block:: arduino

    typedef std::function<void(hardwareSerial_error_t)> OnReceiveErrorCb;

**Error Types:**

* ``UART_NO_ERROR`` - No error
* ``UART_BREAK_ERROR`` - Break condition detected
* ``UART_BUFFER_FULL_ERROR`` - RX buffer is full
* ``UART_FIFO_OVF_ERROR`` - UART FIFO overflow
* ``UART_FRAME_ERROR`` - Frame error (invalid stop bit)
* ``UART_PARITY_ERROR`` - Parity error

**Example:**

.. code-block:: arduino

    void onErrorCallback(hardwareSerial_error_t error) {
        Serial.printf("UART Error: %d\n", error);
    }

    void setup() {
        Serial1.begin(115200);
        Serial1.onReceiveError(onErrorCallback);
    }

eventQueueReset
***************

Clears all events in the event queue (events that trigger ``onReceive()`` and ``onReceiveError()``).

.. code-block:: arduino

    void eventQueueReset(void);

This function can be useful in some use cases where you want to clear pending events.

setHwFlowCtrlMode
*****************

Enables or disables hardware flow control using RTS and/or CTS pins.

.. code-block:: arduino

    bool setHwFlowCtrlMode(SerialHwFlowCtrl mode = UART_HW_FLOWCTRL_CTS_RTS, uint8_t threshold = 64);

* ``mode`` - Hardware flow control mode:

  * ``UART_HW_FLOWCTRL_DISABLE`` (0x0) - Disable hardware flow control
  * ``UART_HW_FLOWCTRL_RTS`` (0x1) - Enable RX hardware flow control (RTS)
  * ``UART_HW_FLOWCTRL_CTS`` (0x2) - Enable TX hardware flow control (CTS)
  * ``UART_HW_FLOWCTRL_CTS_RTS`` (0x3) - Enable full hardware flow control (default)

* ``threshold`` - Flow control threshold (default: 64, which is half of the FIFO length)

**Returns:** ``true`` if flow control mode is set successfully, ``false`` otherwise.

**Note:** CTS and RTS pins must be set using ``setPins()`` before enabling hardware flow control.

setMode
*******

Sets the UART operating mode.

.. code-block:: arduino

    bool setMode(SerialMode mode);

* ``mode`` - UART mode:

  * ``UART_MODE_UART`` (0x00) - Regular UART mode (default)
  * ``UART_MODE_RS485_HALF_DUPLEX`` (0x01) - Half-duplex RS485 mode (RTS pin controls transceiver)
  * ``UART_MODE_IRDA`` (0x02) - IRDA UART mode
  * ``UART_MODE_RS485_COLLISION_DETECT`` (0x03) - RS485 collision detection mode (for testing)
  * ``UART_MODE_RS485_APP_CTRL`` (0x04) - Application-controlled RS485 mode (for testing)

**Returns:** ``true`` if mode is set successfully, ``false`` otherwise.

**Note:** For RS485 half-duplex mode, the RTS pin must be configured using ``setPins()`` to control the transceiver.

setClockSource
**************

Sets the UART clock source. Must be called **before** ``begin()`` to take effect.

.. code-block:: arduino

    bool setClockSource(SerialClkSrc clkSrc);

* ``clkSrc`` - Clock source:

  * ``UART_CLK_SRC_DEFAULT`` - Default clock source (varies by SoC)
  * ``UART_CLK_SRC_APB`` - APB clock (ESP32, ESP32-S2, ESP32-C3, ESP32-S3)
  * ``UART_CLK_SRC_PLL`` - PLL clock (ESP32-C2, ESP32-C5, ESP32-C6, ESP32-C61, ESP32-H2, ESP32-P4)
  * ``UART_CLK_SRC_XTAL`` - XTAL clock (ESP32-C2, ESP32-C3, ESP32-C5, ESP32-C6, ESP32-C61, ESP32-H2, ESP32-S3, ESP32-P4)
  * ``UART_CLK_SRC_RTC`` - RTC clock (ESP32-C2, ESP32-C3, ESP32-C5, ESP32-C6, ESP32-C61, ESP32-H2, ESP32-S3, ESP32-P4)
  * ``UART_CLK_SRC_REF_TICK`` - REF_TICK clock (ESP32, ESP32-S2)

**Note:**
* Clock source availability varies by SoC.
* PLL frequency varies by SoC: ESP32-C2 (40 MHz), ESP32-H2 (48 MHz), ESP32-C5/C6/C61/P4 (80 MHz).
* ESP32-C5, ESP32-C6, ESP32-C61, and ESP32-P4 have LP UART that uses only RTC_FAST or XTAL/2 as clock source.
* For ESP32 and ESP32-S2, REF_TICK is used by default for baud rates ≤ 250000 to avoid baud rate changes when CPU frequency changes, but this limits RX timeout to 1 symbol.

**Returns:** ``true`` if clock source is set successfully, ``false`` otherwise.

setRxInvert
***********

Enables or disables RX signal inversion.

.. code-block:: arduino

    bool setRxInvert(bool invert);

* ``invert`` - If ``true``, inverts the RX signal polarity

**Returns:** ``true`` if inversion is set successfully, ``false`` otherwise.

setTxInvert
***********

Enables or disables TX signal inversion.

.. code-block:: arduino

    bool setTxInvert(bool invert);

* ``invert`` - If ``true``, inverts the TX signal polarity

**Returns:** ``true`` if inversion is set successfully, ``false`` otherwise.

setCtsInvert
************

Enables or disables CTS signal inversion.

.. code-block:: arduino

    bool setCtsInvert(bool invert);

* ``invert`` - If ``true``, inverts the CTS signal polarity

**Returns:** ``true`` if inversion is set successfully, ``false`` otherwise.

setRtsInvert
************

Enables or disables RTS signal inversion.

.. code-block:: arduino

    bool setRtsInvert(bool invert);

* ``invert`` - If ``true``, inverts the RTS signal polarity

**Returns:** ``true`` if inversion is set successfully, ``false`` otherwise.

setDebugOutput
**************

Enables or disables debug output on this Serial port.

.. code-block:: arduino

    void setDebugOutput(bool enable);

* ``enable`` - If ``true``, enables debug output (ESP-IDF log messages will be sent to this Serial port)

**Note:** By default, debug output is sent to UART0 (Serial0).

operator bool
*************

Returns whether the Serial port is initialized and ready.

.. code-block:: arduino

    operator bool() const;

**Returns:** ``true`` if the Serial port is initialized, ``false`` otherwise.

**Example:**

.. code-block:: arduino

    Serial1.begin(115200);
    while (!Serial1) {
        delay(10);  // Wait for Serial1 to be ready
    }

Serial Configuration Constants
------------------------------

The following constants are used for serial configuration in the ``begin()`` function:

Data Bits, Parity, Stop Bits
****************************

* ``SERIAL_5N1``, ``SERIAL_5N2``, ``SERIAL_5E1``, ``SERIAL_5E2``, ``SERIAL_5O1``, ``SERIAL_5O2`` - 5 data bits
* ``SERIAL_6N1``, ``SERIAL_6N2``, ``SERIAL_6E1``, ``SERIAL_6E2``, ``SERIAL_6O1``, ``SERIAL_6O2`` - 6 data bits
* ``SERIAL_7N1``, ``SERIAL_7N2``, ``SERIAL_7E1``, ``SERIAL_7E2``, ``SERIAL_7O1``, ``SERIAL_7O2`` - 7 data bits
* ``SERIAL_8N1``, ``SERIAL_8N2``, ``SERIAL_8E1``, ``SERIAL_8E2``, ``SERIAL_8O1``, ``SERIAL_8O2`` - 8 data bits

Where:
* First number = data bits (5, 6, 7, or 8)
* Letter = parity: N (None), E (Even), O (Odd)
* Last number = stop bits (1 or 2)

Example Applications
********************

.. _baud-rate-detection-example:

Baud Rate Detection Example:

.. literalinclude:: ../../../libraries/ESP32/examples/Serial/BaudRateDetect_Demo/BaudRateDetect_Demo.ino
    :language: arduino

OnReceive Callback Example:

.. literalinclude:: ../../../libraries/ESP32/examples/Serial/OnReceive_Demo/OnReceive_Demo.ino
    :language: arduino

RS485 Communication Example:

.. literalinclude:: ../../../libraries/ESP32/examples/Serial/RS485_Echo_Demo/RS485_Echo_Demo.ino
    :language: arduino

Complete list of `Serial examples <https://github.com/espressif/arduino-esp32/tree/master/libraries/ESP32/examples/Serial>`_.
