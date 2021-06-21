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

int I2SClass::_beginCount = 0;

I2SClass::I2SClass(uint8_t deviceIndex, uint8_t clockGenerator, uint8_t sdPin, uint8_t sckPin, uint8_t fsPin) :
  _deviceIndex(0),
  _clockGenerator(clockGenerator),
  _sdPin(sdPin),    // shared data pin
  _inSdPin(-1),  // input data pin
  _outSdPin(-1), // output data pin
  _sckPin(sckPin),  // clock pin
  _fsPin(fsPin),    // frame (word) select pin

  _state(I2S_STATE_IDLE),
  _dmaChannel(-1),
  _bitsPerSample(0),
  _sampleRate(0),
  _mode(I2S_PHILIPS_MODE),

  _initialized(false),
  _callbackTaskHandle(NULL),
  _i2sEventQueue(NULL),
  _task_kill_cmd_semaphore_handle(NULL),

  _onTransmit(NULL),
  _onReceive(NULL)
{
}

I2SClass::I2SClass(uint8_t deviceIndex, uint8_t clockGenerator, uint8_t inSdPin, uint8_t outSdPin, uint8_t sckPin, uint8_t fsPin) : // set duplex
  _deviceIndex(0),
  _clockGenerator(clockGenerator),
  _sdPin(inSdPin),    // shared data pin
  _inSdPin(inSdPin),  // input data pin
  _outSdPin(outSdPin), // output data pin
  _sckPin(sckPin), // clock pin
  _fsPin(fsPin),   // frame (word) select pin

  _state(I2S_STATE_DUPLEX),
  _dmaChannel(-1),
  _bitsPerSample(0),
  _sampleRate(0),
  _mode(I2S_PHILIPS_MODE),

  _initialized(false),
  _callbackTaskHandle(NULL),
  _i2sEventQueue(NULL),
  _task_kill_cmd_semaphore_handle(NULL),

  _onTransmit(NULL),
  _onReceive(NULL)
{
}

void I2SClass::createCallbackTask()
{
  int stack_size = 3000;
  if(_callbackTaskHandle == NULL){
    if(_task_kill_cmd_semaphore_handle == NULL){
      _task_kill_cmd_semaphore_handle = xSemaphoreCreateBinary();
    }
    xTaskCreate(
      onDmaTransferComplete, // Function to implement the task
      "onDmaTransferComplete", // Name of the task
      stack_size,  // Stack size in words
      NULL,  // Task input parameter
      1,  // Priority of the task
      &_callbackTaskHandle  // Task handle.
      );
  }
}

void I2SClass::destroyCallbackTask()
{
  if(_callbackTaskHandle != NULL && xTaskGetCurrentTaskHandle() != _callbackTaskHandle){
    xSemaphoreGive(_task_kill_cmd_semaphore_handle);
  } // callback handle check
}

int I2SClass::begin(int mode, long sampleRate, int bitsPerSample)
{
  // master mode (driving clock and frame select pins - output)
  return begin(mode, sampleRate, bitsPerSample, true);
}

