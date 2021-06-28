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

//#define _USE_SYS_VIEW 1
#ifdef _USE_SYS_VIEW
  #include "esp_sysview_trace.h" // debug
  //#include "esp_heap_trace.h" // debug
  #include "esp_log.h" // debug
#endif // _USE_SYS_VIEW

#define _I2S_EVENT_QUEUE_LENGTH 10
#define _I2S_DMA_BUFFER_SIZE 512
#define _I2S_DMA_BUFFER_COUNT 8
#define I2S_INTERFACES_COUNT SOC_I2S_NUM

#ifndef I2S_DEVICE
  #define I2S_DEVICE 0
#endif

#ifndef I2S_CLOCK_GENERATOR
  #define I2S_CLOCK_GENERATOR 0 // does nothing for ESP
#endif

#ifndef PIN_I2S_SCK
  #define PIN_I2S_SCK 5
#endif

#ifndef PIN_I2S_FS
  #define PIN_I2S_FS 25
#endif

#ifndef PIN_I2S_SD
  #define PIN_I2S_SD 26
#endif

#ifndef PIN_I2S_SD_IN
  #define PIN_I2S_SD_IN 35 // 35 can be only input pin
#endif


#ifndef PIN_I2S_SD_OUT
  #define PIN_I2S_SD_OUT 26
#endif

I2SClass::I2SClass(uint8_t deviceIndex, uint8_t clockGenerator, uint8_t sdPin, uint8_t sckPin, uint8_t fsPin) :
  _deviceIndex(deviceIndex),
  _sdPin(sdPin),    // shared data pin
  _inSdPin(-1),  // input data pin
  _outSdPin(-1), // output data pin
  _sckPin(sckPin),  // clock pin
  _fsPin(fsPin),    // frame (word) select pin

  _state(I2S_STATE_IDLE),
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
  _deviceIndex(deviceIndex),
  _sdPin(inSdPin),    // shared data pin
  _inSdPin(inSdPin),  // input data pin
  _outSdPin(outSdPin), // output data pin
  _sckPin(sckPin), // clock pin
  _fsPin(fsPin),   // frame (word) select pin

  _state(I2S_STATE_DUPLEX),
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
      2,  // Priority of the task
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
    Serial.println("ERROR I2SClass::begin invalid state"); // debug
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
      Serial.println("ERROR I2SClass::begin invalid mode"); // debug
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

  Serial.println("I2SClass::begin foo"); // debug
  if(_mode == I2S_ADC_DAC){
    Serial.println("I2SClass::begin foo adc"); // debug
    if(_bitsPerSample != 16){ // ADC/DAC can only work in 16-bit sample mode
      Serial.println("ERROR I2SClass::begin invalid bps for ADC/DAC"); // debug
      return 0; // ERR
    }
    i2s_mode = (esp_i2s::i2s_mode_t)(i2s_mode | esp_i2s::I2S_MODE_DAC_BUILT_IN | esp_i2s::I2S_MODE_ADC_BUILT_IN);
  }else{ // End of ADC/DAC mode; start of Normal mode
    Serial.println("I2SClass::begin foo normal"); // debug
    if(_bitsPerSample != 16 && /*_bitsPerSample != 24 && */ _bitsPerSample != 32){
      Serial.println("I2S.begin(): invalid bits per second for normal mode");
      // ESP does support 24 bps, however for the compatibility
      // with original Arduino implementation it is not allowed
      return 0; // ERR
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
  Serial.println("I2SClass::begin bar"); // debug
  esp_i2s::i2s_config_t i2s_config = {
    .mode = i2s_mode,
    .sample_rate = _sampleRate,
    .bits_per_sample = (esp_i2s::i2s_bits_per_sample_t)_bitsPerSample,
    .channel_format = esp_i2s::I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = (esp_i2s::i2s_comm_format_t)(esp_i2s::I2S_COMM_FORMAT_STAND_I2S | esp_i2s::I2S_COMM_FORMAT_STAND_PCM_SHORT), // 0x01 | 0x04
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
    .dma_buf_count = _I2S_DMA_BUFFER_COUNT,
    .dma_buf_len = _I2S_DMA_BUFFER_SIZE
  };

  if (ESP_OK != esp_i2s::i2s_driver_install((esp_i2s::i2s_port_t) _deviceIndex, &i2s_config, _I2S_EVENT_QUEUE_LENGTH, &_i2sEventQueue)){ // Install and start i2s driver
    Serial.println("ERROR I2SClass::begin error installing i2s driver"); // debug
    Serial.flush();
    delay(100);
    return 0; // ERR
  }

  if(_mode == I2S_ADC_DAC){
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
  }else{ // End of ADC/DAC mode; start of Normal mode
    Serial.println("I2SClass::begin calling set"); // debug
    if (ESP_OK != esp_i2s::i2s_set_pin((esp_i2s::i2s_port_t) _deviceIndex, &pin_config)) {
      Serial.println("i2s_set_pin err");
      end();
      return 0; // ERR
    }
  }
  createCallbackTask();
  _initialized = true;
  return 1; // OK
}

int I2SClass::setAllPins(){
  return setAllPins(PIN_I2S_SCK, PIN_I2S_FS, PIN_I2S_SD, PIN_I2S_SD_OUT);
}

int I2SClass::setAllPins(int sckPin, int fsPin, int inSdPin, int outSdPin){
  if(sckPin >= 0){
    _sckPin = sckPin;
  }else{
    _sckPin = PIN_I2S_SCK;
  }

  if(fsPin >= 0){
    _fsPin = fsPin;
  }else{
    _fsPin = PIN_I2S_FS;
  }

  if(inSdPin >= 0){
    _inSdPin = inSdPin;
  }else{
    _inSdPin = PIN_I2S_SD;
  }

  if(outSdPin >= 0){
    _outSdPin = outSdPin;
  }else{
    _outSdPin = PIN_I2S_SD_OUT;
  }

  if(_initialized){
    esp_i2s::i2s_pin_config_t pin_config = {
      .bck_io_num = _sckPin,
      .ws_io_num = _fsPin,
      .data_out_num = _outSdPin,
      .data_in_num = _inSdPin
    };
    if(ESP_OK != esp_i2s::i2s_set_pin((esp_i2s::i2s_port_t) _deviceIndex, &pin_config)){
      Serial.printf("setFullDuplex() set pins error\n"); // debug
      return 0; // ERR
    }
  }
  return 1; // OK
}

int I2SClass::setStateDuplex(){
  if(_inSdPin < 0 || _outSdPin < 0){
    return 0; // ERR
  }
  _state = I2S_STATE_DUPLEX;
  return 1;
}

//int I2SClass::setHalfDuplex(uint8_t inSdPin=PIN_I2S_SD, uint8_t outSdPin=PIN_I2S_SD_OUT){
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
      Serial.printf("setFullDuplex() set pins error\n"); // debug
      return 0; // ERR
    }
    Serial.printf("setFullDuplex(inSdPin=%d, outSdPin=%d) OK\n", inSdPin, outSdPin); // debug
  }
  return 1; // OK
}

