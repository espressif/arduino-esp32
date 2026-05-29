###
I2S
###

About
-----

I2S - Inter-IC Sound, correctly written I²S pronounced "eye-squared-ess", alternative notation is IIS. I²S is an electrical serial bus interface standard used for connecting digital audio devices together.

It is used to communicate PCM (Pulse-Code Modulation) audio data between integrated circuits in an electronic device. The I²S bus separates clock and serial data signals, resulting in simpler receivers than those required for asynchronous communications systems that need to recover the clock from the data stream.

Despite the similar name, I²S is unrelated and incompatible with the bidirectional I²C (IIC) bus.

The I²S bus consists of at least three lines:

.. note:: All lines can be attached to almost any pin and this change can occur even during operation.

* **Bit clock line**

  * Officially "continuous serial clock (SCK)". Typically written "bit clock (BCLK)".
  *  In this library function parameter ``sck``.

* **Word clock line**

  * Officially "word select (WS)". Typically called "left-right clock (LRCLK)" or "frame sync (FS)".
  * 0 = Left channel, 1 = Right channel
  * In this library function parameter ``ws``.

* **Data line**

  * Officially "serial data (SD)", but can be called SDATA, SDIN, SDOUT, DACDAT, ADCDAT, etc.
  * Unlike Arduino I2S with single data pin switching between input and output, in ESP core driver use separate data line for input and output.
  * Output data line is called ``dout`` for function parameter.
  * Input data line is called ``din`` for function parameter.

It may also include a **Master clock** line:

* **Master clock**

  * Officially "master clock (MCLK)".
  * This is not a part of I2S bus, but is used to synchronize multiple I2S devices.
  * In this library function parameter ``mclk``.

.. note:: Please check the `ESP-IDF documentation <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2s.html>`_ for more details on the I2S peripheral for each ESP32 chip.

I2S Configuration
-----------------

Master / Slave Mode
*******************

In **Master mode** (default), the device generates the clock signal ``sck`` and word select signal on ``ws``.

In **Slave mode**, the device listens on attached pins for the clock signal and word select driven by an external master.
The role is selected via the ``role`` parameter of ``begin()`` (default ``I2S_ROLE_MASTER``).

I2S Port
********

By default, the I2S driver automatically selects an available I2S controller.
Applications that need a specific controller, such as dual-I2S audio designs, can select it before initialization using ``setPort()`` or the ``I2SClass`` constructor.

Operation Modes
***************

Setting the operation mode is done with function ``begin`` and is set by function parameter ``mode``.

* ``I2S_MODE_STD``
    In standard mode, there are always two sound channels, i.e., the left and right channels, which are called "slots".
    These slots support 8/16/24/32-bit width sample data.
    The communication format for the slots follows the Philips standard.

* ``I2S_MODE_TDM``
    In Time Division Multiplexing mode (TDM), the number of sound channels is variable, and the width of each channel
    is fixed.

* ``I2S_MODE_PDM_TX``
    PDM (Pulse-density Modulation) mode for the TX channel can convert PCM data into PDM format which always
    has left and right slots.
    PDM TX is only supported on I2S0 and it only supports 16-bit width sample data.
    It needs at least a CLK pin for clock signal and a DOUT pin for data signal.

* ``I2S_MODE_PDM_RX``
    PDM (Pulse-density Modulation) mode for RX channel can receive PDM-format data and convert the data
    into PCM format. PDM RX is only supported on I2S0, and it only supports 16-bit width sample data.
    PDM RX needs at least a CLK pin for clock signal and a DIN pin for data signal.

Simplex / Duplex Mode
*********************

Due to the different clock sources the PDM modes are always in **Simplex** mode, using only one data pin.

The STD and TDM modes operate in the **Duplex** mode, using two separate data pins:

* Output pin ``dout`` for function parameter
* Input pin ``din`` for function parameter

In this mode, the driver is able to read and write simultaneously on each line and is suitable for applications like walkie-talkie or phone.

Data Bit Width
**************

This is the number of bits in a channel sample. The data bit width is set by function parameter ``bits_cfg``.
The current supported values are:

* ``I2S_DATA_BIT_WIDTH_8BIT`` (see note below)
* ``I2S_DATA_BIT_WIDTH_16BIT``
* ``I2S_DATA_BIT_WIDTH_24BIT``, requires the MCLK multiplier to be manually set to 384
* ``I2S_DATA_BIT_WIDTH_32BIT``

