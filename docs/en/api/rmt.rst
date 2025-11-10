####################
Remote Control (RMT)
####################

About
-----
The Remote Control Transceiver (RMT) peripheral was originally designed to act as an infrared transceiver,
but it can be used to generate or receive many other types of digital signals with precise timing.

The RMT peripheral is capable of transmitting and receiving digital signals with precise timing control,
making it ideal for protocols that require specific pulse widths and timing, such as:

* **Infrared (IR) remote control protocols** (NEC, RC5, Sony, etc.)
* **WS2812/NeoPixel RGB LED control** (requires precise timing)
* **Custom communication protocols** with specific timing requirements
* **Pulse generation** for various applications

RMT operates by encoding digital signals as sequences of high/low pulses with specific durations.
Each RMT symbol represents two consecutive pulses (level0/duration0 and level1/duration1).

RMT Memory Blocks
-----------------

RMT channels use memory blocks to store signal data. The number of available memory blocks varies by SoC:

========= ================== ======================================
ESP32 SoC Memory Blocks      Notes
========= ================== ======================================
ESP32     8 blocks total     Shared between TX and RX channels
ESP32-S2  4 blocks total     Shared between TX and RX channels
ESP32-S3  4 blocks TX + 4 RX Separate memory for TX and RX channels
ESP32-C3  2 blocks TX + 2 RX Separate memory for TX and RX channels
ESP32-C5  2 blocks TX + 2 RX Separate memory for TX and RX channels
ESP32-C6  2 blocks TX + 2 RX Separate memory for TX and RX channels
ESP32-H2  2 blocks TX + 2 RX Separate memory for TX and RX channels
ESP32-P4  4 blocks TX + 4 RX Separate memory for TX and RX channels
========= ================== ======================================

Each memory block can store ``RMT_SYMBOLS_PER_CHANNEL_BLOCK`` symbols (64 for ESP32/ESP32-S2, 48 for ESP32-S3/ESP32-C3/ESP32-C5/ESP32-C6/ESP32-H2/ESP32-P4).

**Note:** Each RMT symbol is 4 bytes (32 bits), containing two pulses with their durations and levels.

Arduino-ESP32 RMT API
---------------------

rmtInit
*******

Initializes an RMT channel for a specific GPIO pin with the specified direction, memory size, and frequency.

.. code-block:: arduino

    bool rmtInit(int pin, rmt_ch_dir_t channel_direction, rmt_reserve_memsize_t memsize, uint32_t frequency_Hz);

* ``pin`` - GPIO pin number to use for RMT
* ``channel_direction`` - Channel direction:
  
  * ``RMT_RX_MODE`` - Receive mode (for reading signals)
  * ``RMT_TX_MODE`` - Transmit mode (for sending signals)

* ``memsize`` - Number of memory blocks to reserve for this channel:
  
  * ``RMT_MEM_NUM_BLOCKS_1`` - 1 block
  * ``RMT_MEM_NUM_BLOCKS_2`` - 2 blocks
  * ``RMT_MEM_NUM_BLOCKS_3`` - 3 blocks (ESP32 only)
  * ``RMT_MEM_NUM_BLOCKS_4`` - 4 blocks (ESP32 only)
  * ``RMT_MEM_NUM_BLOCKS_5`` through ``RMT_MEM_NUM_BLOCKS_8`` - 5-8 blocks (ESP32 only)

* ``frequency_Hz`` - RMT channel frequency in Hz (tick frequency). Must be between 312.5 kHz and 80 MHz.
  
  The frequency determines the resolution of pulse durations. For example:
  
  * 10 MHz (100 ns tick) - High precision, suitable for WS2812 LEDs
  * 1 MHz (1 µs tick) - Good for most IR protocols
  * 400 kHz (2.5 µs tick) - Suitable for slower protocols

This function returns ``true`` if initialization is successful, ``false`` otherwise.

**Note:** The RMT tick is set by the frequency parameter. Example: 100 ns tick => 10 MHz, thus frequency will be 10,000,000 Hz.

rmtDeinit
*********

Deinitializes the RMT channel and releases all allocated resources for the specified pin.

.. code-block:: arduino

    bool rmtDeinit(int pin);

* ``pin`` - GPIO pin number that was initialized with RMT

This function returns ``true`` if deinitialization is successful, ``false`` otherwise.

rmtSetEOT
*********

Sets the End of Transmission (EOT) level for the RMT pin when transmission ends.
This function affects how ``rmtWrite()``, ``rmtWriteAsync()``, or ``rmtWriteLooping()`` will set the pin after writing the data.