//int I2SClass::setSimplex(uint8_t sdPin=PIN_I2S_SD){
int I2SClass::setSimplex(uint8_t sdPin){
  _sdPin = sdPin;
  if(_initialized){
    esp_i2s::i2s_pin_config_t pin_config = {
      .bck_io_num = _sckPin,
      .ws_io_num = _fsPin,
      //.data_out_num = -1, // esp_i2s::I2S_PIN_NO_CHANGE,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = _sdPin
    };
    if(ESP_OK != esp_i2s::i2s_set_pin((esp_i2s::i2s_port_t) _deviceIndex, &pin_config)){
      Serial.printf("setHalfDuplex: err setting pin %d\n", sdPin); // debug
      return 0; // ERR
    }
    Serial.printf("setHalfDuplex(sdPin=%d) OK\n", sdPin); // debug
  }
  return 1; // OK
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
  }else{
    // TODO log_e with error - destroy task from inside not permitted
  }
}

// available to read
int I2SClass::available()
{
  // There is no actual way to tell in ESP
  return _I2S_DMA_BUFFER_SIZE;
  //uxQueueSpacesAvailable();
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
  //int bytes_read = read(&sample, _bitsPerSample / 8);
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

int I2SClass::read(void* buffer, size_t size)
{
  if (_state != I2S_STATE_RECEIVER && _state != I2S_STATE_DUPLEX) {
    if(!enableReceiver()){
      return 0; // There was an error switching to receiver
    }
  }
  int read;
#ifdef _USE_SYS_VIEW
  SEGGER_SYSVIEW_Print("i2s_read start");
#endif // _USE_SYS_VIEW
  esp_i2s::i2s_read((esp_i2s::i2s_port_t) _deviceIndex, buffer, size, (size_t*) &read, 0);
#ifdef _USE_SYS_VIEW
  SEGGER_SYSVIEW_Print("i2s_read stop");
#endif // _USE_SYS_VIEW

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
  return write(&sample, _bitsPerSample/8);
}

size_t I2SClass::write(const void *buffer, size_t size)
{
  //Serial.printf("I2SClass::write(): _state = %d\n",_state);
  if (_state != I2S_STATE_TRANSMITTER && _state != I2S_STATE_DUPLEX) {
    if(!enableTransmitter()){
      return 0; // There was an error switching to transmitter
    }
  }
  size_t bytes_written;
#ifdef _USE_SYS_VIEW
  SEGGER_SYSVIEW_Print("i2s_write start");
#endif // _USE_SYS_VIEW
  esp_i2s::i2s_write((esp_i2s::i2s_port_t) _deviceIndex, buffer, size, &bytes_written, 0);
#ifdef _USE_SYS_VIEW
  SEGGER_SYSVIEW_Print("i2s_write stop");
#endif // _USE_SYS_VIEW
  return bytes_written;
}

int I2SClass::peek()
{
  // TODO
  // peek() is not implemented for ESP
  return 0;
}

void I2SClass::flush()
{
  // do nothing, writes are DMA triggered
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
  return _I2S_DMA_BUFFER_SIZE;
  //uxQueueSpacesAvailable();
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
  Serial.printf("I2SClass::enableTransmitter(): _state = %d\n",_state);
  if (_state != I2S_STATE_TRANSMITTER && _state != I2S_STATE_DUPLEX){
    Serial.printf("I2SClass::enableTransmitter(): change data out pin to %d\n",_sdPin);
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

  xQueueSet = xQueueCreateSet(sizeof(i2s_event)*_I2S_EVENT_QUEUE_LENGTH + 1);
  configASSERT(xQueueSet);
  xQueueAddToSet(_i2sEventQueue, xQueueSet);
  xQueueAddToSet(_task_kill_cmd_semaphore_handle, xQueueSet);

  while(true){
    //Serial.printf("I2S:onTransferComplete: loop start\n");
    xActivatedMember = xQueueSelectFromSet(xQueueSet, portMAX_DELAY);
    if(xActivatedMember == _task_kill_cmd_semaphore_handle){
      Serial.printf("I2S:onTransferComplete: received kill command, breaking from loop\n");
      xSemaphoreTake(_task_kill_cmd_semaphore_handle, 0);
      break; // from the infinite loop
    }else if(xActivatedMember == _i2sEventQueue){
      //Serial.printf("I2S:onTransferComplete: received Q\n");
      xQueueReceive(_i2sEventQueue, &i2s_event, 0);
      if((i2s_event == esp_i2s::I2S_EVENT_TX_DONE) && (_state == I2S_STATE_DUPLEX || _state == I2S_STATE_TRANSMITTER)){
        if(_onTransmit){
          //Serial.printf("I2S:onTransferComplete: calling _onTransmit\n");
#ifdef _USE_SYS_VIEW
          SEGGER_SYSVIEW_Print("_onTransmit start");
#endif // _USE_SYS_VIEW
          _onTransmit();
#ifdef _USE_SYS_VIEW
          SEGGER_SYSVIEW_Print("_onTransmit stop");
#endif // _USE_SYS_VIEW
        }
      }else if(i2s_event == esp_i2s::I2S_EVENT_RX_DONE && (_state == I2S_STATE_RECEIVER || _state == I2S_STATE_DUPLEX)){
        if (_onReceive) {
          //Serial.printf("I2S:onTransferComplete: calling _onReceive\n");
#ifdef _USE_SYS_VIEW
          SEGGER_SYSVIEW_Print("_onReceive start");
#endif // _USE_SYS_VIEW
          _onReceive();
#ifdef _USE_SYS_VIEW
          SEGGER_SYSVIEW_Print("_onReceive stop");
#endif // _USE_SYS_VIEW
        }
      } // if event TX or RX
    }
  }
  Serial.printf("xxxxxxx I2S:onTransferComplete: out of loop - kill my self xxxxxxx\n");
  _callbackTaskHandle = NULL; // prevent secondary termination to non-existing task
  vTaskDelete(NULL);
}

void I2SClass::onDmaTransferComplete(void*)
{
  I2S.onTransferComplete();
}

#if I2S_INTERFACES_COUNT > 0
  I2SClass I2S(I2S_DEVICE, I2S_CLOCK_GENERATOR, PIN_I2S_SD, PIN_I2S_SCK, PIN_I2S_FS); // default - half duplex
  //I2SClass I2S(I2S_DEVICE, I2S_CLOCK_GENERATOR, PIN_I2S_SD, PIN_I2S_SD_OUT, PIN_I2S_SCK, PIN_I2S_FS); // full duplex
#endif

#if I2S_INTERFACES_COUNT > 1
  // TODO set default pins for second module
  //I2SClass I2S1(I2S_DEVICE+1, I2S_CLOCK_GENERATOR, PIN_I2S_SD, PIN_I2S_SCK, PIN_I2S_FS); // default - half duplex
#endif