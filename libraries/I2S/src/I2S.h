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
namespace esp_i2s {
  #include "driver/i2s.h" // ESP specific i2s driver
}
#include "freertos/semphr.h"


#define I2S_HAS_SET_BUFFER_SIZE 1

#ifdef CONFIG_IDF_TARGET_ESP32
  #define I2S_INTERFACES_COUNT 2
#endif

#ifdef CONFIG_IDF_TARGET_ESP32S2
  #define I2S_INTERFACES_COUNT 1
#endif

#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define _I2S_EVENT_QUEUE_LENGTH 10

typedef enum {
  I2S_PHILIPS_MODE,
  I2S_RIGHT_JUSTIFIED_MODE,
  I2S_LEFT_JUSTIFIED_MODE,
  I2S_ADC_DAC
} i2s_mode_t;

class I2SClass : public Stream
{
public:
  // the device index and pins must map to the "COM" pads in Table 6-1 of the datasheet
  I2SClass(uint8_t deviceIndex, uint8_t clockGenerator, uint8_t sdPin, uint8_t sckPin, uint8_t fsPin);
  I2SClass(uint8_t deviceIndex, uint8_t clockGenerator, uint8_t inSdPin, uint8_t outSdPin, uint8_t sckPin, uint8_t fsPin); // set duplex
  // the SCK and FS pins are driven as outputs using the sample rate
  int begin(int mode, long sampleRate, int bitsPerSample);
  // the SCK and FS pins are inputs, other side controls sample rate
  int begin(int mode, int bitsPerSample);

  // change pin setup (default is Simplex)
  int setHalfDuplex(uint8_t inSdPin, uint8_t outSdPin);
  int setSimplex(uint8_t sdPin);

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

  void onTransmit(void(*)(void));
  void onReceive(void(*)(void));

  void setBufferSize(int bufferSize);
private:
  int begin(int mode, long sampleRate, int bitsPerSample, bool driveClock);

  int enableTransmitter();
  int enableReceiver();
  void onTransferComplete();

  void destroyCallbackTask();
  void createCallbackTask();

  static void onDmaTransferComplete(void*);

private:
  typedef enum {
    I2S_STATE_IDLE,
    I2S_STATE_TRANSMITTER,
    I2S_STATE_RECEIVER,
    I2S_STATE_DUPLEX
  } i2s_state_t;

  static int _beginCount;

  int _deviceIndex;
  int _clockGenerator;
  int _sdPin;
  int _inSdPin;
  int _outSdPin;
  int _sckPin;
  int _fsPin;


  i2s_state_t _state;
  int _dmaChannel;
  int _bitsPerSample;
  long _sampleRate;
  int _mode;

  bool _initialized;
  TaskHandle_t _callbackTaskHandle;
  QueueHandle_t _i2sEventQueue;
  QueueHandle_t _task_kill_cmd_semaphore_handle;

  void (*_onTransmit)(void);
  void (*_onReceive)(void);
};

// "I2S" is already defined by the CMSIS device, undefine it so the I2SClass
// instance can be called I2S
#undef I2S

extern I2SClass I2S;

#endif
