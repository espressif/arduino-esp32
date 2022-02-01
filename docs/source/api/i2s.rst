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
  *  In this library function parameter ``sckPin`` or constant ``PIN_I2S_SCK``.

* **Word clock line**
  
  * Officially "word select (WS)". Typically called "left-right clock (LRCLK)" or "frame sync (FS)".
  * 0 = Left channel, 1 = Right channel
  * In this library function parameter ``fsPin`` or constant ``PIN_I2S_FS``.

* **Data line**

  * Officially "serial data (SD)", but can be called SDATA, SDIN, SDOUT, DACDAT, ADCDAT, etc.
  * Unlike Arduino I2S with single data pin switching between input and output, in ESP core driver use separate data line for input and output.
  * For backward compatibility, the shared data pin is ``sdPin`` or constant ``PIN_I2S_SD`` when using simplex mode.
  
  * When using in duplex mode, there are two data lines:
    
    * Output data line is called ``outSdPin`` for function parameter, or constant ``PIN_I2S_SD_OUT``
    * Input data line is called ``inSdPin`` for function parameter, or constant ``PIN_I2S_SD_IN``

I2S Modes
---------

The I2S can be set up in three groups of modes:

* Master (default) or Slave.
* Simplex (default) or Duplex.
* Operation modes (Philips standard, ADC/DAC, PDM)
  
  * Most of them are dual-channel, some can be single channel

.. note:: Officially supported operation mode is only ``I2S_PHILIPS_MODE``. Other modes are implemented, but we cannot guarantee flawless execution and behavior.

Master / Slave Mode
*******************

In **Master mode** (default) the device is generating clock signal ``sckPin`` and word select signal on ``fsPin``.

In **Slave mode** the device listens on attached pins for the clock signal and word select - i.e. unless externally driven the pins will remain LOW.

How to enter either mode is described in the function section.

Operation Modes
***************

Setting the operation mode is done with function ``begin`` (see API section)

* ``I2S_PHILIPS_MODE``
    * Currently the only official* ``PIN_I2S_SCK``
* ``PIN_I2S_FS``
* ``PIN_I2S_SD``
* ``PIN_I2S_SD_OUT`` only need to send one channel data but the data will be copied for another channel automatically, then both channels will transmit same data.

* ``ADC_DAC_MODE``
    The output will be an analog signal on pins ``25`` (L or R?) and ``26`` (L or R?).
    Input will be received on pin ``_inSdPin``.
    The data are sampled in 12 bits and stored in a 16 bits, with the 4 most significant bits set to zero.

* ``PDM_STEREO_MODE``
    Pulse-density-modulation is similar to PWM, but instead, the pulses have constant width. The signal is modulated with the number of ones or zeroes in sequence.

* ``PDM_MONO_MODE``
    Single-channel version of PDM mode described above.

Simplex / Duplex Mode
*********************

The **Simplex** mode is the default after driver initialization. Simplex mode uses the shared data pin ``sdPin`` or constant ``PIN_I2S_SD`` for both output and input, but can only read or write. This is the same behavior as in original Arduino library.

The **Duplex** mode uses two separate data pins:

* Output pin ``outSdPin`` for function parameter, or constant ``PIN_I2S_SD_OUT``
* Input pin ``inSdPin`` for function parameter, or constant ``PIN_I2S_SD_IN``

In this mode, the driver is able to read and write simultaneously on each line and is suitable for applications like walkie-talkie or phone.

Switching between these modes is performed simply by calling setDuplex() or setSimplex() (see APi section for details and more functions).

Arduino-ESP32 I2S API
---------------------

The ESP32 I2S library is based on the Arduino I2S Library and implements a few more APIs, described in this `documentation <https://www.arduino.cc/en/Reference/I2S>`_.

Initialization and deinitialization
***********************************

Before initialization, choose which pins you want to use. In DAC mode you can use only pins `25` and `26` for the output.

begin (Master Mode)
^^^^^^^^^^^^^^^^^^^

Before usage choose which pins you want to use. In DAC mode you can use only pins 25 and 26 as output.

.. code-block:: arduino

    int begin(int mode, int sampleRate, int bitsPerSample)

Parameters:
 
* [in] ``mode`` one of above mentioned operation mode, for example ``I2S_PHILIPS_MODE``.

* [in] ``sampleRate`` is the sampling rate in Hz. Currently officially supported value is only 16000 - other than this value will print warning, but continue to operate, however the resulting audio quality may suffer and the app may crash.

* [in] ``bitsPerSample``  is the number of bits in a channel sample.
  
Currently, the supported value is only 16 - other than this value will print a warning, but continues to operate, however, the resulting audio quality may suffer and the application may crash.

For ``ADC_DAC_MODE`` the only possible value will remain 16.

This function will return ``true`` on success or ``fail`` in case of failure.

When failed, an error message will be printed if subscribed.

begin (Slave Mode)
^^^^^^^^^^^^^^^^^^

Performs initialization before use - creates buffers, task handling underlying driver messages, configuring and starting the driver operation.

