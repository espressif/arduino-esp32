/*
  Copyright (c) 2016 Arduino LLC.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef _I2S_H_INCLUDED
#define _I2S_H_INCLUDED

#include <Arduino.h>
#include "freertos/ringbuf.h"

namespace esp_i2s {
  #include "driver/i2s.h" // ESP specific i2s driver
}

// Default pins
#ifndef PIN_I2S_SCK
  #define PIN_I2S_SCK 14
#endif

#ifndef PIN_I2S_FS
  #if CONFIG_IDF_TARGET_ESP32S2
    #define PIN_I2S_FS 27
  #else
    #define PIN_I2S_FS 25
  #endif
#endif

#ifndef PIN_I2S_SD
  #define PIN_I2S_SD 26
#endif

#ifndef PIN_I2S_SD_OUT
  #define PIN_I2S_SD_OUT 26
#endif

#ifndef PIN_I2S_SD_IN
  #define PIN_I2S_SD_IN 35 // Pin 35 is only input!
#endif

typedef enum {
  I2S_PHILIPS_MODE,
  I2S_RIGHT_JUSTIFIED_MODE,
  I2S_LEFT_JUSTIFIED_MODE,
  ADC_DAC_MODE,
  PDM_STEREO_MODE,
  PDM_MONO_MODE
} i2s_mode_t;

class I2SClass : public Stream
{
public:
  // The device index and pins must map to the "COM" pads in Table 6-1 of the datasheet
  I2SClass(uint8_t deviceIndex, uint8_t clockGenerator, uint8_t sdPin, uint8_t sckPin, uint8_t fsPin);

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
  virtual size_t write(uint8_t);
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
private:
  #if (SOC_I2S_SUPPORTS_ADC && SOC_I2S_SUPPORTS_DAC)
    int _gpioToAdcUnit(gpio_num_t gpio_num, esp_i2s::adc_unit_t* adc_unit);
    int _gpioToAdcChannel(gpio_num_t gpio_num, esp_i2s::adc_channel_t* adc_channel);
  #endif
  int begin(int mode, int sampleRate, int bitsPerSample, bool driveClock);

  int _enableTransmitter();
  int _enableReceiver();
  void _onTransferComplete();

  int _createCallbackTask();

  static void onDmaTransferComplete(void*);
  int _installDriver();
  void _uninstallDriver();
  void _setSckPin(int sckPin);
  void _setFsPin(int fsPin);
  void _setDataPin(int sdPin);
  void _setDataOutPin(int outSdPin);
  void _setDataInPin(int inSdPin);
  int  _applyPinSetting();

private:
  typedef enum {
    I2S_STATE_IDLE,
    I2S_STATE_TRANSMITTER,
    I2S_STATE_RECEIVER,
    I2S_STATE_DUPLEX
  } i2s_state_t;

  int _deviceIndex;
  int _sdPin;
  int _inSdPin;
  int _outSdPin;
  int _sckPin;
  int _fsPin;

  i2s_state_t _state;
  int _bitsPerSample;
  uint32_t _sampleRate;
  int _mode;

  uint16_t _buffer_byte_size;

  bool _driverInstalled; // Is IDF I2S driver installed?
  bool _initialized; // Is everything initialized (callback task, I2S driver, ring buffers)?
  TaskHandle_t _callbackTaskHandle;
  QueueHandle_t _i2sEventQueue;
  SemaphoreHandle_t _i2s_general_mutex;
  RingbufHandle_t _input_ring_buffer;
  RingbufHandle_t _output_ring_buffer;
  int _i2s_dma_buffer_size;
  bool _driveClock;
  uint32_t _peek_buff;
  bool _peek_buff_valid;

  void _tx_done_routine(uint8_t* prev_item);
  void _rx_done_routine();

  uint16_t _nesting_counter;
  void _take_if_not_holding();
  void _give_if_top_call();
  void _post_read_data_fix(void *input, size_t *size);
  void _fix_and_write(void *output, size_t size, size_t *bytes_written = NULL, size_t *actual_bytes_written = NULL);

  void (*_onTransmit)(void);
  void (*_onReceive)(void);
};

extern I2SClass I2S;

#endif
