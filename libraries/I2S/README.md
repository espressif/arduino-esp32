# I2S / IIS / Inter-IC Sound

Inter-IC Sound bus is designed for digital transmission of audio data.


This library is based on Arduino I2S library (see doc [here](https://docs.arduino.cc/learn/built-in-libraries/i2s))

This library mimics the behavior and extends possibilities with ESP-specific functionalities.

**The main differences are:**

| Property         | Arduino     | ESP32 extends              |
| ---------------- | ----------- | -------------------------- |
| Bits per sample  | 8,16,32     | 24                         |
| data channels    | 1 (simplex) | 2 (duplex)                 |
| modes            | Philips     | PDM, ADC/DAC               |
| I2S modules      | 1           | ESP32 (only) has 2 modules |
| Pins             | Fixed       | configurable               |

**In this document you will find overview for this version of library and description of class functions.**

**The bus uses following signals:**

 - **SCK**    - Serial Clock signal.
 - **WS**     - Word Select - switching between Left and Right channel.
 - **SD**     - Serial Data - In Simplex mode this is multiplexed data - depending on function calls this is either input or output.
 - **SD OUT** - In duplex mode this line is only for outgouing data (TX).
 - **SD IN**  -  In duplex mode this line is only for incoming data (RX).
 - **MCLK**   - Master Clock signal - runs on higher frequency than SCK and for most cases is not needed.

**The library uses following data structures:**

 - **Bits per sample (bps)** - determines how many bits is used for each sample (see line below). More bits means greater audio quality, however it is more demanding on memory and timing. The allowed values are `8`, `16`, `24` and `32` i.e. `1`, `2`, `3` or `4` Bytes per sample.
 - **Sample** - single channel data. The size of sample depends on `bits per sample`  the size of single sample in bytes is simply calculated by `bits per sample / 8`.
 - **Frame** - sum of samples across all channels. This library is currently hard-coded to dual channel / stereo, i.e. the number of channels is constantly `2`. The size of frame in Bytes can be calculated as `sample size * number of channels = (bits per sample / 8) * number of channels = (bits per sample / 8) * 2`. For example when `bps=16` the frame has `(16 / 8) * 2 = 4 B`.
 - **DMA buffer** - is used in underlying IDF driver and its size is in frames. The Byte size of single DMA buffer is `DMA buf len * frame size = DMA buf len * sample size * number of channels = DMA buf len * (bits per sample / 8) * number of channels = DMA buf len * (bits per sample / 8) * 2`. By default the `DMA buf len = 128` therefore the Byte size is `128 * (bits per sample / 8) * 2`. For example when set `bps=16` then the Byte size is `128 * (16 / 8) * 2 = 512 B`. The functions `setDMABufferFrameSize` and `setDMABufferSampleSize` can be used to change the size (see function section for more details).
 - **DMA buffer array** - There is more than one DMA buffer. The number of DMA buffers is defined in constant `_I2S_DMA_BUFFER_COUNT` by default to value `2`. If you want to change it modify file `I2S.cpp`. The Byte size of all DMA buffer can be calculated as `_I2S_DMA_BUFFER_COUNT * DMA buf len * (bits per sample / 8) * 2`. Extending the previous example (`bps=16`, default `dma buf len=128`): `2* 128 *(16 / 8) * 2 = 1024 B`
 - **Ring buffer** - is used as an abstraction layer above the DMA buffers. There is one ring buffer for transmitting size and second one for receiving side. The size of a ring buffer will be always automatically set to the sum of all DMA buffers. The size of a single ring buffer in Bytes can be calculated as `DMA buffer len * _I2S_DMA_BUFFER_COUNT * (bits per sample / 8) * number of channels`.

**Usage of underlying IDF driver**

Since all ESP32 code runs on FreeRTOS (operating system handling execution time on CPUs and allowing pseudo-parallel execution) this library uses 1 task which monitors messages from underlying IDF I2S driver which sends them whenever it sends contents of single DMA buffer to output (TX) data line, or when it fills a DMA buffer with samples received on input (RX) data line.
Since Arduino utilizes single sample writes and reads heavily, which would cause unbearable lags, there had to be implemented another layer of ring buffers (one for transmitted data and one for received data). The user of this library writes, or reads from this buffer via functions `write` and `read` (exact variants are described in functions section).
The task, upon receiving the message from IDF I2S driver performs either write data from transmit ring buffer into IDFs I2S driver (into TX DMA buffer) or reads received data from IDF I2S driver (from RX DMA buffer) to the receive ring buffer.
Usage of the ring buffers also enabled implementation of functions `available`, `availableForWrite`, `peak` and single sample write/read.

The IDF I2S driver uses multiple sets of buffers of the same size. The size of ring buffer equals to sum of all those buffers. By default the number of IDF I2S buffers is 2. Usually there should be no need to change this, however if you choose to change it, change the constant `_I2S_DMA_BUFFER_COUNT`  in the library, in `src/I2S.cpp`:

The size of IDF I2S buffer is by default 128. The unit used for this buffer is a frame. A frame means the data of all channels in a WS cycle. For example dual channel 16 bits per sample and default buffer size = 2 * 2 * 128 = 512 Bytes.

From this we can get also size of the ring buffers. As stated their size is equal to the sum of IDF I2S buffers. For the example of dual channel, 16 bps the size of each ring buffer is 1024 B.
The functions `setDMABufferFrameSize` and `setDMABufferSampleSize` allows you to change the size in frames and samples respectively (see detailed description below).

On Arduino and most ESPs you can use object `I2S` to use the I2S module. on ESP32 and ESP32-S3 you have the option to use addititonal object `I2S_1`.
I2S module functionality on each SoC differs, please refer to the following table. More info can be found in [IDF documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2s.html?highlight=dac#overview-of-all-modes).

## Modes (i2s_mode_t)
.. note:: (Arduino MKRZero) First transmitted sample is in Right channel (WS=1). If your audio has swapped left and right channel try feed single zero-value sample before the data stream.

 * **I2S_PHILIPS_MODE** Most common mode, FS signal spans across whole channel period. MSB is transmitted first and data starts SCK falling edge 1 SCK period after WS change. WS 0 = Left channel, 1 = Right channel. See more on [Wikipedia](https://en.wikipedia.org/wiki/I%C2%B2S)
 * **I2S_RIGHT_JUSTIFIED_MODE** Does not work on MKRZero, opened [issue](https://github.com/arduino/ArduinoCore-samd/issues/682). Should be expected to the ooposite of `I2S_LEFT_JUSTIFIED_MODE`.
 * **I2S_LEFT_JUSTIFIED_MODE** Input data belonging to righ channel are ignored and data belonging to left channel are transmitted in both channels. Example: `int16_t sample[] = {0x0001, 0x0002, 0x0003, 0x0004}` samples with value `0x0001` and `0x0003` belong to the right channel and will be ignored. Samples with value `0x0002` and `0x0004` will be transmitted as follows: Right channel (WS=1) `0x0002` followd by Left channel )WS=0) `0x0004`. this pattern repeats.
 * **ADC_DAC_MODE** Outputting and inputting raw analog signal - see [example ADCPlotter](https://github.com/espressif/arduino-esp32/blob/master/libraries/I2S/examples/ADCPlotter/ADCPlotter.ino). Can only be initialized with data pins set to 25 a 26.
 * **PDM_STEREO_MODE** Pulse Density Modulation is basically an over-sampled  1-bit signal. Not to be confused with PCM. (ESP32-C3 supports only Transmit mode - read attempts will time-out)
 * **PDM_MONO_MODE** Same as previous but transmits only 1 channel. TODO explain timing diagram and data interpretation.

.. note:: First 2 samples in a stream are zero value, this will not affect audio quality.

.. note:: Some ESP32 SoCs support PCM, TDM and LCD/Camera modes, however support for those modes is out of scope in this simplified library. If you need to use those mode you will need to use [IDF I2S driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2s.html).


### Overview of I2S Modes:
| SoC      | Philips     | ADC/DAC | PDM TX | PDM RX |
| -------- | ----------- | ------- | ------ | ------ |
| ESP32    | I2S + I2S_1 | I2S     | I2S    | I2S    |
| ESP32-S2 | I2S         | N/A     | N/A    | N/A    |
| ESP32-C3 | I2S         | N/A     | I2S    | N/A    |
| ESP32-S3 | I2S + I2S_1 | N/A     | I2S    | I2S    |

## Pins
ESP I2S has fully configurable pins. There is a group of setter and getter functions for each pin separately, two setters for all pins at once (detailed description below) and one un-setter for MCLK. Calling the setter (or un-setter) will take effect immediately and does not need driver restart.

The MCLK pin is usually not needed and for most cases can be ignored. By default the MCLK pin is not attached to any GPIO. To use the MCLK pin it is has to be explicitly configured using `setMclkPin()`, or by providing last parameter to function `setAllPins()`. The MCLK pin can be attached only to few specific pins, the suggested pin is specified by `PIN_I2S_MCLK` constant (each SoC may have different GPIO number).

### Default pins for SoCs and I2S modules:
**I2S object:**

| SoC                          | SCK | WS | SD(OUT) | SDIN | MCLK |
|:---------------------------- |:---:|:--:|:-------:|:----:|:----:|
| ESP32                        |  18 | 19 |   21    |  34  |    0 |
| ESP32-C3, ESP32-S2, ESP32-S3 |  18 | 19 |    4    |   5  |    0 |

**I2S_1 object:**

| SoC      | SCK | WS | SD(OUT) | SDIN | MCLK |
|:-------- |:---:|:--:|:-------:|:----:|:----:|
| ESP32    |  22 | 23 |    27   |   35 |    1 |
| ESP32-S3 |  36 | 37 |    39   |   40 |   35 |

**DAC pin limitation**

Unlike other modes, the ADC output is hard-wired to GPIO 25 (R) and 26 (L). DAC output is slightly limited too - please refer to the following table. Attempt to initialize I2S in `ADC_DAC_MODE` with SD IN pin set to anything other than allowed GPIO will result in failure.

**ADC pin limitation:**

ADC input uses internally ADC modules - usage of I2S in `ADC_DAC_MODE` may interfere with other usage of these ADC modules. Please refer to the [documentation]() for more info.

| SoC      | GPIOs supporting ADC         |
|:-------- |:----------------------------:|
| ESP32    | 0, 2, 4, 12-15, 25-27, 32-39 |
| ESP32-C3 | 0-5                          |
| ESP32-S2 | 1-20                         |
| ESP32-S3 | 1-20                         |

## Master / Slave[ ](https://en.wiktionary.org/wiki/slave#Etymology) modes
There are two versions of initializer function `begin` - one initializes in master mode and the other one in slave mode (see functions below for details).
The usual use-case is operation in master mode which means that the MCU generates the clock signals which are transmitted to slave devices - usually microphones, speakers or I2S decoders.

In master mode it is required to connect the device with clock (SCK), word-select (WS) and data (SD) signals. In duplex mode (see below) it is necessary to connect two data lines: input data (SDIN) and output data (SDOUT). The shared data (SD) signal acts as output in duplex mode.

In slave mode it is necessary to connect the devices with the same signals as in master mode and also master clock (MCLK) signal. If you should try to operat the MCU in slave mode without MCLK, the MCU would not be able properly deduce the timing only from the SCK and there would be no data output from the slave and also the slave would not be able to read any data.

## Duplex / Simplex
Arduino has only one data pin and the driver is switching between receiving and transmitting upon calling `read` or `write`. On all ESP32s each I2S module has 2 independent data lines, one for transmitting and one for receiving.
For backward compatibility with Arduino we are using DataPin for multiplexed data and DataOutPin + DataInPin for independent duplex communication.
The default mode is simplex and the shared data pin switches function upon calling `read` or `write` same as Arduino.
If you wish to use duplex mode call `setDuplex();` this will change the pin setup to use both data lines. If you want to switch back ti simplex call `setSimplex();` and if you need to get current state call `isDuplex();` (detailed function description below).


## Buffers

### Direct Memory Access buffers
The underlying IDF I2S driver uses a set of Direct Memory Access (DMA) buffers which can read from (in case of transmit line) or written to (in case of receive line) directly by the I2S module without using CPU. This helps to achieve flawless audio experience. However if TX buffers are not filled, or RX buffers emptied fast enough the I2S driver will transmit zeros (silence), or rewrite existing data (missing recorded samples) and this can be perceived as audio artifacts such as digital noise, lags, slowed playback, etc.

The best way is to fill and read the buffers in chunks and let the CPU be used on other tasks. Once the DMA buffers are transmitted and freed for write, or read and stored elsewhere the CPU can fill the TX DMA buffer again and the I2S driver can fill the RX DMA buffers with new incoming samples.

The number of DMA buffers can be from 2 - 128 for each data line (TX & RX) this is set by constant `_I2S_DMA_BUFFER_COUNT` in `I2S.cpp`. The default value is `2` and can be changed only in code - requires recompilation.

The size of each DMA buffer is in sample frames and can be between 8 and 1024. One frame equals to number of channels (in this library always 2) multiplied by Bytes per samples (or `(bits_per_sample/8)`. The default value s `128` and can be changed changed by functions `setDMABufferFrameSize()` and `setDMABufferSampleSize()`, this automatically reinstalls the driver. The DMA buffer size can be read by functions `getDMABufferFrameSize()`, `getDMABufferSampleSize()`, and `getDMABufferByteSize()`. More info can be found in section *FUNCTIONS*.

### Ring buffers buffers
Original Arduino I2S library uses extensively single-sample writes and reads - such usage would result in poor audio quality. Therefore another layer of buffers has been added - ring buffers.
There are two ring buffers - one for transmit line and one for receiving line.

The size of ring buffer is equal to the sum of DMA buffers: `ring_buffer_bytes_size = (CHANNEL_NUMBER * (bits_per_sample/8)) * DMABufferFrameSize * _I2S_DMA_BUFFER_COUNT`.
Maximum size of those buffers can be read with functions `getRingBufferSampleSize()` and `getRingBufferByteSize()`.

To get number of free Bytes in the transmit buffer,  that can be actually written is performed with function `availableForWrite()`. To get number of Bytes ready to be read from receiving ring buffer use function `available()`. Changing the size of ring buffer is not possible directly - it is automatically calculated during driver installation based on DMA Buffer size.

### Known issues
InputSerialPlotter often freezes for a short while - this is caused by the fact that the serial output and the incoming data are not synchronized - portion of the input buffer is written to serial followed by few milliseconds until input data are buffered and ready to be read by the loop.

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

Can be called on both uninitialized and initialized object (before and after `begin`).
The change takes effect immediately and does not need driver restart.

Note: You can use value `-1` for default value. (Default for `MCLK` is detached state).

**Parameter:**

* **int pin**  number of GPIO which should be used for the requested pin setup. Any negative value will set default pin number defined in PIN_I2S_X or PIN_I2S1_X.

**Returns:** 1 on success; 0 on error

**Function list:**

* **int setSckPin(int sckPin)**  Set Clock pin
* **int setFsPin(int fsPin)**  Set Frame Sync (Word Select) pin
* **int setDataPin(int sdPin)**  Set shared Data pin for simplex mode
* **int setDataOutPin(int outSdPin)**  Set Data Output pin for duplex mode
* **int setDataInPin(int inSdPin)**  Set Data Input pin for duplex mode
* **int setMclkPin(int sckPin)**  Set Master Clock pin (by default not used-no pin attached)

***
#### int setAllPins()

*Change pin setup for all pins at one call using default values set constants in I2S.h*

Can be called only on initialized object (after `begin()`).
The change takes effect immediately and does not need driver restart.
The `MCLK` pin will be un-set - i.e. detached from any GPIO.

**Returns:** 1 on success; 0 on error
***
#### int setAllPins(int sckPin, int fsPin, int sdPin, int outSdPin, int inSdPin, int mclkPin=-1)

*Change pin setup for all pins at one call.*

Can be called only on initialized object (after `begin()`).
The change takes effect immediately and does not need driver restart.
Note: You can use value `-1` for default value. (Default for `MCLK` is detached state).

**Parameters:**

* **int sckPin**  Clock pin
* **int fsPin**  Frame Sync (Word Select) pin
* **int sdPin**  Shared Data pin for simplex mode
* **int outSdPin**  Data Output pin for duplex mode
* **int inSdPin**  Data Input pin for duplex mode
* **int mclkPin**  Master Clock pin - this parameter has default value `-1` i.e. detached state (this parameter can be omitted in function call)

**Returns:** 1 on success; 0 on error

***
#### int unSetMclkPin()
*Unset MCLK pin making it available for other use*

Can be called on both uninitialized and initialized object (before and after begin).
The change takes effect immediately and does not need driver restart.

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
* **int getMclkPin()**  Get Master Clock pin

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
#### virtual int availableSamplesForWrite()

**Returns:** number of samples that can be written into the ring buffer.
***
#### virtual size_t write(uint8_t data)

*Write single sample of 8 bit size.*

This function is **blocking** - if there is not enough space in ring buffer the function will wait until it can write the sample.

**Parameter:**
* **uint8_t sample**  The sample to be sent

**Returns:** 1 on successful write; 0 on error = did not write the sample to ring buffer

.. note:: This functions is used in many examples for it's simplicity, but it's use is discouraged for performance reasons.

Please consider sending data in arrays using function `size_t write(const uint8_t *buffer, size_t size)`
***
#### size_t write(int32_t)

*Write single sample of up to 32 bit size.*

This function is **blocking** - if there is not enough space in ring buffer the function will wait until it can write the sample.

**Parameter:**
**int32_t sample**  The sample to be sent

**Returns:** Number of written bytes, if successful the value will be equal to `bitsPerSample/8`

.. note:: This functions is used in many examples for it's simplicity, but it's use is discouraged for performance reasons.

Please consider sending data in arrays using function `size_t write(const uint8_t *buffer, size_t size)`

.. note:: If used with `bits_per_sample == 24` then the Most Significant Byte will be ignored. For example `int32_t sample = 0xFFEEDDCC` will be transferred as an array: `sample[0]=0xCC, sample[1]=0xDD, sample[2]=0xEE`

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
#### int setDMABufferFrameSize(int DMABufferFrameSize)

*Change the size of DMA buffers.* The unit is number of sample frames `(CHANNEL_NUMBER * (bits_per_sample/8))`

**Parameter:**

* **int DMABufferFrameSize** The number of frames in one DMA buffer. The value must be between 8 and 1024 (including).

**Returns:** 1 on successful setup; 0 on error = could not install driver with new settings or could not take mutex.

The resulting Bytes size of ring buffers can be calculated:

`ring_buffer_bytes_size = (CHANNEL_NUMBER * (bits_per_sample/8)) * DMABufferFrameSize * _I2S_DMA_BUFFER_COUNT`

**Example:**

  * This library statically set to *dual channel*, therefore `CHANNEL_NUMBER` is always **2**
  * For this example let's have `bits_per_sample` set to **16**
  * Default value of `DMABufferFrameSize` is **128**
  * Default value of `_I2S_DMA_BUFFER_COUNT` is **2**
```
ring_buffer_bytes_size = (bits_per_sample / 8) * DMABufferFrameSize * CHANNEL_NUMBER * _I2S_DMA_BUFFER_COUNT
        1024           = (     16         / 8) *        128         *        2       *           2
```
***
#### int setDMABufferSampleSize(int DMABufferSampleSize)

*Change the size of DMA buffers.* The unit is number of samples `bits_per_sample/8`

This function simply calls `setDMABufferFrameSize(DMABufferSampleSize / CHANNEL_NUMBER);`

**Parameter:**

* **int DMABufferFrameSize** The number of samples in one DMA buffer. The value must be always even (multiple of 2) and between 16 and 2048 (including).

**Returns:** 1 on successful setup; 0 on error = could not install driver with new settings or could not take mutex.

**Example:**

  * This library statically set to *dual channel*, therefore `CHANNEL_NUMBER` is always **2**
  * For this example let's have `bits_per_sample` set to **16**
  * Same value as default can be achieved with parameter value `DMABufferSampleSize` set to **256**
  * Default value of `_I2S_DMA_BUFFER_COUNT` is **2**
```
ring_buffer_bytes_size = (bits_per_sample / 8) * DMABufferSampleSize * _I2S_DMA_BUFFER_COUNT
        1024           = (     16         / 8) *       256           *         2
```
***
#### int getDMABufferFrameSize()
*Get size of single DMA buffer.*

The unit is number of sample frames: Bytes size of 1 frame = `(CHANNEL_NUMBER * (bits_per_sample / 8))`

For more info see `setDMABufferFrameSize`
***
#### int getDMABufferSampleSize()
*Get size of single DMA buffer.*

The unit is number of samples: 1 sample = `(bits_per_sample / 8)`

For more info see setDMABufferFrameSize
***
#### int getDMABufferByteSize()
*Get size of single DMA buffer in Bytes.*

For more info see setDMABufferFrameSize
***
#### int getRingBufferSampleSize();
*Get ring buffer size.*

The unit is number of samples: 1 sample = `(bits_per_sample / 8)`

For more info see setDMABufferFrameSize
***
#### int getRingBufferByteSize();
Get ring buffer size in Bytes.

For more info see setDMABufferFrameSize
***
#### int getI2SNum()

Get the ID number of I2S module used for particular object.

Object `I2S` **returns** value `0`

Object `I2S_1` **returns** value `1`

Only ESP32 and ESP32-S3 have two modules, other SoCs have only one I2S module controlled by object `I2S` and the return value will always be `0`, the second object `I2S_1` does not exist.
***
#### bool isInitialized()

**Returns** `true` if I2S module is correctly initialized and ready for use (function `begin()` was called and returned `1`)

**Returns** `false` if I2S module has not yet been initialized (function `begin()` was not called, or returned `0`), or it has been de-initialized (function `end()` was called)
***
#### int getSampleRate()
**Returns:** 0 on un-initialized object, or if the object is initialized as slave.

On initialized master object **returns** sample rate in Hz (same value which was passed as argument with begin() function)
***
#### int getBitsPerSample()

**Returns:** 0 on un-initialized object.

On initialized object **returns** bits per sample (same value which was passed as argument with begin() function)