This version initializes I2S in SLAVE mode (see previous entry for MASTER mode).

.. code-block:: arduino

    int begin(int mode, int bitsPerSample)

Parameters:

* [in] ``mode`` one of above mentioned modes for example ``I2S_PHILIPS_MODE``.

* [in] ``bitsPerSample`` is the umber of bits in a channel sample. Currently, the only supported value is only 16 - other than this value will print warning, but continue to operate, however the resulting audio quality may suffer and the app may crash.

For ``ADC_DAC_MODE`` the only possible value will remain 16.

This function will return ``true`` on success or ``fail`` in case of failure.

When failed, an error message will be printed if subscribed.

end
^^^

Performs safe deinitialization - free buffers, destroy task, end driver operation, etc.

.. code-block:: arduino

  void end()

Pin setup
*********

Pins can be changed in two ways- 1st constants, 2nd functions.

.. note:: Shared data pin can be equal to any other data pin, but must not be equal to clock pin nor frame sync pin! Input and Output pins must not be equal, but one of them can be equal to shared data pin!

.. code-block:: arduino

    sckPin != fsPin != outSdPin != inSdPin

.. code-block:: arduino

    sckPin != fsPin != sdPin

By default, the pin numbers are defined in constants in the header file. You can redefine any of those constants before including ``I2S.h``. This way the driver will use these new default values and you will not need to specify pins in your code. The constants and their default values are:

* ``PIN_I2S_SCK`` 14
* ``PIN_I2S_FS`` 25
* ``PIN_I2S_SD`` 26
* ``PIN_I2S_SD_OUT`` 26
* ``PIN_I2S_SD_IN`` 35

The second option to change pins is using the following functions. These functions can be called on either on initialized or uninitialized object.

If called on the initialized object (after calling ``begin``) the pins will change during operation.
If called on the uninitialized object (before calling ``begin``, or after calling ``end``) the new pin setup will be used on next initialization.

setSckPin
^^^^^^^^^

Set and apply clock pin.

.. code-block:: arduino

  int setSckPin(int sckPin)

This function will return ``true`` on success or ``fail`` in case of failure.

setFsPin
^^^^^^^^

Set and apply frame sync pin.

.. code-block:: arduino

  int setFsPin(int fsPin)

This function will return ``true`` on success or ``fail`` in case of failure.

setDataPin
^^^^^^^^^^

Set and apply shared data pin used in simplex mode.

.. code-block:: arduino

  int setDataPin(int sdPin)

This function will return ``true`` on success or ``fail`` in case of failure.

setDataInPin
^^^^^^^^^^^^

Set and apply data input pin.

.. code-block:: arduino

  int setDataInPin(int inSdPin)

This function will return ``true`` on success or ``fail`` in case of failure.

setDataOutPin
^^^^^^^^^^^^^

Set and apply data output pin.

.. code-block:: arduino

  int setDataOutPin(int outSdPin)

This function will return ``true`` on success or ``fail`` in case of failure.

setAllPins
^^^^^^^^^^

Set all pins using given values in parameters. This is simply a wrapper of four functions mentioned above.

.. code-block:: arduino

  int setAllPins(int sckPin, int fsPin, int sdPin, int outSdPin, int inSdPin)

Set all pins to default i.e. take values from constants mentioned above. This simply calls the the function with the following constants.

* ``PIN_I2S_SCK`` 14
* ``PIN_I2S_FS`` 25
* ``PIN_I2S_SD`` 26
* ``PIN_I2S_SD_OUT`` 26
* ``PIN_I2S_SD_IN`` 35

.. code-block:: arduino

  int setAllPins()

getSckPin
^^^^^^^^^

Get the current value of the clock pin.

.. code-block:: arduino

  int getSckPin()

getFsPin
^^^^^^^^

Get the current value of frame sync pin.

.. code-block:: arduino

  int getFsPin()

getDataPin
^^^^^^^^^^

Get the current value of shared data pin.

.. code-block:: arduino

  int getDataPin()

getDataInPin
^^^^^^^^^^^^

Get the current value of data input pin.

.. code-block:: arduino

  int getDataInPin()

getDataOutPin
^^^^^^^^^^^^^

Get the current value of data output pin.

.. code-block:: arduino

  int getDataOutPin()

onTransmit
^^^^^^^^^^

Register the function to be called on each successful transmit event.

.. code-block:: arduino

  void onTransmit(void(*)(void))

onReceive
^^^^^^^^^

Register the function to be called on each successful receives event.

.. code-block:: arduino

  void onReceive(void(*)(void))

setBufferSize
^^^^^^^^^^^^^

Set the size of buffer.

.. code-block:: arduino

  int setBufferSize(int bufferSize)

This function can be called on both the initialized or uninitialized driver.

If called on initialized, it will change internal values for buffer size and re-initialize driver with new value.
If called on uninitialized, it will only change the internal values which will be used for next initialization.

Parameter ``bufferSize`` must be in range from 8 to 1024 and the unit is sample words. The default value is 128.