int I2SClass::begin(int mode, int bitsPerSample)
{
  Serial.println("ERROR I2SClass::begin Audio in Slave mode is not implemented for ESP");
  Serial.println("Note: If it is NOT your intention to initialize in slave mode, you are probably missing <sampleRate> parameter - see the declaration below");
  Serial.println("\tint I2SClass::begin(int mode, long sampleRate, int bitsPerSample)");
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
    return 0; // ERR
  }

  // TODO implement left / right justified modes
  switch (mode) {
    case I2S_PHILIPS_MODE:
      //case I2S_RIGHT_JUSTIFIED_MODE: // normally this should work, but i don't how to set it up for ESP
      //case I2S_LEFT_JUSTIFIED_MODE: // normally this should work, but i don't how to set it up for ESP
    case I2S_ADC_DAC:
      break;

    case I2S_RIGHT_JUSTIFIED_MODE: // normally this should work, but i don't how to set it up for ESP
    case I2S_LEFT_JUSTIFIED_MODE: // normally this should work, but i don't how to set it up for ESP
    default:
      // invalid mode
      return 0; // ERR
  }

  _mode = mode;
  _sampleRate = sampleRate;
  _bitsPerSample = bitsPerSample;

  esp_i2s::i2s_mode_t i2s_mode;
  if(driveClock){
    i2s_mode = esp_i2s::I2S_MODE_MASTER;
  }else{
    // TODO there will much more work with slave mode
    i2s_mode = esp_i2s::I2S_MODE_SLAVE;
  }

  if(_mode == I2S_ADC_DAC){
    if(bitsPerSample != 16){ // ADC/DAC can only work in 16-bit sample mode
      return 0; // ERR
    }

    i2s_mode = (esp_i2s::i2s_mode_t)(i2s_mode | esp_i2s::I2S_MODE_TX | esp_i2s::I2S_MODE_RX | esp_i2s::I2S_MODE_DAC_BUILT_IN | esp_i2s::I2S_MODE_ADC_BUILT_IN);

    esp_i2s::i2s_config_t i2s_config = {
      .mode = i2s_mode,
      .sample_rate = _sampleRate,
      .bits_per_sample = esp_i2s::I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = esp_i2s::I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = (esp_i2s::i2s_comm_format_t)(esp_i2s::I2S_COMM_FORMAT_STAND_I2S | esp_i2s::I2S_COMM_FORMAT_STAND_PCM_SHORT), // 0x01 | 0x04
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 512 // buffer length in Bytes
    };

    if (ESP_OK != esp_i2s::i2s_driver_install((esp_i2s::i2s_port_t) _deviceIndex, &i2s_config, _I2S_EVENT_QUEUE_LENGTH, &_i2sEventQueue)){ // Install and start i2s driver
      return 0; // ERR
    }

    esp_i2s::adc_unit_t adc_unit = (esp_i2s::adc_unit_t) 1;
    esp_i2s::adc1_channel_t adc_channel = (esp_i2s::adc1_channel_t) 6; //
    esp_i2s::i2s_set_dac_mode(esp_i2s::I2S_DAC_CHANNEL_BOTH_EN);
    esp_i2s::i2s_set_adc_mode(adc_unit, adc_channel);
    if(ESP_OK != esp_i2s::i2s_set_pin((esp_i2s::i2s_port_t) _deviceIndex, NULL)){
      Serial.println("i2s_set_pin err");
      return 0; // ERR
    }

    esp_i2s::adc1_config_width(esp_i2s::ADC_WIDTH_BIT_12);
    esp_i2s::adc1_config_channel_atten(adc_channel, esp_i2s::ADC_ATTEN_DB_11);
    esp_i2s::i2s_adc_enable((esp_i2s::i2s_port_t) _deviceIndex);

  }else{ // normal I2S mode without ADC/DAC
    if(_bitsPerSample != 16 && /*_bitsPerSample != 24 && */ _bitsPerSample != 32){
      Serial.println("I2S.begin(): invalid bits per second");
      // ESP does support 24 bps, however for the compatibility
      // with original Arduino implementation it is not allowed
      return 0; // ERR
    }

    i2s_mode = (esp_i2s::i2s_mode_t)(i2s_mode | esp_i2s::I2S_MODE_RX | esp_i2s::I2S_MODE_TX);
    esp_i2s::i2s_channel_fmt_t i2s_channel_format = esp_i2s::I2S_CHANNEL_FMT_RIGHT_LEFT;
    esp_i2s::i2s_comm_format_t i2s_comm_format    = (esp_i2s::i2s_comm_format_t)(esp_i2s::I2S_COMM_FORMAT_STAND_I2S | esp_i2s::I2S_COMM_FORMAT_STAND_PCM_SHORT); // 0x01 | 0x04
    esp_i2s::i2s_config_t i2s_config = {
      .mode = i2s_mode,
      .sample_rate = _sampleRate,
      .bits_per_sample = (esp_i2s::i2s_bits_per_sample_t) _bitsPerSample,
      .channel_format = i2s_channel_format,
      .communication_format = i2s_comm_format,
      .intr_alloc_flags = 1,
      .dma_buf_count = 8,
      .dma_buf_len = 512 // buffer length in Bytes
    };

    esp_i2s::i2s_pin_config_t pin_config;
    if (_state == I2S_STATE_DUPLEX){ // duplex
      pin_config = {
        .bck_io_num = _sckPin,
        .ws_io_num = _fsPin,
        .data_out_num = _outSdPin,
        .data_in_num = _inSdPin
      };
    }else{ // simplex
      pin_config = {
          .bck_io_num = _sckPin,
          .ws_io_num = _fsPin,
          .data_out_num = -1, // esp_i2s::I2S_PIN_NO_CHANGE,
          .data_in_num = _sdPin
      };
    }

    if (ESP_OK != esp_i2s::i2s_driver_install((esp_i2s::i2s_port_t) _deviceIndex, &i2s_config, 10, &_i2sEventQueue)) {
      Serial.println("i2s_driver_install err");
      return 0; // ERR
    }

    if (ESP_OK != esp_i2s::i2s_set_pin((esp_i2s::i2s_port_t) _deviceIndex, &pin_config)) {
      Serial.println("i2s_set_pin err");
      end();
      return 0; // ERR
    }
  } // ADC/DAC or normal I2S mode
  createCallbackTask();
  _initialized = true;
  return 1; // OK
}

int I2SClass::setHalfDuplex(uint8_t inSdPin, uint8_t outSdPin){
  _inSdPin = inSdPin;
  _outSdPin = outSdPin;
  if(_initialized){
    esp_i2s::i2s_pin_config_t pin_config = {
      .bck_io_num = _sckPin,
      .ws_io_num = _fsPin,
      .data_out_num = _outSdPin,
      .data_in_num = _inSdPin
    };
    if(ESP_OK != esp_i2s::i2s_set_pin((esp_i2s::i2s_port_t) _deviceIndex, &pin_config)){
      return 0; // ERR
    }
  }
  return 1; // OK
}

