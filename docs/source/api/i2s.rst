###
I2S
###

About
-----

I2S - Inter-IC Sound, correctly written I²S pronounced "eye-squared-ess", alternative notation is IIS. I²S is an electrical serial bus interface standard used for connecting digital audio devices together. It is used to communicate PCM (Pulse-Code Modulation) audio data between integrated circuits in an electronic device. The I²S bus separates clock and serial data signals, resulting in simpler receivers than those required for asynchronous communications systems that need to recover the clock from the data stream. Despite the similar name, I²S is unrelated and incompatible with the bidirectional I²C (IIC) bus.

The I²S bus consists of at least three lines:

All lines can be attached to almost any pin and this change can occur even during operation.

* **Bit clock line**

  * Officially "continuous serial clock (SCK)". Typically written "bit clock (BCLK)".
  *  In this library function parameter ``sckPin`` or constant ``PIN_I2S_SCK``.

* **Word clock line**

  * Officially "word select (WS)". Typically called "left-right clock (LRCLK)" or "frame sync (FS)".
  * 0 = Left channel, 1 = Right channel
  * In this library function parameter ``fsPin`` or constant ``PIN_I2S_FS``.

* **At least one multiplexed data line**

  * Officially "serial data (SD)", but can be called SDATA, SDIN, SDOUT, DACDAT, ADCDAT, etc.
  * Unlike in original Arduino I2S library with single data pin swithing between input and output, in ESP version of this library we use separate data line for input and output.
  * Input data are called ``inSdPin`` and ``sdPin`` for function parameter, or constant ``PIN_I2S_SD`` (for backward compatibility with original Arduino I2S library)
  * Output data are called ``outSdPin`` for function parameter, or constant ``PIN_I2S_SD_OUT``

I2S Modes
*********

The I2C can be used in few different modes:

.. note:: Officially supported mode is only ``I2S_PHILIPS_MODE``. Other modes are implemented, but we cannot guarantee flawless execution and behavior.

Master / slave mode
-------------------

In **Master mode** (default) the device is generating clock signal ``sckPin`` and word select signal on ``fsPin``.

In **Slave mode** the device listens on attached pins for clock signal and word select - i.e. unless externally driven the pins will remain LOW.

How to enter either mode is described in function section.

Operation modes
---------------

* ``I2S_PHILIPS_MODE``
    Currently the only officially supported mode.
    This mode is using original Philips specification for I2S bus. See their paper https://web.archive.org/web/20080706121949/http://www.nxp.com/acrobat_download/various/I2SBUS.pdf

    .. Note::Following modes currently not officially supported. Using any of the following modes will print warning, but continue to operate. However the quality is not guaranteed and the application may crash.

* ``I2S_RIGHT_JUSTIFIED_MODE``
    In this mode, you only need to send one channel data but the data will be copied for another channel automatically, then both channels will transmit same data.

* ``I2S_LEFT_JUSTIFIED_MODE``
    In this mode, you only need to send one channel data but the data will be copied for another channel automatically, then both channels will transmit same data.

* ``ADC_DAC_MODE``
    Output will be analog signal on pins 25 (L or R?) and 26 (L or R?).
    Input will be received on pin ``_inSdPin``.
    The data are sampled on 12 bits and stored in 16 bits with 4 most significant bits set to zero.

* ``PDM_STEREO_MODE``
    Pulse-density-modulation is similar to PWM, but instead the pulses have constant width. The signal is modulated with number of ones or zeroes in sequence.

* ``PDM_MONO_MODE``
    Single-channel version of PDM mode described above.


Arduino-ESP32 I2S API
---------------------

The ESP32 I2S library is based on the Arduino I2S Library and implements a few more APIs, described in this documentation.
https://www.arduino.cc/en/Reference/I2S

Initialization and deinitialization
***********************************
Before usage choose which pins you want to use. In DAC mode you can use only pins 25 and 26 for output.

int begin(int mode, int sampleRate, int bitsPerSample)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Performs initialization before use - creates buffers, task handling underlying driver messages, configuring and starting the driver operation.

This version initializes I2S in MASTER mode (see next entry for SLAVE mode).

