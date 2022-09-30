# I2S / IIS / Inter-IC Sound

This library is based on Arduino I2S library (see doc [here](https://docs.arduino.cc/learn/built-in-libraries/i2s))
This library mimics the behavior and extends possibilities with ESP-specific functionalities.

The main differences are:
Property           Arduino      ESP32 extends
bits per sample    8,16,32        24
data channels      1 (simplex)   2(duplex)
modes              Philips       PDM, ADC/DAC
I2S modules        1             2 (only for ESP32, other SoC have also only 1)
pins               fixed         configurable

In this document you will find overview for this version of library and description of class functions.

Since all ESP32 code runs on FreeRTOS (operating system handling execution time on CPUs and allowing pseudo-parallel execution) this library uses 1 task which monitors messages from underlying IDF I2S driver which sends them whenever it sends sample from buffer to data line, or when it fills buffer with samples received from data line.
Since Arduino utilizes single sample writes and reads heavily, which would case unbearable lags there had to be implemented another layer of ring buffers (one for transmitted data and one for received data). The user of this library writes, or reads from this buffer via functions `write` and `read` (exact variants are described below).
The task, upon receiving the message from IDF I2S driver performs either flush data from transmit ring buffer to IDF I2S driver or received data from IDF I2S driver to receive ring buffer.
Usage of the ring buffers also enabled implementation of functions `available`, `availableForWrite`, `peak` and single sample write/read.

The IDF I2S driver uses multiple sets of buffer of the same size. The size of ring buffer equals to sum of all those buffers. By default the number of IDF I2S buffers is 2. Usually there should be no need to change this, however if you choose to change it, change the constant `_I2S_DMA_BUFFER_COUNT`  in the library, in `src/I2S.cpp`:

The size of IDF I2S buffer is by default 128. The unit used for this buffer is a frame. A frame means the data of all channels in a WS cycle. For example dual channel 16 bits per sample and default buffer size = 2 * 2 * 128 = 512 Bytes.

From this we can get also size of the ring buffers. As stated their size is equal to the sum of IDF I2S buffers. For the example of dual channel, 16 bps the size of each ring buffer is 1024 B.
The function `setBufferSize` allows you to change the size in frames (see detailed description below).

On Arduino and most ESPs you can use object `I2S` to use the I2S module. on ESP32 and ESP32-S3 you have the option to use addititonal object `I2S1`.
I2S module functionality on each SoC differs, please refer to [IDF documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2s.html?highlight=dac#overview-of-all-modes) for an overview.

## Pins
ESP I2S has fully configurable pins. There is a group of setter and getter functions for each pin separately and one setter for all pins at once (detailed description below). Calling the setter will take effect immediately and does not need driver restart.
Default pins are setup as follows:
### Common I2S object:
| SCK | WS | SD(OUT) | SDIN | SoC                          |
| --- | -- | ------- | ---- |:---------------------------- |
|  19 | 21 |   22    |  23  | ESP32                        |
|  19 | 21 |    4    |   5  | ESP32-C3, ESP32-S2, ESP32-S3 |


### I2S1 object:
| SCK | WS | SD(OUT) | SDIN | SoC      |
| --- | -- | ------- | ---- |:-------- |
|  18 | 22 |    25   |   26 | ESP32    |
|  36 | 37 |    39   |   40 | ESP32-S3 |


## Duplex / Simplex
Arduino has only one data pin and the driver is switching between receiving and transmitting upon calling `read` or `write`. On all ESP32s each I2S module has 2 independent data lines, one for transmitting and one for receiving.
For backward compatibility with Arduino we are using DataPin for multiplexed data and DataOutPin + DataInPin for independent duplex communication.
The default mode is simplex and the shared data pin switches function upon calling `read` or `write` same as Arduino.
If you wish to use duplex mode call `setDuplex();` this will change the pin setup to use both data lines. If you want to switch back ti simplex call `setSimplex();` and if you need to get current state call `isDuplex();` (detailed function description below).

## Modes (i2s_mode_t)

 * **I2S_PHILIPS_MODE** Most common mode, FS signal spans across whole channel period. TODO more info
 * **I2S_RIGHT_JUSTIFIED_MODE** ? TODO
 * **I2S_LEFT_JUSTIFIED_MODE** ? TODO
 * **ADC_DAC_MODE** Outputting and inputting raw analog signal - see example ADCPlotter
 * **PDM_STEREO_MODE** Pulse Density Modulation is basically an over-sampled  1-bit signal. Not to be confused with PCM.
 * **PDM_MONO_MODE** Same as previous but transmits only 1 channel. TODO explain timing diagram and data interpretation

## Functions

#### I2SClass(uint8_t deviceIndex, uint8_t clockGenerator, uint8_t sdPin, uint8_t sckPin, uint8_t fsPin)

Constructor - initializes object with default values

**Parameters:**

* **uint8_t deviceIndex**  In case the SoC has more I2S modules, specify which one is instantiated. Possible values are "0" (for all ESPs) and "1" (only for ESP32 and ESP32-S3)
* **uint8_t clockGenerator**  Has no meaning for ESP and is kept only for compatibility
* **uint8_t sdPin**  Shared data pin used for simplex mode
* **uint8_t sckPin**  Clock pin
* **uint8_t fsPin**  Frame Sync (Word Select) pin

**Default settings:**

* Input data pin (used for duplex mode) is initialized with `PIN_I2S_SD_IN`
* Out data pin (used for duplex mode) is initialized with `PIN_I2S_SD`
* Mode = `I2S_PHILIPS_MODE`
* Buffer size = `128`

***
#### int begin(int mode, int sampleRate, int bitsPerSample)

*Init in MASTER mode.*

The SCK and FS pins are driven as outputs using the sample rate.
Initializes IDF I2S driver, creates ring buffers and callback handler task.

**Parameters:**

* **int mode**  Operation mode (Phillips, Left/Right Justified, ADC+DAC,PDM) see **Modes** for exact enumerations
* **int sampleRate**  sampling frequency in Hz. Common values are 8000,11025,16000,22050,32000,44100,64000,88200,128000
* **int bitsPerSample**  Number of bits per one sample (one channel). Possible values are 8,16,24,32

**Returns:** 1 on success; 0 on error
***
#### int begin(int mode, int bitsPerSample)

*Init in SLAVE mode[.](https://en.wiktionary.org/wiki/slave#Etymology)*

The SCK and FS pins are inputs and must be controlled(generated) be external source (MASTER device).
Initializes IDF I2S driver, creates ring buffers and callback handler task.

**Parameters:**

* **int mode**  Operation mode (Phillips, Left/Right Justified, ADC+DAC,PDM) see i2s_mode_t for exact enumerations
* **int bitsPerSample**  Number of bits per one sample (one channel). Possible values are 8,16,24,32

**Returns:** 1 on success; 0 on error
***
#### void end()

*De-initialize IDF I2S driver, frees ring buffers and terminates callback handler task.*
***
#### int setXPin()

*Change pin setup for each pin separately.*

Can be called only on initialized object (after `begin()`).
The change takes effect immediately and does not need driver restart.

**Parameter:**

* **int pin**  number of GPIO which should be used for the requested pin setup

**Returns:** 1 on success; 0 on error

**Function list:**

* **int setSckPin(int sckPin)**  Set Clock pin
* **int setFsPin(int fsPin)**  Set Frame Sync (Word Select) pin
* **int setDataPin(int sdPin)**  Set shared Data pin for simplex mode
* **int setDataOutPin(int outSdPin)**  Set Data Output pin for duplex mode
* **int setDataInPin(int inSdPin)**  Set Data Input pin for duplex mode

***
#### int setAllPins()

*Change pin setup for all pins at one call using default values set constants in I2S.h*

Can be called only on initialized object (after `begin()`).
The change takes effect immediately and does not need driver restart.

**Returns:** 1 on success; 0 on error
***
#### int setAllPins(int sckPin, int fsPin, int sdPin, int outSdPin, int inSdPin)

*Change pin setup for all pins at one call.*

Can be called only on initialized object (after `begin()`).
The change takes effect immediately and does not need driver restart.

**Parameters:**

* **int sckPin**  Clock pin
* **int fsPin**  Frame Sync (Word Select) pin
* **int sdPin**  Shared Data pin for simplex mode
* **int outSdPin**  Data Output pin for duplex mode
* **int inSdPin**  Data Input pin for duplex mode

**Returns:** 1 on success; 0 on error
***
#### int getXPin()

*Get current pin GPIO number*

**Returns:** the GPIO number of requested pin

**Function list:**

* **int getSckPin()**  Get Clock pin
* **int getFsPin()**  Get Frame Sync (Word Select) pin
* **int getDataPin()**  Get shared Data pin for simplex mode
* **int getDataOutPin()**  Get Data Output pin for duplex mode
* **int getDataInPin()**  Get Data Input pin for duplex mode

***
#### int setDuplex()

*Change mode to duplex* (default is simplex)

**Returns:** 1 on success; 0 on error
***
#### int setSimplex()
Change mode to Simplex

**Returns:** 1 on success; 0 on error
***
#### int isDuplex()

*Get current mode*

**Returns:** 1 if current mode is Duplex; 0 If current mode is not Duplex
***
#### int available()
**Returns:** number of Bytes available to read from ring buffer.

.. note:: The ring buffer is filled automatically by handler task from IDF I2S driver.

***
#### int peek()

Reads a single sample from ring buffer and keeps it available for future read (i.e. does not remove the sample from ring buffer)

**Returns:** First sample from ring buffer

.. note:: The ring buffer is filled automatically by handler task from IDF I2S driver.

***
#### int read()

*Reads a single sample* from ring buffer and removes it from the ring buffer.

**Returns:** First sample from ring buffer

.. note:: The ring buffer is filled automatically by handler task from IDF I2S driver.

***
#### int read(void* buffer, size_t size)

*Reads an array of samples* from ring buffer and removes them from the ring buffer.

**Parameters:**

* **[OUT] void* buffer**  Buffer into which the samples will be copied. The buffer must allocated before calling this function!
* **[IN] size_t size**  Requested number of bytes to be read

**Returns:** Number of bytes that were actually read.

.. note:: Always check the returned value!

***
#### virtual int availableForWrite()

**Returns:** number of bytes that can be written into the ring buffer.
***
#### virtual size_t write(uint8_t data)

*Write single sample of 8 bit size.*

This function is **blocking** - if there is not enough space in ring buffer the function will wait until it can write the sample.

**Parameter:**
* **uint8_t data**  The sample to be sent

**Returns:** 1 on successful write; 0 on error = did not write the sample to ring buffer

.. note:: This functions is used in many examples for it's simplicity, but it's use is discouraged for performance reasons.

Please consider sending data in arrays using function `size_t write(const uint8_t *buffer, size_t size)`
***
#### size_t write(int32_t)

*Write single sample of up to 32 bit size.*

This function is **blocking** - if there is not enough space in ring buffer the function will wait until it can write the sample.

**Parameter:**
* **int32_t data**  The sample to be sent

**Returns:** Number of written bytes, if successful the value will be equal to bitsPerSample/8

.. note:: This functions is used in many examples for it's simplicity, but it's use is discouraged for performance reasons.

Please consider sending data in arrays using function `size_t write(const uint8_t *buffer, size_t size)`
***
#### size_t write(const uint8_t* buffer, size_t size)

*Write array of samples.*

This function is **non-blocking** - the function might write only portion of samples into ring buffer, or potentially none at all. Do check the returned value at all times!

**Parameters:**

* **uint8_t* buffer**  Array of samples
* **size_t size**  Number of bytes in array

**Returns:** Number of bytes successfully written to ring buffer.

.. note:: This is the preferred function for writing samples.

***
#### size_t write(const void* buffer, size_t size)

*Write array of samples.*

This function is **non-blocking** - the function might write only portion of samples into ring buffer, or potentially none at all. Do check the returned value at all times!

**Parameters:**
* **void* buffer**  Array of samples
* **size_t size**  Number of bytes in array

**Returns:** Number of bytes successfully written to ring buffer.

.. note:: This is the preferred function for writing samples.

***
#### void flush()

*Force-write data* from ring buffer to IDF I2S driver.

This function is useful when sending low amount of data, however such use will lead to low quality audio.

.. note:: The ring buffer is emptied (sent) automatically by handler task from IDF I2S driver.

***
#### void onTransmit(void(\*)(void))

*Callback handle* which will be used each time when the IDF I2S driver **transmits** data from buffer.
***
#### void onReceive(void(\*)(void))

*Callback handle* which will be used each time when the IDF I2S driver **receives** data into buffer.
***
#### int setBufferSize(int bufferSize)

*Change the size of buffers.* The unit is number of sample frames `(number_of_channels * (bits_per_sample/8))`

The resulting Bytes size of ring buffers can be calculated:

`ring_buffer_bytes_size = (number_of_channels * (bits_per_sample/8)) * bufferSize * _I2S_DMA_BUFFER_COUNT`

*Example:* default value of `_I2S_DMA_BUFFER_COUNT` is **2**, default value of `bufferSize` is **128**; for *dual channel*, *16 bps* we will get
```
ring_buffer_bytes_size = (number_of_channels * (bits_per_sample/8)) * bufferSize * _I2S_DMA_BUFFER_COUNT
        1024           = (       2           * (     16        /8)) *    128     *         2
```
***
#### int getBufferSize()

*Get buffer size.* The unit is number of sample frames `(number_of_channels * (bits_per_sample/8))`

For more info see `setBufferSize`