int I2SClass::setSimplex(uint8_t sdPin){
  _sdPin = sdPin;
  if(_initialized){
    esp_i2s::i2s_pin_config_t pin_config = {
      .bck_io_num = _sckPin,
      .ws_io_num = _fsPin,
      .data_out_num = -1, // esp_i2s::I2S_PIN_NO_CHANGE,
      .data_in_num = _sdPin
    };
    if(ESP_OK != esp_i2s::i2s_set_pin((esp_i2s::i2s_port_t) _deviceIndex, &pin_config)){
      return 0; // ERR
    }
  }
  return 1; // OK
}

void I2SClass::end()
{
  if(xTaskGetCurrentTaskHandle() != _callbackTaskHandle){
    destroyCallbackTask();
  }

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
}

// available to read
int I2SClass::available()
{
  // There is no actual way to tell in ESP
  return 8;
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
  int bytes_read = read(&sample, _bitsPerSample / 8);

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

int I2SClass::read(void* buffer, size_t size)
{
  static long debug_timer_prev = 0;
  if (_state != I2S_STATE_RECEIVER && _state != I2S_STATE_DUPLEX) {
    if(!enableReceiver()){
      return 0; // There was an error switching to receiver
    }
  }
  int read;
  debug_timer_prev = millis();
  esp_i2s::i2s_read((esp_i2s::i2s_port_t) _deviceIndex, buffer, size, (size_t*) &read, 10);

    if(_mode == I2S_ADC_DAC){
    for(int i = 0; i < read / 2; ++i){
      ((uint16_t*)buffer)[i] = ((uint16_t*)buffer)[i] & 0x0FFF;
    }
  }
  return read;
}

/*
size_t I2SClass::write(int sample)
{
  return write((int32_t)sample);
}
*/

size_t I2SClass::write(int32_t sample)
{
  return write(&sample, 1);
}

size_t I2SClass::write(const void *buffer, size_t size)
{
  if (_state != I2S_STATE_TRANSMITTER && _state != I2S_STATE_DUPLEX) {
    if(!enableTransmitter()){
      return 0; // There was an error switching to transmitter
    }
  }
  size_t bytes_written;
  esp_i2s::i2s_write((esp_i2s::i2s_port_t) _deviceIndex, buffer, size, &bytes_written, 10);
  return bytes_written;
}

int I2SClass::peek()
{
  // TODO
  Serial.println("I2SClass: peek() is not implemented for ESP");
  return 0;
}

void I2SClass::flush()
{
  // do nothing, writes are DMA triggered in ESP
}

size_t I2SClass::write(uint8_t data)
{
  return write((int32_t)data);
}

size_t I2SClass::write(const uint8_t *buffer, size_t size)
{
  return write((const void*)buffer, size);
}

int I2SClass::availableForWrite()
{
  // There is no actual way to tell in ESP
  return 512;
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
  Serial.println("I2SClass::setBufferSize() does nothing for ESP");
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
      return 0; // ERR
    }
    _state = I2S_STATE_RECEIVER;
  }
  return 1; // Ok
}

void I2SClass::onTransferComplete()
{
  static QueueSetHandle_t xQueueSet;
  QueueSetMemberHandle_t xActivatedMember;
  esp_i2s::i2s_event_type_t i2s_event;
  EventBits_t uxReturn;

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
      if((i2s_event == esp_i2s::I2S_EVENT_TX_DONE) && (_state == I2S_STATE_DUPLEX || _state == I2S_STATE_TRANSMITTER)){
        if(_onTransmit){
          _onTransmit();
        }
      }else if(i2s_event == esp_i2s::I2S_EVENT_RX_DONE && (_state == I2S_STATE_RECEIVER || _state == I2S_STATE_DUPLEX)){
        if (_onReceive) {
          _onReceive();
        }
      } // if event TX or RX
    }
  _callbackTaskHandle = NULL; // prevent secondary termination to non-existing task
  }
  vTaskDelete(NULL);
}

void I2SClass::onDmaTransferComplete(void*)
{
    I2S.onTransferComplete();
}

#if I2S_INTERFACES_COUNT > 0
  #ifdef ESP_PLATFORM
    #define I2S_DEVICE 0
    #define I2S_CLOCK_GENERATOR 0 // does nothing for ESP
    #define PIN_I2S_SCK 5
    #define PIN_I2S_FS 25
    #define PIN_I2S_SD 35 // ESP data in / codec data out (microphone)
    #define PIN_I2S_SD_OUT 26 // ESP data out / codec data in (speakers)
  #endif

I2SClass I2S(I2S_DEVICE, I2S_CLOCK_GENERATOR, PIN_I2S_SD, PIN_I2S_SCK, PIN_I2S_FS); // default - half duplex
//I2SClass I2S(I2S_DEVICE, I2S_CLOCK_GENERATOR, PIN_I2S_SD, PIN_I2S_SD_OUT, PIN_I2S_SCK, PIN_I2S_FS); // full duplex
#endif