parameters:
 [in] ``mode`` one of above mentioned modes for example ``I2S_PHILIPS_MODE``.

 [in] ``sampleRate`` sampling rate in Hz. Currently officially supported value is only 16000 - other than this value will print warning, but continue to operate, however the resulting audio quality may suffer and the app may crash.

 [in] ``bitsPerSample`` Number of bits in a channel sample. Currently officially supported value is only 16 - other than this value will print warning, but continue to operate, however the resulting audio quality may suffer and the app may crash.
 For ``ADC_DAC_MODE`` the only possible value will remain 16.

returns 1 on success, 0 on failure. When failed an error message will be printed if subscribed.

int begin(int mode, int bitsPerSample)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Performs initialization before use - creates buffers, task handling underlying driver messages, configuring and starting the driver operation.

This version initializes I2S in SLAVE mode (see previous entry for MASTER mode).

parameters:
 [in] ``mode`` one of above mentioned modes for example ``I2S_PHILIPS_MODE``.

 [in] ``bitsPerSample`` Number of bits in a channel sample. Currently officially supported value is only 16 - other than this value will print warning, but continue to operate, however the resulting audio quality may suffer and the app may crash.
 For ``ADC_DAC_MODE`` the only possible value will remain 16.

Returns 1 on success, 0 on failure. When failed an error message will be printed if subscribed.

void end()
^^^^^^^^^^
Performs safe deinitialization - free buffers, destroy task, end driver operation, etc.

Pin setup
*********
Pins can changed in two ways- 1st constants, 2nd functions.

..Note:: Shared data pin can be equal to any other data pin, but must not be equal to clock pin nor frame sync pin! Input and Output pins must not be equal, but one of them can be equal to shared data pin!

sckPin != fsPin != outSdPin != inSdPin

sckPin != fsPin != sdPin

By default the pin numbers are defined in constants in the header file. You can redefine any of those constants before including ``I2S.h``. This way the driver will be use these new default values and you will not need to specify pins in your code. The constants and their default values are

``PIN_I2S_SCK 14``

``PIN_I2S_FS 25``

``PIN_I2S_SD 26``

``PIN_I2S_SD_OUT 26``

``PIN_I2S_SD_IN 35``

Second option to change pins is using the following functions. These functions *MUST* be called on intialized object (after calling ``begin``) therefore they can change pin value during operation.


int setSckPin(int sckPin)
^^^^^^^^^^^^^^^^^^^^^^^^^
Set and apply clock pin.

Returns 1 on success, 0 on failure.

int setFsPin(int fsPin)
^^^^^^^^^^^^^^^^^^^^^^^
Set and apply frame sync pin.

Returns 1 on success, 0 on failure.

int setDataPin(int sdPin)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Set and apply shared data pin used in simplex mode.

Returns 1 on success, 0 on failure.

int setDataInPin(int inSdPin)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Set and apply data input pin.

Returns 1 on success, 0 on failure.

int setDataOutPin(int outSdPin)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Set and apply data output pin.
Returns 1 on success, 0 on failure.

int setAllPins(int sckPin, int fsPin, int sdPin, int outSdPin, int inSdPin)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Set all pins using given values in parameters. This simply a wrapper of four functions mentioned above.


int setAllPins()
^^^^^^^^^^^^^^^^
Set all pins to default i.e. take values from constants mentioned above. This simply calls the the function ``setAllPins(PIN_I2S_SCK, PIN_I2S_FS, PIN_I2S_SD, PIN_I2S_SD_OUT, PIN_I2S_SD_IN);``

int getSckPin()
^^^^^^^^^^^^^^^
Get current value of clock pin.

int getFsPin()
^^^^^^^^^^^^^^
Get current value of frame sync pin.

int getDataPin()
^^^^^^^^^^^^^^^^
Get current value of shared data pin.

int getDataInPin()
^^^^^^^^^^^^^^^^^^
Get current value of data input pin.

int getDataOutPin()
^^^^^^^^^^^^^^^^^^^
Get current value of data output pin.


void onTransmit(void(*)(void))
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Register function which will be called on each successful i2s driver transmit event.

void onReceive(void(*)(void))
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Register function which will be called on each successful i2s driver receive event.

int setBufferSize(int bufferSize)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Set new size of buffer.