.. note::
    **8-bit data width on ESP32 (HW v1):** The original ESP32's I2S peripheral uses
    ``uint16_t``-aligned DMA words for 8-bit data, with only the high 8 bits valid.
    The Arduino library handles this transparently: ``write()`` packs each ``int8_t``
    sample into the high byte of a ``uint16_t``, and ``readBytes()`` unpacks it back.
    For mono 8-bit, the existing stereo workaround is also applied (hardware runs in
    stereo mode, software converts). This is fully automatic and requires no special
    handling in application code. See the
    `ESP-IDF I2S STD TX Mode documentation <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2s.html#std-tx-mode>`_
    for background on the hardware behavior.

Sample Rate
***********

The sample rate is set by function parameter ``rate``. It is the number of samples per second in Hz.

Slot Mode
*********

The slot mode is set by function parameter ``ch``. The current supported values are:

* ``I2S_SLOT_MODE_MONO``
    I2S channel slot format mono. Transmit the same data in all slots for TX mode.
    Only receive the data in the first slot for RX mode.

* ``I2S_SLOT_MODE_STEREO``
    I2S channel slot format stereo. Transmit different data in different slots for TX mode.
    Receive the data in all slots for RX mode.

.. note::
    On the original ESP32 (I2S hardware version 1), 8-bit and 16-bit mono modes have a
    known hardware limitation: the FIFO packing logic scrambles data during transmission
    and reception. The library works around this automatically by configuring the hardware
    in stereo mode and converting between mono and stereo in software. ``write()`` duplicates
    each mono sample into both left and right slots, and ``readBytes()`` extracts the selected
    channel to produce mono output. For 8-bit, the library also handles the ``uint16_t``
    high-byte packing required by the hardware. This is fully transparent to application code.

Slot Mask
*********

The ``slot_mask`` parameter selects which channel slot(s) to use. It is available in
``begin()``, ``configureTX()``, and ``configureRX()``. When set to ``-1`` (default),
the library chooses a sensible default based on the slot mode.

The supported values are:

* ``I2S_STD_SLOT_LEFT`` (1) -- Use the left slot only.
* ``I2S_STD_SLOT_RIGHT`` (2) -- Use the right slot only.
* ``I2S_STD_SLOT_BOTH`` (3) -- Use both left and right slots.

**TX behavior (mono mode):**

* ``LEFT`` -- Transmit mono data on the left wire slot; right is silent.
* ``RIGHT`` -- Transmit mono data on the right wire slot; left is silent.
* ``BOTH`` -- Transmit mono data on both wire slots. Useful for driving a stereo DAC
  from a single-channel source.

**TX behavior (stereo mode):**

* ``LEFT`` -- Transmit only the left-channel data from the buffer on the left wire slot; right is silent.
* ``RIGHT`` -- Transmit only the right-channel data from the buffer on the right wire slot; left is silent.
* ``BOTH`` -- Transmit interleaved stereo data normally.

**RX behavior (mono mode):**

* ``LEFT`` -- Receive the left channel only.
* ``RIGHT`` -- Receive the right channel only.
* ``BOTH`` -- On the original ESP32 (HW v1), averages the left and right channels.
  On other targets, causes sample duplication in the DMA buffer (not recommended).

**RX behavior (stereo mode):**

The library always forces ``slot_mask`` to ``BOTH`` for stereo RX, regardless of the
value passed by the user. This is because the ESP-IDF does not reliably ignore
``slot_mask`` in stereo RX mode on all targets — some hardware duplicates the selected
channel instead of delivering normal interleaved data. The library overrides the mask
to guarantee correct interleaved ``[L, R, L, R, ...]`` output.

.. note::
    On the original ESP32 (HW v1) with mono 8-bit or 16-bit mode, ``slot_mask`` controls
    which channel the software workaround extracts: ``LEFT`` extracts the left channel,
    ``RIGHT`` extracts the right channel, and ``BOTH`` averages the two channels.
    This can be changed at runtime via ``configureRX()``.

Arduino-ESP32 I2S API
---------------------

Initialization and deinitialization
***********************************

Before initialization, set which pins you want to use.

I2SClass
^^^^^^^^

Create an I2S object. The optional ``port`` parameter selects the I2S controller.
If it is not specified, the driver uses ``I2S_NUM_AUTO``.

.. code-block:: arduino

  I2SClass I2S(i2s_port_t port=I2S_NUM_AUTO)

setPort
^^^^^^^

Set the I2S controller to use. This function must be called before ``begin()``.

