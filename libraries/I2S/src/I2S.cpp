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

#include <Arduino.h>
#include <wiring_private.h>
#include "I2S.h"
#include "freertos/semphr.h"

#define _I2S_EVENT_QUEUE_LENGTH 16
#define _I2S_DMA_BUFFER_SIZE 512 // BUFFER SIZE must be between 200 and 1024
// (Theoretically it could be above 8, but for some reason sizes below 200 results in frozen callback task)
// And values bellow 500 may result in low quality audio

#define _I2S_DMA_BUFFER_COUNT 4 // BUFFER COUNT must be between 2 and 128
#define I2S_INTERFACES_COUNT SOC_I2S_NUM

#ifndef I2S_DEVICE
  #define I2S_DEVICE 0
#endif

#ifndef I2S_CLOCK_GENERATOR
  #define I2S_CLOCK_GENERATOR 0 // does nothing for ESP
#endif

I2SClass::I2SClass(uint8_t deviceIndex, uint8_t clockGenerator, uint8_t sdPin, uint8_t sckPin, uint8_t fsPin) :
  _deviceIndex(deviceIndex),
  _sdPin(sdPin),   // shared data pin
  _inSdPin(-1),    // input data pin
  _outSdPin(-1),   // output data pin
  _sckPin(sckPin), // clock pin
  _fsPin(fsPin),   // frame (word) select pin

  _state(I2S_STATE_IDLE),
  _bitsPerSample(0),
  _sampleRate(0),
  _mode(I2S_PHILIPS_MODE),

  _buffer_byte_size(0),

  _initialized(false),
  _callbackTaskHandle(NULL),
  _i2sEventQueue(NULL),
  _task_kill_cmd_semaphore_handle(NULL),
  _input_ring_buffer(NULL),
  _output_ring_buffer(NULL),

  _onTransmit(NULL),
  _onReceive(NULL)
{
}

I2SClass::I2SClass(uint8_t deviceIndex, uint8_t clockGenerator, uint8_t inSdPin, uint8_t outSdPin, uint8_t sckPin, uint8_t fsPin) : // set duplex
  _deviceIndex(deviceIndex),
  _sdPin(inSdPin),     // shared data pin
  _inSdPin(inSdPin),   // input data pin
  _outSdPin(outSdPin), // output data pin
  _sckPin(sckPin),     // clock pin
  _fsPin(fsPin),       // frame (word) select pin

  _state(I2S_STATE_DUPLEX),
  _bitsPerSample(0),
  _sampleRate(0),
  _mode(I2S_PHILIPS_MODE),

  _buffer_byte_size(0),

  _initialized(false),
  _callbackTaskHandle(NULL),
  _i2sEventQueue(NULL),
  _task_kill_cmd_semaphore_handle(NULL),
  _input_ring_buffer(NULL),
  _output_ring_buffer(NULL),

  _onTransmit(NULL),
  _onReceive(NULL)
{
}

int I2SClass::createCallbackTask()
{
  int stack_size = 10000;
  if(_callbackTaskHandle == NULL){
    if(_task_kill_cmd_semaphore_handle == NULL){
      _task_kill_cmd_semaphore_handle = xSemaphoreCreateBinary();
      if(_task_kill_cmd_semaphore_handle == NULL){
        log_e("Could not create semaphore");
        return 0; // ERR
      }
    }

    xTaskCreate(
      onDmaTransferComplete, // Function to implement the task
      "onDmaTransferComplete", // Name of the task
      stack_size,  // Stack size in words
      NULL,  // Task input parameter
      2,  // Priority of the task
      &_callbackTaskHandle  // Task handle.
      );
    if(_callbackTaskHandle == NULL){
      log_e("Could not create callback task");
      return 0; // ERR
    }
  }
  return 1; // OK
}

void I2SClass::destroyCallbackTask()
{
  if(_callbackTaskHandle != NULL && xTaskGetCurrentTaskHandle() != _callbackTaskHandle){
    xSemaphoreGive(_task_kill_cmd_semaphore_handle);
    while(_callbackTaskHandle != NULL){
      ; // wait until task ends itself properly
    }
    vSemaphoreDelete(_task_kill_cmd_semaphore_handle); // delete semaphore after usage
    _task_kill_cmd_semaphore_handle = NULL; // prevent usage of uninitialized (deleted) semaphore
  }else{ // callback handle check
    log_e("Could not destroy callback");
  }
}

