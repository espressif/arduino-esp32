# I2S  / IIS / Inter-IC Sound

This library is based on Arduino I2S library (see doc [here](https://docs.arduino.cc/learn/built-in-libraries/i2s))
This library mimics the behavior and extends possibilities with ESP-specific functionalities.
For example Arduino I2S can operate on 8, 16 and 32 bits per sample, ESP adds 24 bits per sample.
In this document you will find description of functionality which is not available in Arduino and therefore is not described in the Arduino documentation.

note: Arduino describes:
I2S.begin(mode, sampleRate, bitsPerSample); // controller device
I2S.begin(mode, bitsPerSample); // peripheral device

I2S.end()

I2S.available()

I2S.write(val) // blocking
I2S.write(buf, len) // not blocking

I2S.availableForWrite()

onTransmit(handler)

onReceive(handler)

Modes:  I2S_PHILIPS_MODE,
  I2S_RIGHT_JUSTIFIED_MODE,
  I2S_LEFT_JUSTIFIED_MODE

// What we have +

modes:
	I2S_PHILIPS_MODE, (similar to PCM)
  I2S_RIGHT_JUSTIFIED_MODE,
  I2S_LEFT_JUSTIFIED_MODE,
  ++++++++++++++++++++++++++++++
  ADC_DAC_MODE outputting and inputting raw analog signal - see example ADCPlotter
  PDM_STEREO_MODE, not to be confused with PCM. PDM is oversampled  1bit audio
  PDM_MONO_MODE

ESP32 = 2x I2S = I2S + I2S1
flush
virtual size_t write(const uint8_t *buffer, size_t size); = more efficient
same for  read
  size_t write_blocking(const void *buffer, size_t size);
  size_t write_nonblocking(const void *buffer, size_t size);

  internall buffers
  int setBufferSize(int bufferSize);
  int getBufferSize();

  variable pins:
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