.. code-block:: arduino

  bool setPort(i2s_port_t port)

Parameters:

* [in] ``port`` is the I2S controller, for example ``I2S_NUM_0``, another SoC-specific port such as ``I2S_NUM_1``, or ``I2S_NUM_AUTO``.

This function will return ``true`` on success or ``false`` in case of failure.

getPort
^^^^^^^

Get the configured I2S controller.
After ``begin()``, this returns the controller allocated by the driver.
Before ``begin()``, this returns the configured controller value, such as ``I2S_NUM_AUTO``.

.. code-block:: arduino

  i2s_port_t getPort()

begin
^^^^^

Initialize the I2S driver. Before calling ``begin()``, choose which pins you want to use with ``setPins()`` or the PDM pin-setup functions.

.. code-block:: arduino

    bool begin(i2s_mode_t mode, uint32_t rate, i2s_data_bit_width_t bits_cfg, i2s_slot_mode_t ch, int8_t slot_mask=-1, i2s_role_t role=I2S_ROLE_MASTER)

Parameters:

* [in] ``mode`` one of above mentioned operation mode, for example ``I2S_MODE_STD``.

* [in] ``rate`` is the sampling rate in Hz, for example ``16000``.

* [in] ``bits_cfg``  is the number of bits in a channel sample, for example ``I2S_DATA_BIT_WIDTH_16BIT``.

* [in] ``ch`` is the slot mode, for example ``I2S_SLOT_MODE_STEREO``.

* [in] ``slot_mask`` selects which slot(s) to use (see `Slot Mask`_ section).
         This parameter is optional and defaults to ``-1`` (library default: LEFT for mono TX/RX, BOTH for stereo).
         For stereo RX, the library always forces BOTH regardless of this value.

* [in] ``role`` is the I2S role. Use ``I2S_ROLE_MASTER`` (default) to generate clock and word-select signals, or ``I2S_ROLE_SLAVE`` to receive them from an external master.

This function will return ``true`` on success or ``false`` in case of failure.

When failed, an error message will be printed if the correct log level is set.

end
^^^

Performs safe deinitialization - free buffers, destroy task, end driver operation, etc.

.. code-block:: arduino

  void end()

Pin setup
*********

The function to set the pins will depend on the operation mode.

setPins
^^^^^^^

Set the pins for the I2S interface when using the standard or TDM mode.

.. code-block:: arduino

  void setPins(int8_t bclk, int8_t ws, int8_t dout, int8_t din=-1, int8_t mclk=-1)

Parameters:

* [in] ``bclk`` is the bit clock pin.

* [in] ``ws`` is the word select pin.

* [in] ``dout`` is the data output pin. Can be set to ``-1`` if not used.

* [in] ``din`` is the data input pin. This parameter is optional and defaults to ``-1`` (not used).

* [in] ``mclk`` is the master clock pin. This parameter is optional and defaults to ``-1`` (not used).

setPinsPdmTx
^^^^^^^^^^^^

Set the pins for the I2S interface when using the PDM TX mode.

.. code-block:: arduino

  void setPinsPdmTx(int8_t clk, int8_t dout0, int8_t dout1=-1)

Parameters:

* [in] ``clk`` is the clock pin.

* [in] ``dout0`` is the data output pin 0.

* [in] ``dout1`` is the data output pin 1. This parameter is optional and defaults to ``-1`` (not used).

setPinsPdmRx
^^^^^^^^^^^^

Set the pins for the I2S interface when using the PDM RX mode.

.. code-block:: arduino

  void setPinsPdmRx(int8_t clk, int8_t din0, int8_t din1=-1, int8_t din2=-1, int8_t din3=-1)

Parameters:

* [in] ``clk`` is the clock pin.

* [in] ``din0`` is the data input pin 0.

* [in] ``din1`` is the data input pin 1. This parameter is optional and defaults to ``-1`` (not used).

* [in] ``din2`` is the data input pin 2. This parameter is optional and defaults to ``-1`` (not used).

* [in] ``din3`` is the data input pin 3. This parameter is optional and defaults to ``-1`` (not used).

setInverted
^^^^^^^^^^^

Set which pins have inverted logic when using the standard or TDM mode. Data pins cannot be inverted.

.. code-block:: arduino

  void setInverted(bool bclk, bool ws, bool mclk=false)

Parameters:

* [in] ``bclk`` true if the bit clock pin is inverted. False otherwise.

* [in] ``ws`` true if the word select pin is inverted. False otherwise.