int I2SClass::begin(int mode, long sampleRate, int bitsPerSample)
{
  // master mode (driving clock and frame select pins - output)
  return begin(mode, sampleRate, bitsPerSample, true);
}

int I2SClass::begin(int mode, int bitsPerSample)
{
  log_e("ERROR I2SClass::begin Audio in Slave mode is not implemented for ESP\n\
         Note: If it is NOT your intention to initialize in slave mode, you are probably missing <sampleRate> parameter - see the declaration below\
         \tint I2SClass::begin(int mode, long sampleRate, int bitsPerSample)");
  return 0; // ERR
  // slave mode (not driving clock and frame select pin - input)
  //return begin(mode, 0, bitsPerSample, false);
}

int I2SClass::begin(int mode, long sampleRate, int bitsPerSample, bool driveClock)
{
  if(_initialized){
    end();
  }

  if (_state != I2S_STATE_IDLE && _state != I2S_STATE_DUPLEX) {
    log_e("I2S.begin: unexpected _state (%d)",_state);
    return 0; // ERR
  }

  // TODO implement left / right justified modes
  switch (mode) {
    case I2S_PHILIPS_MODE:
    case I2S_ADC_DAC:
      break;

    case I2S_RIGHT_JUSTIFIED_MODE: // normally this should work, but i don't how to set it up for ESP
    case I2S_LEFT_JUSTIFIED_MODE: // normally this should work, but i don't how to set it up for ESP
    default:
      // invalid mode
      log_e("ERROR I2SClass::begin() unknown mode");
      return 0; // ERR
  }

  _mode = mode;
  _sampleRate = sampleRate;
  _bitsPerSample = bitsPerSample;
  esp_i2s::i2s_mode_t i2s_mode = (esp_i2s::i2s_mode_t)(esp_i2s::I2S_MODE_RX | esp_i2s::I2S_MODE_TX);

  if(driveClock){
    i2s_mode = (esp_i2s::i2s_mode_t)(i2s_mode | esp_i2s::I2S_MODE_MASTER);
  }else{
    // TODO there will much more work with slave mode
    i2s_mode = (esp_i2s::i2s_mode_t)(i2s_mode | esp_i2s::I2S_MODE_SLAVE);
  }
  esp_i2s::i2s_pin_config_t pin_config;
  pin_config.bck_io_num = _sckPin;
  pin_config.ws_io_num = _fsPin;

  if(_mode == I2S_ADC_DAC){
    if(_bitsPerSample != 16){ // ADC/DAC can only work in 16-bit sample mode
      log_e("ERROR I2SClass::begin invalid bps for ADC/DAC");
      return 0; // ERR
    }
    i2s_mode = (esp_i2s::i2s_mode_t)(i2s_mode | esp_i2s::I2S_MODE_DAC_BUILT_IN | esp_i2s::I2S_MODE_ADC_BUILT_IN);
  }else{ // End of ADC/DAC mode; start of Normal mode
    if(_bitsPerSample != 16 && _bitsPerSample != 24 &&  _bitsPerSample != 32){
      if(_bitsPerSample == 8){
        log_e("ESP unfortunately does not support 8 bits per sample");
      }else{
        log_e("Invalid bits per sample for normal mode (requested %d)\nAllowed bps = 16 | 24 | 32", _bitsPerSample);
      }
      return 0; // ERR
    }
    if(_bitsPerSample == 24){
      log_w("Original Arduino library does not support 24 bits per sample - keep that in mind if you should switch back");
    }

    if (_state == I2S_STATE_DUPLEX){ // duplex
      pin_config = {
        .data_out_num = _outSdPin,
        .data_in_num = _inSdPin
      };
    }else{ // simplex
      pin_config = {
          .data_out_num = I2S_PIN_NO_CHANGE,
          .data_in_num = _sdPin
      };
    }

  } // Normal mode
  esp_i2s::i2s_config_t i2s_config = {
    .mode = i2s_mode,
    .sample_rate = _sampleRate,
    .bits_per_sample = (esp_i2s::i2s_bits_per_sample_t)_bitsPerSample,
    .channel_format = esp_i2s::I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = (esp_i2s::i2s_comm_format_t)(esp_i2s::I2S_COMM_FORMAT_STAND_I2S | esp_i2s::I2S_COMM_FORMAT_STAND_PCM_SHORT), // 0x01 | 0x04 // default
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
    .dma_buf_count = _I2S_DMA_BUFFER_COUNT,
    .dma_buf_len = _I2S_DMA_BUFFER_SIZE
  };

  _buffer_byte_size = _I2S_DMA_BUFFER_SIZE * (_bitsPerSample / 8) * _I2S_DMA_BUFFER_COUNT;
  _input_ring_buffer  = xRingbufferCreate(_buffer_byte_size, RINGBUF_TYPE_BYTEBUF);
  _output_ring_buffer = xRingbufferCreate(_buffer_byte_size, RINGBUF_TYPE_BYTEBUF);
  if(_input_ring_buffer == NULL || _output_ring_buffer == NULL){
    log_e("ERROR I2SClass::begin could not create one or both internal buffers. Requested size = %d\n", _buffer_byte_size);
    return 0; // ERR
  }

  if (ESP_OK != esp_i2s::i2s_driver_install((esp_i2s::i2s_port_t) _deviceIndex, &i2s_config, _I2S_EVENT_QUEUE_LENGTH, &_i2sEventQueue)){ // Install and start i2s driver
    log_e("ERROR could not install i2s driver");
    return 0; // ERR
  }

  if(_mode == I2S_ADC_DAC){
   esp_i2s::adc_unit_t adc_unit = (esp_i2s::adc_unit_t) 1;
    esp_i2s::adc1_channel_t adc_channel = (esp_i2s::adc1_channel_t) 6; //
    esp_i2s::i2s_set_dac_mode(esp_i2s::I2S_DAC_CHANNEL_BOTH_EN);
    esp_i2s::i2s_set_adc_mode(adc_unit, adc_channel);
    if(ESP_OK != esp_i2s::i2s_set_pin((esp_i2s::i2s_port_t) _deviceIndex, NULL)){
      log_e("i2s_set_pin failed");
      return 0; // ERR
    }

    esp_i2s::adc1_config_width(esp_i2s::ADC_WIDTH_BIT_12);
    esp_i2s::adc1_config_channel_atten(adc_channel, esp_i2s::ADC_ATTEN_DB_11);
    esp_i2s::i2s_adc_enable((esp_i2s::i2s_port_t) _deviceIndex);
  }else{ // End of ADC/DAC mode; start of Normal mode
    if (ESP_OK != esp_i2s::i2s_set_pin((esp_i2s::i2s_port_t) _deviceIndex, &pin_config)) {
      log_e("i2s_set_pin failed");
      end();
      return 0; // ERR
    }
  }

  if(!createCallbackTask()){
    return 0; // ERR
  }
  _initialized = true;

  return 1; // OK
}