Example: 16 bit sample, dual channel, buffer size for input: 

  ``128 = 2B sample * 2 channels * 128 buffer size * buffer count (default 2) = 1024B``
  
And more ```1024B`` for output buffer in total of ``2kB`` used.

This function always assumes dual-channel, keeping the same size even for MONO modes.

This function will return ``true`` on success or ``fail`` in case of failure.

When failed, an error message will be printed.

getBufferSize
^^^^^^^^^^^^^

Get current buffer sizes in sample words (see description for ``setBufferSize``).

.. code-block:: arduino

  int getBufferSize()

Duplex vs Simplex
*****************

Original Arduino I2S library supports only *simplex* mode (only transmit or only receive at a time). For compatibility, we kept this behavior, but ESP natively supports *duplex* mode (receive and transmit simultaneously on separate pins).
By default this library is initialized in simplex mode as it would in Arduino, switching input and output on ``sdPin`` (constant ``PIN_I2S_SD`` default pin 26).

setDuplex
^^^^^^^^^

Switch to duplex mode and use separate pins:

.. code-block:: arduino

  int setDuplex()

input: inSdPin (constant PIN_I2S_SD_IN, default 35)
output: outSdPin (constant PIN_I2S_SD, default 26)

setSimplex
^^^^^^^^^^

(Default mode)

Switch to simplex mode using shared data pin sdPin (constant PIN_I2S_SD, default 26).

.. code-block:: arduino

  int setSimplex()

isDuplex
^^^^^^^^

Returns 1 if current mode is duplex, 0 if current mode is simplex (default).

.. code-block:: arduino

  int isDuplex()

Data stream
***********

available
^^^^^^^^^

Returns number of **bytes** ready to read.

.. code-block:: arduino

  int available()

read
^^^^

Read ``size`` bytes from internal buffer if possible.

.. code-block:: arduino

  int read(void* buffer, size_t size)

This function is non-blocking, i.e. if the requested number of bytes is not available, it will return as much as possible without waiting.

Hint: use ``available()`` before calling this function.

Parameters:

[out] ``void* buffer`` buffer into which will be copied data read from internal buffer. WARNING: this buffer must be allocated before use!

[in] ``size_t size`` number of bytes required to be read.

Returns number of successfully bytes read. Returns ``false``` in case of reading error.

Read one sample.

.. code-block:: arduino

  int read()

peek
^^^^

Read one sample from the internal buffer and returns it.

.. code-block:: arduino

  int peek()

Repeated peeks will be returned in the same sample until ``read`` is called.

flush
^^^^^

Force write internal buffer to driver.

.. code-block:: arduino

  void flush()

write
^^^^^

Write a single byte.

.. code-block:: arduino

  size_t write(uint8_t)

Single-sample writes are blocking - waiting until there is free space in the internal buffer to be written into.

Returns number of successfully written bytes, in this case, 1. Returns 0 on error.

Write single sample.

.. code-block:: arduino

  size_t write(int32_t)

Single-sample writes are blocking - waiting until there is free space in the internal buffer to be written into.

Returns number of successfully written bytes. Returns 0 on error.

Expected return number is ``bitsPerSample/8``.

Write buffer of supplied size;

.. code-block:: arduino

  size_t write(const void *buffer, size_t size)

Parameters:

[in] ``const void *buffer`` buffer to be written
[in] ``size_t size`` size of buffer in bytes

Returns number of successfully written bytes. Returns 0 in case of error.
The expected return number is equal to ``size``.

write
^^^^^

This is a wrapper of the previous function performing typecast from `uint8_t*`` to ``void*``.

.. code-block:: arduino

  size_t write(const uint8_t *buffer, size_t size)

availableForWrite
^^^^^^^^^^^^^^^^^

Returns number of bytes available for write.

.. code-block:: arduino

  int availableForWrite()

write_blocking
^^^^^^^^^^^^^^

Core function implementing blocking write, i.e. waits until all requested data are written.

.. code-block:: arduino

  size_t write_blocking(const void *buffer, size_t size)

WARNING: If too many bytes are requested, this can cause WatchDog Trigger Reset!

Returns number of successfully written bytes. Returns 0 on error.

write_nonblocking
^^^^^^^^^^^^^^^^^

Core function implementing non-blocking write, i.e. writes as much as possible and exits.

.. code-block:: arduino

  size_t write_nonblocking(const void *buffer, size_t size)

Returns number of successfully written bytes. Returns 0 on error.

Sample code
-----------

.. code-block:: c

  #include <I2S.h>
  const int buff_size = 128;
  int available, read;
  uint8_t buffer[buff_size];

  I2S.begin(I2S_PHILIPS_MODE, 16000, 16);
  I2S.read(); // Switch the driver in simplex mode to receive
  available = I2S.available();
  if(available < buff_size){
    read = I2S.read(buffer, available);
  }else{
    read = I2S.read(buffer, buff_size);
  }
  I2S.write(buffer, read);
  I2S.end();