* [in] ``mclk`` true if the master clock pin is inverted. False otherwise. This parameter is optional and defaults to ``false``.

setInvertedPdm
^^^^^^^^^^^^^^

Set which pins have inverted logic when using the PDM mode. Data pins cannot be inverted.

.. code-block:: arduino

  void setInvertedPdm(bool clk)

Parameters:

* [in] ``clk`` true if the clock pin is inverted. False otherwise.

I2S Configuration
*****************

The I2S configuration can be changed during operation.

configureTX
^^^^^^^^^^^

Configure the I2S TX channel.

.. code-block:: arduino

  bool configureTX(uint32_t rate, i2s_data_bit_width_t bits_cfg, i2s_slot_mode_t ch, int8_t slot_mask=-1)

Parameters:

* [in] ``rate`` is the sampling rate in Hz, for example ``16000``.

* [in] ``bits_cfg``  is the number of bits in a channel sample, for example ``I2S_DATA_BIT_WIDTH_16BIT``.

* [in] ``ch`` is the slot mode, for example ``I2S_SLOT_MODE_STEREO``.

* [in] ``slot_mask`` selects which slot(s) to use for TX (see `Slot Mask`_ section).
         This parameter is optional and defaults to ``-1`` (library default).

This function will return ``true`` on success or ``false`` in case of failure.

When failed, an error message will be printed if the correct log level is set.

configureRX
^^^^^^^^^^^

Configure the I2S RX channel. This function can change the sample rate, bit width,
slot mode, RX transform, and slot mask at runtime without calling ``end()``/``begin()``.
If only the ``slot_mask`` changes (rate, bits, and channel mode remain the same), the
library performs a lightweight slot-only reconfiguration.

.. code-block:: arduino

  bool configureRX(uint32_t rate, i2s_data_bit_width_t bits_cfg, i2s_slot_mode_t ch, i2s_rx_transform_t transform=I2S_RX_TRANSFORM_NONE, int8_t slot_mask=-1)

Parameters:

* [in] ``rate`` is the sampling rate in Hz, for example ``16000``.

* [in] ``bits_cfg``  is the number of bits in a channel sample, for example ``I2S_DATA_BIT_WIDTH_16BIT``.

* [in] ``ch`` is the slot mode, for example ``I2S_SLOT_MODE_STEREO``.

* [in] ``transform`` is the transform mode, for example ``I2S_RX_TRANSFORM_NONE``.
         This can be used to apply a transformation/conversion to the received data.
         The supported values are: ``I2S_RX_TRANSFORM_NONE`` (no transformation),
         ``I2S_RX_TRANSFORM_32_TO_16`` (convert from 32 bits of data width to 16 bits) and
         ``I2S_RX_TRANSFORM_16_STEREO_TO_MONO`` (convert from stereo to mono when using 16 bits of data width).
         When using ``I2S_RX_TRANSFORM_16_STEREO_TO_MONO``, the channel extracted depends on the configured
         ``slot_mask``: ``LEFT`` extracts the left channel, ``RIGHT`` extracts the right channel, and ``BOTH``
         averages both channels.

* [in] ``slot_mask`` is the slot mask for RX channel selection (see `Slot Mask`_ section).
         This parameter is optional and defaults to ``-1`` (use current setting).
         In mono mode, this selects which channel to receive (LEFT, RIGHT, or BOTH).
         In stereo mode, this parameter is ignored — the library always uses BOTH.

This function will return ``true`` on success or ``false`` in case of failure.

When failed, an error message will be printed if the correct log level is set.

txChan
^^^^^^

Get the TX channel handler pointer.

.. code-block:: arduino

  i2s_chan_handle_t txChan()

txSampleRate
^^^^^^^^^^^^

Get the TX sample rate.

.. code-block:: arduino

  uint32_t txSampleRate()

txDataWidth
^^^^^^^^^^^

Get the TX data width (8, 16 or 32 bits).

.. code-block:: arduino

  i2s_data_bit_width_t txDataWidth()

txSlotMode
^^^^^^^^^^

Get the TX slot mode (stereo or mono).

.. code-block:: arduino

  i2s_slot_mode_t txSlotMode()

rxChan
^^^^^^

Get the RX channel handler pointer.

.. code-block:: arduino

  i2s_chan_handle_t rxChan()

rxSampleRate
^^^^^^^^^^^^

Get the RX sample rate.

.. code-block:: arduino

  uint32_t rxSampleRate()

rxDataWidth
^^^^^^^^^^^