int I2SClass::_applyPinSetting(){
  if(_initialized){
    esp_i2s::i2s_pin_config_t pin_config = {
      .bck_io_num = _sckPin,
      .ws_io_num = _fsPin,
      .data_out_num = _outSdPin,
      .data_in_num = _inSdPin
    };
    if(ESP_OK != esp_i2s::i2s_set_pin((esp_i2s::i2s_port_t) _deviceIndex, &pin_config)){
      log_e("i2s_set_pin failed");
      return 0; // ERR
    }
  }
  return 1; // OK
}

void I2SClass::_setSckPin(int sckPin){
  if(sckPin >= 0){
    _sckPin = sckPin;
  }else{
    _sckPin = PIN_I2S_SCK;
  }
}

int I2SClass::setSckPin(int sckPin){
  _setSckPin(sckPin);
  return _applyPinSetting();
}

void I2SClass::_setFsPin(int fsPin){
  if(fsPin >= 0){
    _fsPin = fsPin;
  }else{
    _fsPin = PIN_I2S_FS;
  }
}

int I2SClass::setFsPin(int fsPin){
  _setFsPin(fsPin);
  return _applyPinSetting();
}

void I2SClass::_setDataInPin(int inSdPin){
  if(inSdPin >= 0){
    _inSdPin = inSdPin;
  }else{
    _inSdPin = PIN_I2S_SD;
  }
}