.. code-block:: arduino

    bool rmtSetEOT(int pin, uint8_t EOT_Level);

* ``pin`` - GPIO pin number configured for RMT TX mode
* ``EOT_Level`` - End of transmission level:
  
  * ``0`` (LOW) - Pin will be set to LOW after transmission (default)
  * Non-zero (HIGH) - Pin will be set to HIGH after transmission

**Note:** This only affects the transmission process. The pre-transmission idle level can be set manually using ``digitalWrite(pin, level)``.

This function returns ``true`` if EOT level is set successfully, ``false`` otherwise.

rmtWrite
********

Sends RMT data in blocking mode. The function waits until all data is transmitted or until timeout occurs.

.. code-block:: arduino

    bool rmtWrite(int pin, rmt_data_t *data, size_t num_rmt_symbols, uint32_t timeout_ms);

* ``pin`` - GPIO pin number configured for RMT TX mode
* ``data`` - Pointer to array of ``rmt_data_t`` symbols to transmit
* ``num_rmt_symbols`` - Number of RMT symbols to transmit
* ``timeout_ms`` - Timeout in milliseconds. Use ``RMT_WAIT_FOR_EVER`` for indefinite wait

**Blocking mode:** The function only returns after sending all data or by timeout.

This function returns ``true`` if transmission is successful, ``false`` on error or timeout.

**Example:**

.. code-block:: arduino

    rmt_data_t symbols[] = {
        {8, 1, 4, 0},  // High for 8 ticks, Low for 4 ticks (bit '1')
        {4, 1, 8, 0}   // High for 4 ticks, Low for 8 ticks (bit '0')
    };
    
    rmtWrite(pin, symbols, 2, RMT_WAIT_FOR_EVER);

rmtWriteAsync
*************

Sends RMT data in non-blocking (asynchronous) mode. The function returns immediately after starting the transmission.

.. code-block:: arduino

    bool rmtWriteAsync(int pin, rmt_data_t *data, size_t num_rmt_symbols);

* ``pin`` - GPIO pin number configured for RMT TX mode
* ``data`` - Pointer to array of ``rmt_data_t`` symbols to transmit
* ``num_rmt_symbols`` - Number of RMT symbols to transmit

**Non-blocking mode:** Returns immediately after execution. Use ``rmtTransmitCompleted()`` to check if transmission is finished.

**Note:** If more than one ``rmtWriteAsync()`` is called in sequence, it will wait for the first transmission to finish, returning ``false`` to indicate failure.

This function returns ``true`` on execution success, ``false`` otherwise.

rmtWriteLooping
****************

Sends RMT data in infinite looping mode. The data will be transmitted continuously until stopped.

.. code-block:: arduino

    bool rmtWriteLooping(int pin, rmt_data_t *data, size_t num_rmt_symbols);

* ``pin`` - GPIO pin number configured for RMT TX mode
* ``data`` - Pointer to array of ``rmt_data_t`` symbols to transmit
* ``num_rmt_symbols`` - Number of RMT symbols to transmit

**Looping mode:** The data is transmitted continuously in a loop. To stop looping, call ``rmtWrite()`` or ``rmtWriteAsync()`` with new data, or call ``rmtWriteLooping()`` with ``NULL`` data or zero size.

**Note:** Looping mode needs a zero-ending data symbol ``{0, 0, 0, 0}`` to mark the end of data.

This function returns ``true`` on execution success, ``false`` otherwise.

rmtWriteRepeated
****************

Sends RMT data a fixed number of times (repeated transmission).

.. code-block:: arduino

    bool rmtWriteRepeated(int pin, rmt_data_t *data, size_t num_rmt_symbols, uint32_t loop_count);

* ``pin`` - GPIO pin number configured for RMT TX mode
* ``data`` - Pointer to array of ``rmt_data_t`` symbols to transmit
* ``num_rmt_symbols`` - Number of RMT symbols to transmit
* ``loop_count`` - Number of times to repeat the transmission (must be at least 1)

**Note:** 
* ``loop_count == 0`` is invalid (no transmission)
* ``loop_count == 1`` transmits once (no looping)
* ``loop_count > 1`` transmits the data repeatedly

**Note:** Loop count feature is only supported on certain SoCs. On unsupported SoCs, this function will return ``false``.

This function returns ``true`` on execution success, ``false`` otherwise.

rmtTransmitCompleted
********************