Get the RX data width (8, 16 or 32 bits).

.. code-block:: arduino

  i2s_data_bit_width_t rxDataWidth()

rxSlotMode
^^^^^^^^^^

Get the RX slot mode (stereo or mono).

.. code-block:: arduino

  i2s_slot_mode_t rxSlotMode()

I/O Operations
**************

readBytes
^^^^^^^^^

Read a certain amount of data bytes from the I2S interface.

.. code-block:: arduino

  size_t readBytes(char *buffer, size_t size)

Parameters:

* [in] ``buffer`` is the buffer to store the read data. The buffer must be at least ``size`` bytes long.

* [in] ``size`` is the number of bytes to read.

This function will return the number of bytes read.

read
^^^^

Read the next available byte from the I2S interface.

.. code-block:: arduino

  int read()

This function will return the next available byte or ``-1`` if no data is available
or an error occurred.

write

There are two versions of the write function:

The first version writes a certain amount of data bytes to the I2S interface.

.. code-block:: arduino

  size_t write(uint8_t *buffer, size_t size)

Parameters:

* [in] ``buffer`` is the buffer containing the data to be written.

* [in] ``size`` is the number of bytes to write from the buffer.

This function will return the number of bytes written.

The second version writes a single byte to the I2S interface.

.. code-block:: arduino

  size_t write(uint8_t d)

Parameters:

* [in] ``d`` is the byte to be written.

This function will return ``1`` if the byte was written or ``0`` if an error occurred.

available
^^^^^^^^^

Get if there is data available to be read.

.. code-block:: arduino

  int available()

This function will return ``I2S_READ_CHUNK_SIZE`` if there is data available to be read or ``-1`` if not.

peek
^^^^

Get the next available byte from the I2S interface without removing it from the buffer. Currently not implemented.

.. code-block:: arduino

  int peek()

This function will currently always return ``-1``.

lastError
^^^^^^^^^

Get the last error code for an I/O operation on the I2S interface.

.. code-block:: arduino

  int lastError()

recordWAV
^^^^^^^^^

Record a short PCM WAV to memory with the current RX settings.
Returns a buffer that must be freed by the user.

.. code-block:: arduino

  uint8_t * recordWAV(size_t rec_seconds, size_t * out_size)

Parameters:

* [in] ``rec_seconds`` is the number of seconds to record.

* [out] ``out_size`` is the size of the returned buffer in bytes.

This function will return a pointer to the buffer containing the recorded WAV data or ``NULL`` if an error occurred.

playWAV
^^^^^^^

Play a PCM WAV from memory with the current TX settings.

.. code-block:: arduino

  void playWAV(uint8_t * data, size_t len)

Parameters:

* [in] ``data`` is the buffer containing the WAV data.

* [in] ``len`` is the size of the buffer in bytes.

playMP3
^^^^^^^

Play a MP3 from memory with the current TX settings.

.. code-block:: arduino

  bool playMP3(uint8_t *src, size_t src_len)

Parameters:

* [in] ``src`` is the buffer containing the MP3 data.

* [in] ``src_len`` is the size of the buffer in bytes.

This function will return ``true`` on success or ``false`` in case of failure.

When failed, an error message will be printed if the correct log level is set.

Sample code
-----------

Master mode (default)
*********************

.. code-block:: arduino

  #include <ESP_I2S.h>

  const int buff_size = 128;
  int available_bytes, read_bytes;
  uint8_t buffer[buff_size];
  I2SClass I2S;

  void setup() {
    I2S.setPins(5, 25, 26, 35, 0); //SCK, WS, SDOUT, SDIN, MCLK
    I2S.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    I2S.read();
    available_bytes = I2S.available();
    if(available_bytes < buff_size) {
      read_bytes = I2S.readBytes(buffer, available_bytes);
    } else {
      read_bytes = I2S.readBytes(buffer, buff_size);
    }
    I2S.write(buffer, read_bytes);
    I2S.end();
  }

  void loop() {}

Slave mode
**********

.. code-block:: arduino

  #include <ESP_I2S.h>

  const int buff_size = 128;
  uint8_t buffer[buff_size];
  I2SClass I2S;

  void setup() {
    I2S.setPins(5, 25, -1, 35); //SCK, WS, SDOUT (not used), SDIN
    I2S.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, -1, I2S_ROLE_SLAVE);
    size_t read_bytes = I2S.readBytes((char *)buffer, buff_size);
    // process received data...
    I2S.end();
  }

  void loop() {}