int I2SClass::setDataInPin(int inSdPin){
  _setDataInPin(inSdPin);
  pinMode(_inSdPin, INPUT_PULLDOWN);
  // TODO if there is any default pinMode - set it to old input
  return _applyPinSetting();
}

void I2SClass::_setDataOutPin(int outSdPin){
  if(outSdPin >= 0){
    _outSdPin = outSdPin;
  }else{
    _outSdPin = PIN_I2S_SD_OUT;
  }
}

int I2SClass::setDataOutPin(int outSdPin){
  _setDataOutPin(outSdPin);
  return _applyPinSetting();
}


int I2SClass::setAllPins(){
  return setAllPins(PIN_I2S_SCK, PIN_I2S_FS, PIN_I2S_SD, PIN_I2S_SD_OUT);
}

int I2SClass::setAllPins(int sckPin, int fsPin, int inSdPin, int outSdPin){
  setSckPin(sckPin);
  setFsPin(fsPin);
  setDataInPin(inSdPin);
  setDataOutPin(outSdPin);

  return _applyPinSetting();
}

int I2SClass::setStateDuplex(){
  if(_inSdPin < 0 || _outSdPin < 0){
    log_e("I2S cannot set Duplex-one or both pins not set\n input pin = %d\toutput pin = %d", _inSdPin, _outSdPin);
    return 0; // ERR
  }
  _state = I2S_STATE_DUPLEX;
  return 1;
}

int I2SClass::getSckPin(){
  return _sckPin;
}

int I2SClass::getFsPin(){
  return _fsPin;
}

int I2SClass::getDataPin(){
  return _sdPin;
}

int I2SClass::getDataInPin(){
  return _inSdPin;
}

int I2SClass::getDataOutPin(){
  return _outSdPin;
}

void I2SClass::end()
{
  if(xTaskGetCurrentTaskHandle() != _callbackTaskHandle){
    destroyCallbackTask();

    if(_initialized){
      if(_mode == I2S_ADC_DAC){
        esp_i2s::i2s_adc_disable((esp_i2s::i2s_port_t) _deviceIndex);
      }
      esp_i2s::i2s_driver_uninstall((esp_i2s::i2s_port_t) _deviceIndex);
      _initialized = false;
      if(_state != I2S_STATE_DUPLEX){
        _state = I2S_STATE_IDLE;
      }
    }
    _onTransmit = NULL;
    _onReceive  = NULL;
    vRingbufferDelete(_input_ring_buffer);
    vRingbufferDelete(_output_ring_buffer);
  }else{
    log_w("WARNING: ending I2SClass from callback task not permitted, but attempted!");
  }
}

// Bytes available to read
int I2SClass::available()
{
  return _buffer_byte_size - (int)xRingbufferGetCurFreeSize(_input_ring_buffer);
}

union i2s_sample_t {
  uint8_t b8;
  int16_t b16;
  int32_t b32;
};

int I2SClass::read()
{
  i2s_sample_t sample;
  sample.b32 = 0;
  read(&sample, _bitsPerSample / 8);

  if (_bitsPerSample == 32) {
    return sample.b32;
  } else if (_bitsPerSample == 16) {
    return sample.b16;
  } else if (_bitsPerSample == 8) {
    return sample.b8;
  } else {
    return 0;
  }
  return 0;
}

int I2SClass::read(void* buffer, size_t size){
  if (_state != I2S_STATE_RECEIVER && _state != I2S_STATE_DUPLEX) {
    if(!enableReceiver()){
      return 0; // There was an error switching to receiver
    }
  }

  size_t item_size = 0;
  void *tmp_buffer;
  tmp_buffer = xRingbufferReceiveUpTo(_input_ring_buffer, &item_size, pdMS_TO_TICKS(1000), size);
  if(tmp_buffer != NULL){
    memcpy(buffer, tmp_buffer, item_size);
    if(_mode == I2S_ADC_DAC){
      for(size_t i = 0; i < item_size / 2; ++i){
        ((uint16_t*)buffer)[i] = ((uint16_t*)buffer)[i] & 0x0FFF;
      }
    } // ADC/DAC mode
    vRingbufferReturnItem(_input_ring_buffer, tmp_buffer);
    return item_size;
  }else{
    return 0;
  }
}

/*
size_t I2SClass::write(int sample)
{
  return write((int32_t)sample);
}
*/