Checks if the RMT transmission is completed and the channel is ready for transmitting new data.

.. code-block:: arduino

    bool rmtTransmitCompleted(int pin);

* ``pin`` - GPIO pin number configured for RMT TX mode

This function returns ``true`` when all data has been sent and the channel is ready for a new transmission, ``false`` otherwise.

**Note:** 
* If ``rmtWrite()`` times out or ``rmtWriteAsync()`` is called, this function will return ``false`` until all data is sent out.
* ``rmtTransmitCompleted()`` will always return ``true`` when ``rmtWriteLooping()`` is active, because looping mode never completes.

rmtRead
*******

Initiates blocking receive operation. Reads RMT data and stores it in the provided buffer.

.. code-block:: arduino

    bool rmtRead(int pin, rmt_data_t *data, size_t *num_rmt_symbols, uint32_t timeout_ms);

* ``pin`` - GPIO pin number configured for RMT RX mode
* ``data`` - Pointer to buffer where received RMT symbols will be stored
* ``num_rmt_symbols`` - Pointer to variable containing maximum number of symbols to read. 
  
  On return, this variable will contain the actual number of symbols read.
  
* ``timeout_ms`` - Timeout in milliseconds. Use ``RMT_WAIT_FOR_EVER`` for indefinite wait

**Blocking mode:** The function waits until data is received or timeout occurs.

If the reading operation times out, ``num_rmt_symbols`` won't change and ``rmtReceiveCompleted()`` can be used later to check if data is available.

This function returns ``true`` when data is successfully read, ``false`` on error or timeout.

rmtReadAsync
************

Initiates non-blocking (asynchronous) receive operation. Returns immediately after starting the receive process.

.. code-block:: arduino

    bool rmtReadAsync(int pin, rmt_data_t *data, size_t *num_rmt_symbols);

* ``pin`` - GPIO pin number configured for RMT RX mode
* ``data`` - Pointer to buffer where received RMT symbols will be stored
* ``num_rmt_symbols`` - Pointer to variable containing maximum number of symbols to read.
  
  On completion, this variable will be updated with the actual number of symbols read.

**Non-blocking mode:** Returns immediately after execution. Use ``rmtReceiveCompleted()`` to check if data is available.

This function returns ``true`` on execution success, ``false`` otherwise.

rmtReceiveCompleted
*******************

Checks if RMT data reception is completed and new data is available for processing.

.. code-block:: arduino

    bool rmtReceiveCompleted(int pin);

* ``pin`` - GPIO pin number configured for RMT RX mode

This function returns ``true`` when data has been received and is available in the buffer, ``false`` otherwise.

**Note:** The data reception information is reset when a new ``rmtRead()`` or ``rmtReadAsync()`` function is called.

rmtSetCarrier
*************

Sets carrier frequency modulation/demodulation for RMT TX or RX channel.

.. code-block:: arduino

    bool rmtSetCarrier(int pin, bool carrier_en, bool carrier_level, uint32_t frequency_Hz, float duty_percent);

* ``pin`` - GPIO pin number configured for RMT
* ``carrier_en`` - Enable/disable carrier modulation (TX) or demodulation (RX)
* ``carrier_level`` - Carrier polarity level:
  
  * ``true`` - Positive polarity (active high)
  * ``false`` - Negative polarity (active low)

* ``frequency_Hz`` - Carrier frequency in Hz (e.g., 38000 for 38 kHz IR carrier)
* ``duty_percent`` - Duty cycle as a float from 0.0 to 1.0 (e.g., 0.33 for 33% duty cycle, 0.5 for 50% square wave)

**Note:** Parameters changed in Arduino Core 3: low and high (ticks) are now expressed in Carrier Frequency in Hz and duty cycle in percentage (float 0.0 to 1.0).

**Example:** 38.5 kHz carrier with 33% duty cycle: ``rmtSetCarrier(pin, true, true, 38500, 0.33)``

This function returns ``true`` if carrier is set successfully, ``false`` otherwise.

rmtSetRxMinThreshold
********************

Sets the minimum pulse width filter threshold for RX channel. Pulses smaller than this threshold will be ignored as noise.

.. code-block:: arduino

    bool rmtSetRxMinThreshold(int pin, uint8_t filter_pulse_ticks);

* ``pin`` - GPIO pin number configured for RMT RX mode
* ``filter_pulse_ticks`` - Minimum pulse width in RMT ticks. Pulses (high or low) smaller than this will be filtered out.
  
  Set to ``0`` to disable the filter.

