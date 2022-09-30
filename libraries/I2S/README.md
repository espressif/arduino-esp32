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

## Modes
 * I2S_PHILIPS_MODE Most common mode see elsewhere
 * I2S_RIGHT_JUSTIFIED_MODE ?
 * I2S_LEFT_JUSTIFIED_MODE ?
 * ADC_DAC_MODE outputting and inputting raw analog signal - see example ADCPlotter
 * PDM_STEREO_MODE Pulse Density Modulation is basically oversampled  1bit signal. not to be confused with PCM.
 * PDM_MONO_MODE Same as previous but transmits only 1 channel TODO explain timing diagram and data interpretation

## Functions
copy paste from .h
  // Init in MASTER mode: the SCK and FS pins are driven as outputs using the sample rate
  int begin(int mode, int sampleRate, int bitsPerSample);

  // Init in SLAVE mode: the SCK and FS pins are inputs, other side controls sample rate
  int begin(int mode, int bitsPerSample);

  // change pin setup and mode (default is Half Duplex)
  // Can be called only on initialized object (after begin)
  int setSckPin(int sckPin);
  int setFsPin(int fsPin);
  int setDataPin(int sdPin); // shared data pin for simplex
  int setDataOutPin(int outSdPin);
  int setDataInPin(int inSdPin);

  int setAllPins();
  int setAllPins(int sckPin, int fsPin, int sdPin, int outSdPin, int inSdPin);

  int getSckPin();
  int getFsPin();
  int getDataPin();
  int getDataOutPin();
  int getDataInPin();

  int setDuplex();
  int setSimplex();
  int isDuplex();

  void end();

  // from Stream
  virtual int available();
  virtual int read();
  virtual int peek();
  virtual void flush();

  // from Print
  virtual size_t write(uint8_t data);
  virtual size_t write(const uint8_t *buffer, size_t size);

  virtual int availableForWrite();

  int read(void* buffer, size_t size);

  //size_t write(int);
  size_t write(int32_t);
  size_t write(const void *buffer, size_t size);
  size_t write_blocking(const void *buffer, size_t size);
  size_t write_nonblocking(const void *buffer, size_t size);

  void onTransmit(void(*)(void));
  void onReceive(void(*)(void));

  int setBufferSize(int bufferSize);
  int getBufferSize();