size_t I2SClass::write(uint8_t data)
{
  return write((int32_t)data);
}

size_t I2SClass::write(int32_t sample)
{
  return write(&sample, _bitsPerSample/8);
}

size_t I2SClass::write(const uint8_t *buffer, size_t size)
{
  return write((const void*)buffer, size);
}

size_t I2SClass::write(const void *buffer, size_t size)
{
  if (_state != I2S_STATE_TRANSMITTER && _state != I2S_STATE_DUPLEX) {
    if(!enableTransmitter()){
      return 0; // There was an error switching to transmitter
    }
  }
  if(pdTRUE == xRingbufferSend(_output_ring_buffer, buffer, size, 10)){
    return size;
  }else{
    return 0;
  }
}

int I2SClass::peek()
{
  // TODO
  // peek() is not implemented for ESP yet
  return 0;
}

void I2SClass::flush()
{
  const size_t single_dma_buf = _I2S_DMA_BUFFER_SIZE*(_bitsPerSample/8);
  size_t item_size = 0;
  size_t bytes_written;
  void *item = NULL;

  item = xRingbufferReceiveUpTo(_output_ring_buffer, &item_size, pdMS_TO_TICKS(1000), single_dma_buf);
  if (item != NULL){
    esp_i2s::i2s_write((esp_i2s::i2s_port_t) _deviceIndex, item, item_size, &bytes_written, 0);
    if(item_size != bytes_written){
    }
    vRingbufferReturnItem(_output_ring_buffer, item);
  }
}

// Bytes available to write
int I2SClass::availableForWrite()
{
  return (int)xRingbufferGetCurFreeSize(_output_ring_buffer);
}

void I2SClass::onTransmit(void(*function)(void))
{
  _onTransmit = function;
}

void I2SClass::onReceive(void(*function)(void))
{
  _onReceive = function;
}

void I2SClass::setBufferSize(int bufferSize)
{
  // does nothing in ESP
}

int I2SClass::enableTransmitter()
{
  if (_state != I2S_STATE_TRANSMITTER && _state != I2S_STATE_DUPLEX){
    esp_i2s::i2s_pin_config_t pin_config = {
      .bck_io_num = _sckPin,
      .ws_io_num = _fsPin,
      .data_out_num = _outSdPin != -1 ? _outSdPin : _sdPin,
      .data_in_num = -1 // esp_i2s::I2S_PIN_NO_CHANGE,
    };
    if(ESP_OK != esp_i2s::i2s_set_pin((esp_i2s::i2s_port_t) _deviceIndex, &pin_config)){
      _state = I2S_STATE_IDLE;
      return 0; // ERR
    }
    _state = I2S_STATE_TRANSMITTER;
  }
  return 1; // Ok
}

int I2SClass::enableReceiver()
{
  if (_state != I2S_STATE_RECEIVER && _state != I2S_STATE_DUPLEX){
    esp_i2s::i2s_pin_config_t pin_config = {
        .bck_io_num = _sckPin,
        .ws_io_num = _fsPin,
        .data_out_num = -1, // esp_i2s::I2S_PIN_NO_CHANGE,
        .data_in_num = _inSdPin != -1 ? _inSdPin : _sdPin
    };
    if(ESP_OK != esp_i2s::i2s_set_pin((esp_i2s::i2s_port_t) _deviceIndex, &pin_config)){
      _state = I2S_STATE_IDLE;
      log_e("i2s_set_pin failed");
      return 0; // ERR
    }
    _state = I2S_STATE_RECEIVER;
  }
  return 1; // Ok
}