This function can be called both on initialized and uninitialized driver.
If called on initialized, it will change internal values for buffer size and re-initialize driver with new value.
If called on uninitialized, it will only change the internal values which will be used for next initialization.

Parameter ``bufferSize`` must be in range <8; 1024>. the unit is sample words. Default value on object creation is 128.
Example: 16 bit sample, dual channel, buffer size 128 = 2B sample * 2 channels * 128 buffer size * buffer count (default 2) = 1024B for input buffer + 1024B for output buffer = total 2kB used.

This function always assumes dual channel, keeping the same size even for MONO modes.

Returns 1 on success, 0 on failure. When failed an error message will be printed if subscribed.

int getBufferSize()
^^^^^^^^^^^^^^^^^^^
Get current buffer sizes in sample words (see description for ``setBufferSize``).

Duplex vs Simplex
*****************
Original Arduino I2S library supports only *simplex* mode (only transmit or only receive at a time). For compatibility we kept this behavior, but ESP natively supports *duplex* mode (receive and transmit simultaneously on separate pins).
By default this library is initialized in simplex mode as it would in Arduino, switching input and output on sdPin (constant PIN_I2S_SD) (default pin 26).


int setDuplex()
^^^^^^^^^^^^^^^
Switch to duplex mode and use separate pins:
input: inSdPin (constant PIN_I2S_SD_IN, default 35)
output: outSdPin (constant PIN_I2S_SD, default 26)

int setSimplex()
^^^^^^^^^^^^^^^^
(Default mode)

Switch to simplex mode using shared data pin sdPin (constant PIN_I2S_SD, default 26).

int isDuplex()
^^^^^^^^^^^^^^
Returns 1 if current mode is duplex, 0 if current mode is simplex (default).

Data stream
***********

int available()
^^^^^^^^^^^^^^^
Returns number of **BYTES** ready to read

int read(void* buffer, size_t size)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Read ``size`` Bytes from internal buffer if possible.

This function is non-blocking, i.e. if requested number of Bytes is not available it will return as much as possible without waiting.

Hint: use ``available()`` before calling this function.

Parameters:

[out] ``void* buffer`` buffer into which will be copied data read from internal buffer. WARNING: this buffer must be allocated before use!

[in] ``size_t size`` number of Bytes required to be read.
Returns number of successfully read Bytes. Returns 0 on error.

int read()
^^^^^^^^^^
Read one sample

int peek()
^^^^^^^^^^
Read 1 sample from internal buffer and return it.
Repeated peeks will return the same sample until read is called.


void flush()
^^^^^^^^^^^^
Force write internal buffer to driver.

size_t write(uint8_t)
^^^^^^^^^^^^^^^^^^^^^
Write single Byte.

Single-sample writes are blocking - waiting until there is free space in internal buffer to be written into.

Returns number of successfully written Bytes, in this case 1. Returns 0 on error.

size_t write(int32_t)
^^^^^^^^^^^^^^^^^^^^^
Write sample.

Single-sample writes are blocking - waiting until there is free space in internal buffer to be written into.

Returns number of successfully written bytes. Returns 0 on error.

Expected return number is ``bitsPerSample/8``.

size_t write(const void *buffer, size_t size)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Write buffer of supplied size;

Parameters:

[in] ``const void *buffer`` buffer to be written

[in] ``size_t size`` size of buffer in Bytes

Returns number of successfully written bytes. Returns 0 on error.
Expected return number is equal to ``size``.

size_t write(const uint8_t *buffer, size_t size)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
This is a wrapper of previous function performing typecast from `uint8_t*`` to ``void*``.

int availableForWrite()
^^^^^^^^^^^^^^^^^^^^^^^
Returns number of **BYTES** available for write.


size_t write_blocking(const void *buffer, size_t size)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Core function implementing blocking write, i.e. waits until all requested data are written.
WARNING: If too many bytes are requested, this can cause WatchDog Trigger Reset!

Returns number of successfully written bytes. Returns 0 on error.

size_t write_nonblocking(const void *buffer, size_t size)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Core function implementing non-blocking write, i.e. writes as much as possible and exits.

Returns number of successfully written bytes. Returns 0 on error.