**Note:** The filter threshold is specified in RMT ticks, which depends on the RMT frequency set during ``rmtInit()``.

This function returns ``true`` if filter threshold is set successfully, ``false`` otherwise.

rmtSetRxMaxThreshold
********************

Sets the maximum idle threshold for RX channel. When no edge is detected for longer than this threshold, the receiving process is finished.

.. code-block:: arduino

    bool rmtSetRxMaxThreshold(int pin, uint16_t idle_thres_ticks);

* ``pin`` - GPIO pin number configured for RMT RX mode
* ``idle_thres_ticks`` - Maximum idle time in RMT ticks. When no edge is detected for longer than this time, reception ends.
  
  This threshold also defines how many low/high bits are read at the end of the received data.

**Note:** The idle threshold is specified in RMT ticks, which depends on the RMT frequency set during ``rmtInit()``.

This function returns ``true`` if idle threshold is set successfully, ``false`` otherwise.

RMT Data Structure
------------------

rmt_data_t
**********

RMT data structure representing a single RMT symbol (two consecutive pulses).

.. code-block:: arduino

    typedef union {
        struct {
            uint32_t duration0 : 15;  // Duration of first pulse in RMT ticks
            uint32_t level0    : 1;   // Level of first pulse (0 = LOW, 1 = HIGH)
            uint32_t duration1 : 15;  // Duration of second pulse in RMT ticks
            uint32_t level1    : 1;   // Level of second pulse (0 = LOW, 1 = HIGH)
        };
        uint32_t val;  // Access as 32-bit value
    } rmt_data_t;

Each RMT symbol contains two pulses:
* **First pulse:** ``level0`` for ``duration0`` ticks
* **Second pulse:** ``level1`` for ``duration1`` ticks

**Example:**

.. code-block:: arduino

    // Create a symbol: HIGH for 8 ticks, then LOW for 4 ticks
    rmt_data_t symbol = {
        .duration0 = 8,
        .level0 = 1,
        .duration1 = 4,
        .level1 = 0
    };
    
    // Or using struct initialization
    rmt_data_t symbol2 = {8, 1, 4, 0};

Helper Macros
-------------

RMT_SYMBOLS_OF
**************

Helper macro to calculate the number of RMT symbols in an array.

.. code-block:: arduino

    #define RMT_SYMBOLS_OF(x) (sizeof(x) / sizeof(rmt_data_t))

**Example:**

.. code-block:: arduino

    rmt_data_t data[] = {
        {8, 1, 4, 0},
        {4, 1, 8, 0}
    };
    
    size_t num_symbols = RMT_SYMBOLS_OF(data);  // Returns 2

RMT_WAIT_FOR_EVER
*****************

Constant for indefinite timeout in blocking operations.

.. code-block:: arduino

    #define RMT_WAIT_FOR_EVER ((uint32_t)portMAX_DELAY)

Use this constant as the ``timeout_ms`` parameter in ``rmtWrite()`` or ``rmtRead()`` to wait indefinitely.

**Example:**

.. code-block:: arduino

    rmtWrite(pin, data, num_symbols, RMT_WAIT_FOR_EVER);

RMT_SYMBOLS_PER_CHANNEL_BLOCK
******************************

Constant defining the number of RMT symbols per memory block.

* ESP32/ESP32-S2: 64 symbols per block
* ESP32-S3/ESP32-C3/ESP32-C5/ESP32-C6/ESP32-H2/ESP32-P4: 48 symbols per block

**Example:**

.. code-block:: arduino

    // Allocate buffer for 1 memory block
    rmt_data_t buffer[RMT_SYMBOLS_PER_CHANNEL_BLOCK];

Example Applications
********************

RMT Write RGB LED (WS2812):
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. literalinclude:: ../../../libraries/ESP32/examples/RMT/RMTWrite_RGB_LED/RMTWrite_RGB_LED.ino
    :language: arduino

RMT LED Blink:
^^^^^^^^^^^^^^

.. literalinclude:: ../../../libraries/ESP32/examples/RMT/RMT_LED_Blink/RMT_LED_Blink.ino
    :language: arduino

RMT Read XJT Protocol:
^^^^^^^^^^^^^^^^^^^^^^^

.. literalinclude:: ../../../libraries/ESP32/examples/RMT/RMTReadXJT/RMTReadXJT.ino
    :language: arduino

Complete list of `RMT examples <https://github.com/espressif/arduino-esp32/tree/master/libraries/ESP32/examples/RMT>`_.