void I2SClass::onTransferComplete()
{
  static QueueSetHandle_t xQueueSet;
  const size_t single_dma_buf = _I2S_DMA_BUFFER_SIZE*(_bitsPerSample/8);
  QueueSetMemberHandle_t xActivatedMember;
  esp_i2s::i2s_event_type_t i2s_event;
  size_t item_size = 0;
  size_t prev_item_size = 0;
  void *item = NULL;
  bool prev_item_valid = false;
  size_t bytes_written, bytes_read;
  int prev_item_offset = 0;
  uint8_t prev_item[_I2S_DMA_BUFFER_SIZE*4];
  uint8_t _inputBuffer[_I2S_DMA_BUFFER_SIZE*4];

  xQueueSet = xQueueCreateSet(sizeof(i2s_event)*_I2S_EVENT_QUEUE_LENGTH + 1);
  configASSERT(xQueueSet);
  xQueueAddToSet(_i2sEventQueue, xQueueSet);
  xQueueAddToSet(_task_kill_cmd_semaphore_handle, xQueueSet);

  while(true){
    xActivatedMember = xQueueSelectFromSet(xQueueSet, portMAX_DELAY);
    if(xActivatedMember == _task_kill_cmd_semaphore_handle){
      xSemaphoreTake(_task_kill_cmd_semaphore_handle, 0);
      break; // from the infinite loop
    }else if(xActivatedMember == _i2sEventQueue){
      xQueueReceive(_i2sEventQueue, &i2s_event, 0);
      if(i2s_event == esp_i2s::I2S_EVENT_TX_DONE){
        if(prev_item_valid){ // use item from previous round
          esp_i2s::i2s_write((esp_i2s::i2s_port_t) _deviceIndex, prev_item+prev_item_offset, prev_item_size, &bytes_written, 0);
          if(prev_item_size == bytes_written){
            prev_item_valid = false;
          } // write size check
          prev_item_offset = bytes_written;
          prev_item_size -= bytes_written;
        } // prev_item_valid

        if(_buffer_byte_size - xRingbufferGetCurFreeSize(_output_ring_buffer) >= single_dma_buf){ // fill up the I2S DMA buffer
          do{ // "send to esp i2s driver" loop
            bytes_written = 0;
            item_size = 0;
            item = xRingbufferReceiveUpTo(_output_ring_buffer, &item_size, pdMS_TO_TICKS(1000), single_dma_buf);
            if (item != NULL){
              esp_i2s::i2s_write((esp_i2s::i2s_port_t) _deviceIndex, item, item_size, &bytes_written, 0);
              if(item_size != bytes_written){ // save item that was not written correctly for later
                memcpy(prev_item, (void*)&((uint8_t*)item)[bytes_written], item_size-bytes_written);
                prev_item_size = item_size - bytes_written;
                prev_item_offset = 0;
                prev_item_valid = true;
              } // save item that was not written correctly for later
              vRingbufferReturnItem(_output_ring_buffer, item);
            } // Check received item
          }while(item_size == bytes_written && item_size == single_dma_buf);
        } // don't read from almost empty buffer

        if(_onTransmit){
          _onTransmit();
        } // user callback

      }else if(i2s_event == esp_i2s::I2S_EVENT_RX_DONE){
        size_t avail = xRingbufferGetCurFreeSize(_input_ring_buffer);
        esp_i2s::i2s_read((esp_i2s::i2s_port_t) _deviceIndex, _inputBuffer, avail <= single_dma_buf ? avail : single_dma_buf, (size_t*) &bytes_read, 0);
        if(pdTRUE != xRingbufferSend(_input_ring_buffer, _inputBuffer, bytes_read, 0)){
          log_w("I2S failed to send item from DMA to internal buffer\n");
        }else{
          if (_onReceive) {
            _onReceive();
          } // user callback
        } // xRingbufferSendComplete
      } // RX Done
    } // Queue set (I2S event or kill command)
  } // infinite loop
  _callbackTaskHandle = NULL; // prevent secondary termination to non-existing task
}

void I2SClass::onDmaTransferComplete(void*)
{

  I2S.onTransferComplete();
  vTaskDelete(NULL);
}

#if I2S_INTERFACES_COUNT > 0
  I2SClass I2S(I2S_DEVICE, I2S_CLOCK_GENERATOR, PIN_I2S_SD, PIN_I2S_SCK, PIN_I2S_FS); // default - half duplex
  //I2SClass I2S(I2S_DEVICE, I2S_CLOCK_GENERATOR, PIN_I2S_SD, PIN_I2S_SD_OUT, PIN_I2S_SCK, PIN_I2S_FS); // full duplex
#endif

#if I2S_INTERFACES_COUNT > 1
  // TODO set default pins for second module
  //I2SClass I2S1(I2S_DEVICE+1, I2S_CLOCK_GENERATOR, PIN_I2S_SD, PIN_I2S_SCK, PIN_I2S_FS); // default - half duplex
#endif
