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

  // change pin setup and mode (default is Half Duplex)
  // Can be called only on initialized object (after begin)
  int setStateDuplex();
  int setAllPins();
  int setAllPins(int sckPin, int fsPin, int inSdPin, int outSdPin);
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

  int _deviceIndex;
  int _sdPin;
  int _inSdPin;
  int _outSdPin;
  int _sckPin;
  int _fsPin;

  i2s_state_t _state;
  int _bitsPerSample;
  long _sampleRate;
  int _mode;

  uint16_t _buffer_byte_size;
  uint16_t _output_buffer_pointer;
  uint16_t _input_buffer_pointer;
  size_t _read_available;
  SemaphoreHandle_t _in_buf_semaphore;
  SemaphoreHandle_t _out_buf_semaphore;
  void *_inputBuffer;
  void *_outputBuffer;

  bool _initialized;
  TaskHandle_t _callbackTaskHandle;
  QueueHandle_t _i2sEventQueue;
  QueueHandle_t _task_kill_cmd_semaphore_handle;

  void (*_onTransmit)(void);
  void (*_onReceive)(void);
};

extern I2SClass I2S;

#endif
