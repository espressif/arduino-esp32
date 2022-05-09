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
#define _I2S_DMA_BUFFER_COUNT 2 // BUFFER COUNT must be between 2 and 128
#define I2S_INTERFACES_COUNT SOC_I2S_NUM

#ifndef I2S_DEVICE
  #define I2S_DEVICE 0
#endif

#ifndef I2S_CLOCK_GENERATOR
  #define I2S_CLOCK_GENERATOR 0 // does nothing for ESP
#endif

I2SClass::I2SClass(uint8_t deviceIndex, uint8_t clockGenerator, uint8_t sdPin, uint8_t sckPin, uint8_t fsPin) :
  _deviceIndex(deviceIndex),
  _sdPin(sdPin),             // shared data pin
  _inSdPin(PIN_I2S_SD_IN),   // input data pin
  _outSdPin(PIN_I2S_SD),     // output data pin
  _sckPin(sckPin),           // clock pin
  _fsPin(fsPin),             // frame (word) select pin

  _state(I2S_STATE_IDLE),
  _bitsPerSample(0),
  _sampleRate(0),
  _mode(I2S_PHILIPS_MODE),

  _buffer_byte_size(0),

  _driverInstalled(false),
  _initialized(false),
  _callbackTaskHandle(NULL),
  _i2sEventQueue(NULL),
  _i2s_general_mutex(NULL),
  _input_ring_buffer(NULL),
  _output_ring_buffer(NULL),
  _i2s_dma_buffer_size(128), // Number of frames in each DMA buffer. Frame size = number of channels * Bytes per sample; Must be between 8 and 1024
  _driveClock(true),
  _peek_buff(0),
  _peek_buff_valid(false),
  _nesting_counter(0),

  _onTransmit(NULL),
  _onReceive(NULL)
{
  _i2s_general_mutex = xSemaphoreCreateMutex();
  if(_i2s_general_mutex == NULL){
    log_e("I2S could not create internal mutex!");
  }
}

int I2SClass::_createCallbackTask(){
  int stack_size = 20000;
  if(_callbackTaskHandle != NULL){
    log_e("Callback task already exists!");
    return 0; // ERR
  }

  xTaskCreate(
    onDmaTransferComplete,   // Function to implement the task
    "onDmaTransferComplete", // Name of the task
    stack_size,              // Stack size in words
    NULL,                    // Task input parameter
    2,                       // Priority of the task
    &_callbackTaskHandle     // Task handle.
    );
  if(_callbackTaskHandle == NULL){
    log_e("Could not create callback task");
    return 0; // ERR
  }
  return 1; // OK
}

int I2SClass::_installDriver(){
  if(_driverInstalled){
    log_e("I2S driver is already installed");
    return 0; // ERR
  }

  esp_i2s::i2s_mode_t i2s_mode = (esp_i2s::i2s_mode_t)(esp_i2s::I2S_MODE_RX | esp_i2s::I2S_MODE_TX);

  if(_driveClock){
    i2s_mode = (esp_i2s::i2s_mode_t)(i2s_mode | esp_i2s::I2S_MODE_MASTER);
  }else{
    i2s_mode = (esp_i2s::i2s_mode_t)(i2s_mode | esp_i2s::I2S_MODE_SLAVE);
  }

  if(_mode == ADC_DAC_MODE){
    #if (SOC_I2S_SUPPORTS_ADC && SOC_I2S_SUPPORTS_DAC)
      if(_bitsPerSample != 16){ // ADC/DAC can only work in 16-bit sample mode
        log_e("ERROR invalid bps for ADC/DAC. Allowed only 16, requested %d", _bitsPerSample);
        return 0; // ERR
      }
      i2s_mode = (esp_i2s::i2s_mode_t)(i2s_mode | esp_i2s::I2S_MODE_DAC_BUILT_IN | esp_i2s::I2S_MODE_ADC_BUILT_IN);
    #else
      log_e("This chip does not support ADC / DAC mode");
      return 0; // ERR
    #endif
  }else if(_mode == I2S_PHILIPS_MODE ||
           _mode == I2S_RIGHT_JUSTIFIED_MODE ||
           _mode == I2S_LEFT_JUSTIFIED_MODE){ // End of ADC/DAC mode; start of Normal Philips mode
    if(_bitsPerSample != 8 && _bitsPerSample != 16 && _bitsPerSample != 24 &&  _bitsPerSample != 32){
        log_e("Invalid bits per sample for normal mode (requested %d)\nAllowed bps = 8 | 16 | 24 | 32", _bitsPerSample);
      return 0; // ERR
    }
    if(_bitsPerSample == 24){
      log_w("Original Arduino library does not support 24 bits per sample.\nKeep that in mind if you should switch back to Arduino");
    }
  }else if(_mode == PDM_STEREO_MODE || _mode == PDM_MONO_MODE){ // end of Normal Philips mode; start of PDM mode
    #if (SOC_I2S_SUPPORTS_PDM_TX && SOC_I2S_SUPPORTS_PDM_RX)
      i2s_mode = (esp_i2s::i2s_mode_t)(i2s_mode | esp_i2s::I2S_MODE_PDM);
    #else
      log_e("This chip does not support PDM");
      return 0; // ERR
    #endif
  } // Mode
  esp_i2s::i2s_config_t i2s_config = {
    .mode = i2s_mode,
    .sample_rate = _sampleRate,
    .bits_per_sample = (esp_i2s::i2s_bits_per_sample_t)_bitsPerSample,
    .channel_format = esp_i2s::I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = (esp_i2s::i2s_comm_format_t)(esp_i2s::I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
    .dma_buf_count = _I2S_DMA_BUFFER_COUNT,
    .dma_buf_len = _i2s_dma_buffer_size,
    .use_apll = false
  };

  if(_driveClock == false){
    i2s_config.use_apll = true;
    i2s_config.fixed_mclk = 512*_sampleRate;
  }

  // Install and start i2s driver
  while(ESP_OK != esp_i2s::i2s_driver_install((esp_i2s::i2s_port_t) _deviceIndex, &i2s_config, _I2S_EVENT_QUEUE_LENGTH, &_i2sEventQueue)){
    // increase buffer size
    if(2*_i2s_dma_buffer_size <= 1024){
      log_w("WARNING i2s driver install failed.\nTrying to increase I2S DMA buffer size from %d to %d\n", _i2s_dma_buffer_size, 2*_i2s_dma_buffer_size);
      setBufferSize(2*_i2s_dma_buffer_size);
    }else if(_i2s_dma_buffer_size < 1024){
      log_w("WARNING i2s driver install failed.\nTrying to decrease I2S DMA buffer size from %d to 1024\n", _i2s_dma_buffer_size);
      setBufferSize(1024);
    }else{ // install failed with max buffer size
      log_e("ERROR i2s driver install failed");
      return 0; // ERR
    }
  } //try installing with increasing size

  if(_mode == I2S_RIGHT_JUSTIFIED_MODE || _mode == I2S_LEFT_JUSTIFIED_MODE || _mode == PDM_MONO_MODE){ // mono/single channel
    // Set the clock for MONO. Stereo is not supported yet.
    if(ESP_OK != esp_i2s::i2s_set_clk((esp_i2s::i2s_port_t) _deviceIndex, _sampleRate, (esp_i2s::i2s_bits_per_sample_t)_bitsPerSample, esp_i2s::I2S_CHANNEL_MONO)){
      log_e("Setting the I2S Clock has failed!\n");
      return 0; // ERR
    }
  } // mono channel mode

#if (SOC_I2S_SUPPORTS_ADC && SOC_I2S_SUPPORTS_DAC)
  if(_mode == ADC_DAC_MODE){
    esp_i2s::i2s_set_dac_mode(esp_i2s::I2S_DAC_CHANNEL_BOTH_EN);
    esp_i2s::adc_unit_t adc_unit;
    if(!_gpioToAdcUnit((gpio_num_t)_inSdPin, &adc_unit)){
      log_e("pin to adc unit conversion failed");
      return 0; // ERR
    }
    esp_i2s::adc_channel_t adc_channel;
    if(!_gpioToAdcChannel((gpio_num_t)_inSdPin, &adc_channel)){
      log_e("pin to adc channel conversion failed");
      return 0; // ERR
    }
    if(ESP_OK != esp_i2s::i2s_set_adc_mode(adc_unit, (esp_i2s::adc1_channel_t)adc_channel)){
      log_e("i2s_set_adc_mode failed");
      return 0; // ERR
    }
    if(ESP_OK != esp_i2s::i2s_set_pin((esp_i2s::i2s_port_t) _deviceIndex, NULL)){
      log_e("i2s_set_pin failed");
      return 0; // ERR
    }

    if(adc_unit == esp_i2s::ADC_UNIT_1){
      esp_i2s::adc1_config_width(esp_i2s::ADC_WIDTH_BIT_12);
      esp_i2s::adc1_config_channel_atten((esp_i2s::adc1_channel_t)adc_channel, esp_i2s::ADC_ATTEN_DB_11);
    }else if(adc_unit == esp_i2s::ADC_UNIT_2){
      esp_i2s::adc2_config_channel_atten((esp_i2s::adc2_channel_t)adc_channel, esp_i2s::ADC_ATTEN_DB_11);
    }

    esp_i2s::i2s_adc_enable((esp_i2s::i2s_port_t) _deviceIndex);
    _driverInstalled = true;
  }else // End of ADC/DAC mode
#endif // SOC_I2S_SUPPORTS_ADC_DAC
  if(_mode == I2S_PHILIPS_MODE || _mode == I2S_RIGHT_JUSTIFIED_MODE || _mode == I2S_LEFT_JUSTIFIED_MODE || _mode == PDM_STEREO_MODE || _mode == PDM_MONO_MODE){ // if I2S mode
    _driverInstalled = true; // IDF I2S driver must be installed before calling _applyPinSetting
    if(!_applyPinSetting()){
      log_e("could not apply pin setting during driver install");
      _uninstallDriver();
      return 0; // ERR
    }
  } // if I2S _mode
  return 1; // OK
}

// Init in MASTER mode: the SCK and FS pins are driven as outputs using the sample rate
int I2SClass::begin(int mode, int sampleRate, int bitsPerSample){
  _take_if_not_holding();
  // master mode (driving clock and frame select pins - output)
  int ret = begin(mode, sampleRate, bitsPerSample, true);
  _give_if_top_call();
  return ret;
}

// Init in SLAVE mode: the SCK and FS pins are inputs, other side controls sample rate
int I2SClass::begin(int mode, int bitsPerSample){
  _take_if_not_holding();
  // slave mode (not driving clock and frame select pin - input)
  int ret = begin(mode, 96000, bitsPerSample, false);
  _give_if_top_call();
  return ret;
}


// Core function
int I2SClass::begin(int mode, int sampleRate, int bitsPerSample, bool driveClock){
  _take_if_not_holding();
  if(_initialized){
    log_e("ERROR: Object already initialized! Call I2S.end() to disable");
    _give_if_top_call();
    return 0; // ERR
  }
  _driveClock = driveClock;
  _mode = mode;
  _sampleRate = (uint32_t)sampleRate;
  _bitsPerSample = bitsPerSample;

  // There is work in progress on this library.
  if(_bitsPerSample == 16 && _sampleRate > 16000 && driveClock){
    log_w("This sample rate is not officially supported - audio might be noisy.\nTry using sample rate below or equal to 16000");
  }
  if(_bitsPerSample != 16){
    log_w("This bit-per-sample is not officially supported - audio quality might suffer.\nTry using 16bps, with sample rate below or equal 16000");
  }
  if(_mode != I2S_PHILIPS_MODE){
    log_w("This mode is not officially supported - audio quality might suffer.\nAt the moment the only supported mode is I2S_PHILIPS_MODE");
  }

  if (_state != I2S_STATE_IDLE && _state != I2S_STATE_DUPLEX) {
    log_e("Error: unexpected _state (%d)", _state);
    _give_if_top_call();
    return 0; // ERR
  }

  switch (mode) {
    case I2S_PHILIPS_MODE:
    case I2S_RIGHT_JUSTIFIED_MODE:
    case I2S_LEFT_JUSTIFIED_MODE:

    #if (SOC_I2S_SUPPORTS_ADC && SOC_I2S_SUPPORTS_DAC)
      case ADC_DAC_MODE:
    #endif

    case PDM_STEREO_MODE:
    case PDM_MONO_MODE:
      break;

    default: // invalid mode
      log_e("ERROR: unknown mode");
      _give_if_top_call();
      return 0; // ERR
  }

  if(!_installDriver()){
    log_e("ERROR: failed to install driver");
    end();
    _give_if_top_call();
    return 0; // ERR
  }

  _buffer_byte_size = _i2s_dma_buffer_size * (_bitsPerSample / 8) * _I2S_DMA_BUFFER_COUNT * 2;
  _input_ring_buffer  = xRingbufferCreate(_buffer_byte_size, RINGBUF_TYPE_BYTEBUF);
  _output_ring_buffer = xRingbufferCreate(_buffer_byte_size, RINGBUF_TYPE_BYTEBUF);
  if(_input_ring_buffer == NULL || _output_ring_buffer == NULL){
    log_e("ERROR: could not create one or both internal buffers. Requested size = %d\n", _buffer_byte_size);
    _give_if_top_call();
    return 0; // ERR
  }

  if(!_createCallbackTask()){
    log_e("ERROR: failed to create callback task");
    end();
    _give_if_top_call();
    return 0; // ERR
  }
  _initialized = true;
  _give_if_top_call();
  return 1; // OK
}

int I2SClass::_applyPinSetting(){
  if(_driverInstalled){
    esp_i2s::i2s_pin_config_t pin_config = {
      .bck_io_num = _sckPin,
      .ws_io_num = _fsPin,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = I2S_PIN_NO_CHANGE
    };
    if (_state == I2S_STATE_DUPLEX){ // duplex
      pin_config.data_out_num = _outSdPin;
      pin_config.data_in_num = _inSdPin;
    }else{ // simplex
      if(_state == I2S_STATE_RECEIVER){
        pin_config.data_out_num = I2S_PIN_NO_CHANGE;
        pin_config.data_in_num = _sdPin;
      }else if(_state == I2S_STATE_TRANSMITTER){
        pin_config.data_out_num = _sdPin;
        pin_config.data_in_num = I2S_PIN_NO_CHANGE;
      }else{
        pin_config.data_out_num = I2S_PIN_NO_CHANGE;
        pin_config.data_in_num = _sdPin;
      }
    }
    if(ESP_OK != esp_i2s::i2s_set_pin((esp_i2s::i2s_port_t) _deviceIndex, &pin_config)){
      log_e("i2s_set_pin failed; attempted settings: SCK=%d; FS=%d; DIN=%d; DOUT=%d", pin_config.bck_io_num, pin_config.ws_io_num, pin_config.data_in_num, pin_config.data_out_num);
      return 0; // ERR
    }else{
      return 1; // OK
    }
  } // if(_driverInstalled)
  return 1; // OK
}

void I2SClass::_setSckPin(int sckPin){
  _take_if_not_holding();
  if(sckPin >= 0){
    _sckPin = sckPin;
  }else{
    _sckPin = PIN_I2S_SCK;
  }
  _give_if_top_call();
}

int I2SClass::setSckPin(int sckPin){
  _take_if_not_holding();
  _setSckPin(sckPin);
  int ret = _applyPinSetting();
  _applyPinSetting();
  _give_if_top_call();
  return ret;
}

void I2SClass::_setFsPin(int fsPin){
  if(fsPin >= 0){
    _fsPin = fsPin;
  }else{
    _fsPin = PIN_I2S_FS;
  }
}

int I2SClass::setFsPin(int fsPin){
  _take_if_not_holding();
  _setFsPin(fsPin);
  int ret = _applyPinSetting();
  _give_if_top_call();
  return ret;
}

// shared data pin for simplex
void I2SClass::_setDataPin(int sdPin){
  if(sdPin >= 0){
    _sdPin = sdPin;
  }else{
    _sdPin = PIN_I2S_SD;
  }
}

// shared data pin for simplex
int I2SClass::setDataPin(int sdPin){
  _take_if_not_holding();
  _setDataPin(sdPin);
  int ret = _applyPinSetting();
  _give_if_top_call();
  return ret;
}

void I2SClass::_setDataInPin(int inSdPin){
  if(inSdPin >= 0){
    _inSdPin = inSdPin;
  }else{
    _inSdPin = PIN_I2S_SD_IN;
  }
}

int I2SClass::setDataInPin(int inSdPin){
  _take_if_not_holding();
  _setDataInPin(inSdPin);
  int ret = _applyPinSetting();
  _give_if_top_call();
  return ret;
}

void I2SClass::_setDataOutPin(int outSdPin){
  if(outSdPin >= 0){
    _outSdPin = outSdPin;
  }else{
    _outSdPin = PIN_I2S_SD;
  }
}

int I2SClass::setDataOutPin(int outSdPin){
  _take_if_not_holding();
  _setDataOutPin(outSdPin);
  int ret = _applyPinSetting();
  _give_if_top_call();
  return ret;
}

int I2SClass::setAllPins(){
  _take_if_not_holding();
  int ret = setAllPins(PIN_I2S_SCK, PIN_I2S_FS, PIN_I2S_SD, PIN_I2S_SD_OUT, PIN_I2S_SD_IN);
  _give_if_top_call();
  return ret;
}

int I2SClass::setAllPins(int sckPin, int fsPin, int sdPin, int outSdPin, int inSdPin){
  _take_if_not_holding();
  _setSckPin(sckPin);
  _setFsPin(fsPin);
  _setDataPin(sdPin);
  _setDataOutPin(outSdPin);
  _setDataInPin(inSdPin);
  int ret = _applyPinSetting();
  _give_if_top_call();
  return ret;
}

int I2SClass::setDuplex(){
  _take_if_not_holding();
  _state = I2S_STATE_DUPLEX;
  _give_if_top_call();
  return 1;
}

int I2SClass::setSimplex(){
  _take_if_not_holding();
  _state = I2S_STATE_IDLE;
  _give_if_top_call();
  return 1;
}

int I2SClass::isDuplex(){
  _take_if_not_holding();
  int ret = (int)(_state == I2S_STATE_DUPLEX);
  _give_if_top_call();
  return ret;
}

int I2SClass::getSckPin(){
  _take_if_not_holding();
  int ret = _sckPin;
  _give_if_top_call();
  return ret;
}

int I2SClass::getFsPin(){
  _take_if_not_holding();
  int ret = _fsPin;
  _give_if_top_call();
  return ret;
}

int I2SClass::getDataPin(){
  _take_if_not_holding();
  int ret = _sdPin;
  _give_if_top_call();
  return ret;
}

int I2SClass::getDataInPin(){
  _take_if_not_holding();
  int ret = _inSdPin;
  _give_if_top_call();
  return ret;
}

int I2SClass::getDataOutPin(){
  _take_if_not_holding();
  int ret = _outSdPin;
  _give_if_top_call();
  return ret;
}

void I2SClass::_uninstallDriver(){
  if(_driverInstalled){
    #if (SOC_I2S_SUPPORTS_ADC && SOC_I2S_SUPPORTS_DAC)
      if(_mode == ADC_DAC_MODE){
        esp_i2s::i2s_adc_disable((esp_i2s::i2s_port_t) _deviceIndex);
      }
    #endif
    esp_i2s::i2s_driver_uninstall((esp_i2s::i2s_port_t) _deviceIndex);

    if(_state != I2S_STATE_DUPLEX){
      _state = I2S_STATE_IDLE;
    }
    _driverInstalled = false;
  } // if(_driverInstalled)
}

void I2SClass::end(){
  _take_if_not_holding();
  if(xTaskGetCurrentTaskHandle() != _callbackTaskHandle){
    if(_callbackTaskHandle){
      vTaskDelete(_callbackTaskHandle);
      _callbackTaskHandle = NULL; // prevent secondary termination to non-existing task
    }
    _uninstallDriver();
    _onTransmit = NULL;
    _onReceive  = NULL;
    if(_input_ring_buffer != NULL){
      vRingbufferDelete(_input_ring_buffer);
      _input_ring_buffer = NULL;
    }
    if(_output_ring_buffer != NULL){
      vRingbufferDelete(_output_ring_buffer);
      _output_ring_buffer = NULL;
    }
    _initialized = false;
  }else{
    log_w("WARNING: ending I2SClass from callback task not permitted, but attempted!");
  }
  _give_if_top_call();
}

// Bytes available to read
int I2SClass::available(){
  _take_if_not_holding();
  int ret = 0;
  if(_input_ring_buffer != NULL){
    ret = _buffer_byte_size - (int)xRingbufferGetCurFreeSize(_input_ring_buffer);
  }
  _give_if_top_call();
  return ret;
}

union i2s_sample_t {
  uint8_t b8;
  int16_t b16;
  int32_t b32;
};

int I2SClass::read(){
  _take_if_not_holding();
  i2s_sample_t sample;
  sample.b32 = 0;
  if(_initialized){
    read(&sample, _bitsPerSample / 8);

    if (_bitsPerSample == 32) {
      _give_if_top_call();
      return sample.b32;
    } else if (_bitsPerSample == 16) {
      _give_if_top_call();
      return sample.b16;
    } else if (_bitsPerSample == 8) {
      _give_if_top_call();
      return sample.b8;
    } else {
      _give_if_top_call();
      return 0; // sample value
    }
  } // if(_initialized)
  _give_if_top_call();
  return 0; // sample value
}

int I2SClass::read(void* buffer, size_t size){
  _take_if_not_holding();
  size_t requested_size = size;
  if(_initialized){
    if(!_enableReceiver()){
      _give_if_top_call();
      return 0; // There was an error switching to receiver
    } // _enableReceiver succeeded ?

    size_t item_size = 0;
    void *tmp_buffer;
    if(_input_ring_buffer != NULL){
      if(_peek_buff_valid){
        memcpy(buffer, &_peek_buff, _bitsPerSample/8);
        _peek_buff_valid = false;
        requested_size -= _bitsPerSample/8;
      }
      tmp_buffer = xRingbufferReceiveUpTo(_input_ring_buffer, &item_size, pdMS_TO_TICKS(1000), requested_size);
      if(tmp_buffer != NULL){
        memcpy(buffer, tmp_buffer, item_size);
        #if (SOC_I2S_SUPPORTS_ADC && SOC_I2S_SUPPORTS_DAC)
          if(_mode == ADC_DAC_MODE){
            for(size_t i = 0; i < item_size / 2; ++i){
              ((uint16_t*)buffer)[i] = ((uint16_t*)buffer)[i] & 0x0FFF;
            }
          } // ADC/DAC mode
        #endif
        vRingbufferReturnItem(_input_ring_buffer, tmp_buffer);
        _give_if_top_call();
        return item_size;
      }else{
        log_w("input buffer is empty - timed out");
        _give_if_top_call();
        return 0; // 0 Bytes read / ERR
      } // tmp buffer not NULL ?
    } // ring buffer not NULL ?
  } // if(_initialized)
  _give_if_top_call();
  return 0; // 0 Bytes read / ERR
}

size_t I2SClass::write(uint8_t data){
  _take_if_not_holding();
  size_t ret = 0;
  if(_initialized){
    ret = write_blocking((int32_t*)&data, 1);
  }
  _give_if_top_call();
  return ret;
}

size_t I2SClass::write(int32_t sample){
  _take_if_not_holding();
  size_t ret = 0;
  if(_initialized){
    ret = write_blocking(&sample, _bitsPerSample/8);
  }
  _give_if_top_call();
  return ret;
}

size_t I2SClass::write(const uint8_t *buffer, size_t size){
  _take_if_not_holding();
  size_t ret = 0;
  if(_initialized){
    ret = write((const void*)buffer, size);
  }
  _give_if_top_call();
  return ret;
}

size_t I2SClass::write(const void *buffer, size_t size){
  _take_if_not_holding();
  size_t ret = 0;
  if(_initialized){
    //size_t ret = write_blocking(buffer, size);
    ret = write_nonblocking(buffer, size);
  } // if(_initialized)
  _give_if_top_call();
  return ret;
}

// blocking version of write
// This version of write will wait indefinitely to write requested samples
// into output buffer
size_t I2SClass::write_blocking(const void *buffer, size_t size){
  _take_if_not_holding();
  if(_initialized){
    if(!_enableTransmitter()){
      _give_if_top_call();
      return 0; // There was an error switching to transmitter
    } // _enableTransmitter succeeded ?

    if(_output_ring_buffer != NULL){
      int ret = xRingbufferSend(_output_ring_buffer, buffer, size, portMAX_DELAY);
      if(pdTRUE == ret){
        _give_if_top_call();
        return size;
      }else{
        log_e("xRingbufferSend() with infinite wait returned with error");
        _give_if_top_call();
        return 0;
      } // ring buffer send ok ?
    } // ring buffer not NULL ?
  } // if(_initialized)
  return 0;
  log_w("I2S not initialized");
  _give_if_top_call();
  return 0;
}

// non-blocking version of write
// In case there is not enough space in buffer to write requested size
// this function will try to flush the buffer and write requested data with 0 time-out
size_t I2SClass::write_nonblocking(const void *buffer, size_t size){
  _take_if_not_holding();
  if(_initialized){
    if(_state != I2S_STATE_TRANSMITTER && _state != I2S_STATE_DUPLEX){
      if(!_enableTransmitter()){
        _give_if_top_call();
        return 0; // There was an error switching to transmitter
      }
    }
    if(availableForWrite() < size){
      flush();
    }
    if(_output_ring_buffer != NULL){
      if(pdTRUE == xRingbufferSend(_output_ring_buffer, buffer, size, 0)){
        _give_if_top_call();
        return size;
      }else{
        log_w("I2S could not write all data into ring buffer!");
        _give_if_top_call();
        return 0;
      }
    }
  } // if(_initialized)
  return 0;
  _give_if_top_call(); // this should not be needed
}

/*
  Read 1 sample from internal buffer and return it.
  Repeated peeks will return the same sample until read is called.
*/
int I2SClass::peek(){
  _take_if_not_holding();
  int ret = 0;
  if(_initialized && _input_ring_buffer != NULL && !_peek_buff_valid){
    size_t item_size = 0;
    void *item = NULL;

    item = xRingbufferReceiveUpTo(_input_ring_buffer, &item_size, 0, _bitsPerSample/8); // fetch 1 sample
    if (item != NULL && item_size == _bitsPerSample/8){
      _peek_buff = *((int*)item);
      vRingbufferReturnItem(_input_ring_buffer, item);
      _peek_buff_valid = true;
    }

  } // if(_initialized)
  if(_peek_buff_valid){
    ret = _peek_buff;
  }
  _give_if_top_call();
  return ret;
}

void I2SClass::flush(){
  _take_if_not_holding();
  if(_initialized){
    const size_t single_dma_buf = _i2s_dma_buffer_size*(_bitsPerSample/8)*2;
    size_t item_size = 0;
    void *item = NULL;
    if(_output_ring_buffer != NULL){
      item = xRingbufferReceiveUpTo(_output_ring_buffer, &item_size, 0, single_dma_buf);
      if (item != NULL){
        _fix_and_write(item, item_size);
        vRingbufferReturnItem(_output_ring_buffer, item);
      }
    }
  } // if(_initialized)
  _give_if_top_call();
}

// Bytes available to write
int I2SClass::availableForWrite(){
  _take_if_not_holding();
  int ret = 0;
  if(_initialized){
    if(_output_ring_buffer != NULL){
      ret = (int)xRingbufferGetCurFreeSize(_output_ring_buffer);
    }
  } // if(_initialized)
  _give_if_top_call();
  return ret;
}

void I2SClass::onTransmit(void(*function)(void)){
  _take_if_not_holding();
  _onTransmit = function;
  _give_if_top_call();
}

void I2SClass::onReceive(void(*function)(void)){
  _take_if_not_holding();
  _onReceive = function;
  _give_if_top_call();
}

int I2SClass::setBufferSize(int bufferSize){
  _take_if_not_holding();
  int ret = 0;
  if(bufferSize >= 8 && bufferSize <= 1024){
    _i2s_dma_buffer_size = bufferSize;
  }else{
    log_e("setBufferSize: wrong input! Buffer size must be between 8 and 1024. Requested %d", bufferSize);
    _give_if_top_call();
    return 0; // ERR
  } // check requested buffer size

  if(_initialized){
    _uninstallDriver();
    ret = _installDriver();
    _give_if_top_call();
    return ret;
  }else{ // check requested buffer size
    _give_if_top_call();
    return 1; // It's ok to change buffer size for uninitialized driver - new size will be used on begin()
  } // if(_initialized)
  _give_if_top_call();
  return 0; // ERR
}

int I2SClass::getBufferSize(){
  _take_if_not_holding();
  int ret = _i2s_dma_buffer_size;
  _give_if_top_call();
  return ret;
}

int I2SClass::_enableTransmitter(){
  if(_state != I2S_STATE_DUPLEX && _state != I2S_STATE_TRANSMITTER){
    _state = I2S_STATE_TRANSMITTER;
    return _applyPinSetting();
  }
  return 1; // Ok
}

int I2SClass::_enableReceiver(){
  if(_state != I2S_STATE_DUPLEX && _state != I2S_STATE_RECEIVER){
    _state = I2S_STATE_RECEIVER;
    return _applyPinSetting();
  }
  return 1; // Ok
}

void I2SClass::_tx_done_routine(uint8_t* prev_item){
  static bool prev_item_valid = false;
  const size_t single_dma_buf = _i2s_dma_buffer_size*(_bitsPerSample/8)*2; // *2 for stereo - it has double number of samples for 2 channels
  static size_t item_size = 0;
  static size_t prev_item_size = 0;
  static void *item = NULL;
  static int prev_item_offset = 0;
  static size_t bytes_written = 0;

  if(prev_item_valid){ // use item from previous round
    _fix_and_write(prev_item+prev_item_offset, prev_item_size, &bytes_written);
    if(prev_item_size == bytes_written){
      prev_item_valid = false;
    } // write size check
    prev_item_offset = bytes_written;
    prev_item_size -= bytes_written;
  } // prev_item_valid

  if(_output_ring_buffer != NULL && (_buffer_byte_size - xRingbufferGetCurFreeSize(_output_ring_buffer) >= single_dma_buf)){ // fill up the I2S DMA buffer
    bytes_written = 0;
    item_size = 0;
    if(_buffer_byte_size - xRingbufferGetCurFreeSize(_output_ring_buffer) >= _i2s_dma_buffer_size*(_bitsPerSample/8)){ // don't read from almost empty buffer
      item = xRingbufferReceiveUpTo(_output_ring_buffer, &item_size, pdMS_TO_TICKS(0), single_dma_buf);
      if (item != NULL){
        _fix_and_write(item, item_size, &bytes_written);
        if(item_size != bytes_written){ // save item that was not written correctly for later
          memcpy(prev_item, (void*)&((uint8_t*)item)[bytes_written], item_size-bytes_written);
          prev_item_size = item_size - bytes_written;
          prev_item_offset = 0;
          prev_item_valid = true;
        } // save item that was not written correctly for later
        vRingbufferReturnItem(_output_ring_buffer, item);
      } // Check received item
    } // don't read from almost empty buffer
  } // fill up the I2S DMA buffer
  if(_onTransmit){
    _onTransmit();
  } // user callback
}

void I2SClass::_rx_done_routine(){
  size_t bytes_read = 0;
  const size_t single_dma_buf = _i2s_dma_buffer_size*(_bitsPerSample/8);

  if(_input_ring_buffer != NULL){
    uint8_t *_inputBuffer = (uint8_t*)malloc(_i2s_dma_buffer_size*4);
    size_t avail = xRingbufferGetCurFreeSize(_input_ring_buffer);
    if(avail > 0){
      esp_err_t ret = esp_i2s::i2s_read((esp_i2s::i2s_port_t) _deviceIndex, _inputBuffer, avail <= single_dma_buf ? avail : single_dma_buf, (size_t*) &bytes_read, 0);
      if(ret != ESP_OK){
        log_w("i2s_read returned with error %d", ret);
      }
      _post_read_data_fix(_inputBuffer, &bytes_read);
    }

    if(bytes_read > 0){ // when read more than 0, then send to ring buffer
      if(pdTRUE != xRingbufferSend(_input_ring_buffer, _inputBuffer, bytes_read, 0)){
        log_w("I2S failed to send item from DMA to internal buffer\n");
      } // xRingbufferSendComplete
    } // if(bytes_read > 0)
    free(_inputBuffer);
    if (_onReceive && avail < _buffer_byte_size){ // when user callback is registered && and there is some data in ring buffer to read
      _onReceive();
    } // user callback
  }
}

void I2SClass::_onTransferComplete(){
  uint8_t prev_item[_i2s_dma_buffer_size*4];
  esp_i2s::i2s_event_t i2s_event;

  while(true){
    xQueueReceive(_i2sEventQueue, &i2s_event, portMAX_DELAY);
    if(i2s_event.type == esp_i2s::I2S_EVENT_TX_DONE){
      _tx_done_routine(prev_item);
    }else if(i2s_event.type == esp_i2s::I2S_EVENT_RX_DONE){
      _rx_done_routine();
    } // RX Done
  } // infinite loop
}

void I2SClass::onDmaTransferComplete(void*){
  I2S._onTransferComplete();
}

void I2SClass::_take_if_not_holding(){
  TaskHandle_t mutex_holder = xSemaphoreGetMutexHolder(_i2s_general_mutex);
  if(mutex_holder != NULL && mutex_holder == xTaskGetCurrentTaskHandle()){
    ++_nesting_counter;
    return; // we are already holding this mutex - no need to take it
  }

  // we are not holding the mutex - wait for it and take it
  if(xSemaphoreTake(_i2s_general_mutex, portMAX_DELAY) != pdTRUE ){
    log_e("I2S internal mutex take returned with error");
  }
  //_give_if_top_call(); // call after this function
}

void I2SClass::_give_if_top_call(){
  if(_nesting_counter){
    --_nesting_counter;
  }else{
    if(xSemaphoreGive(_i2s_general_mutex) != pdTRUE){
      log_e("I2S internal mutex give error");
    }
  }
}


// Fixes data in-situ received from esp i2s driver. After fixing they reflect what was on the bus.
// input - bytes as received from i2s_read - this serves as input and output buffer
// size - number of bytes (this may be changed during operation)
void I2SClass::_post_read_data_fix(void *input, size_t *size){
  ulong dst_ptr = 0;
  switch(_bitsPerSample){
    case 8:
      for(int i = 0; i < *size; i+=4){
        ((uint8_t*)input)[dst_ptr++] = ((uint8_t*)input)[i+3];
        ((uint8_t*)input)[dst_ptr++] = ((uint8_t*)input)[i+1];
      }
      *size /= 2;
    break;
    case 16:
      uint16_t tmp;
      for(int i = 0; i < *size/2; i+=2){
        tmp = ((uint16_t*)input)[i];
        ((uint16_t*)input)[dst_ptr++] = ((uint16_t*)input)[i+1];
        ((uint16_t*)input)[dst_ptr++] = tmp;
      }
      break;
      default: ; // Do nothing
  } // switch
}

// Prepares data and writes them to IDF i2s driver.
// This counters possible bug in ESP IDF I2S driver
// output - bytes to be sent
// size - number of bytes in original buffer
// bytes_written - number of bytes used from original buffer
// actual_bytes_written - number of bytes written by i2s_write after fix
void I2SClass::_fix_and_write(void *output, size_t size, size_t *bytes_written, size_t *actual_bytes_written){
  ulong src_ptr = 0;
  uint8_t* buff = NULL;
  size_t buff_size = size;
  switch(_bitsPerSample){
    case 8:
      buff_size = size *2;
      buff = (uint8_t*)calloc(buff_size, sizeof(uint8_t));
      if(buff == NULL){
        log_e("callock error");
        if(bytes_written != NULL){ *bytes_written = 0; }
        return;
      }
      for(int i = 0; i < buff_size ; i+=4){
        ((uint8_t*)buff)[i+3] = (uint16_t)((uint8_t*)output)[src_ptr++];
        ((uint8_t*)buff)[i+1] = (uint16_t)((uint8_t*)output)[src_ptr++];
      }
    break;
    case 16:
      buff = (uint8_t*)malloc(buff_size);
      if(buff == NULL){
        log_e("malloc error");
        if(bytes_written != NULL){ *bytes_written = 0; }
        return;
      }
      for(int i = 0; i < size/2; i += 2 ){
        ((uint16_t*)buff)[i]   = ((uint16_t*)output)[i+1]; // [1] <- [0]
        ((uint16_t*)buff)[i+1] = ((uint16_t*)output)[i]; // [0] <- [1]
      }
    break;
    case 24:
      buff = (uint8_t*)output;
      break;
    case 32:
      buff = (uint8_t*)output;
      break;
    default: ; // Do nothing
  } // switch

  size_t _bytes_written;
  esp_err_t ret = esp_i2s::i2s_write((esp_i2s::i2s_port_t) _deviceIndex, buff, buff_size, &_bytes_written, 0); // fixed
  if(ret != ESP_OK){
    log_e("Error: writing data to i2s - function returned with err code %d", ret);
  }
  if(ret == ESP_OK && buff_size != _bytes_written){
    log_w("Warning: writing data to i2s - written %d B instead of requested %d B", _bytes_written, buff_size);
  }
  // free if the buffer was actually allocated
  if(_bitsPerSample == 8 || _bitsPerSample == 16){
    free(buff);
  }
  if(bytes_written != NULL){
    *bytes_written = _bitsPerSample == 8 ? _bytes_written/2 : _bytes_written;
  }
  if(actual_bytes_written != NULL){
    *actual_bytes_written = _bytes_written;
  }
}


#if (SOC_I2S_SUPPORTS_ADC && SOC_I2S_SUPPORTS_DAC)
int I2SClass::_gpioToAdcUnit(gpio_num_t gpio_num, esp_i2s::adc_unit_t* adc_unit){
  switch(gpio_num){
#if CONFIG_IDF_TARGET_ESP32
    // ADC 1
    case GPIO_NUM_36:
    case GPIO_NUM_37:
    case GPIO_NUM_38:
    case GPIO_NUM_39:
    case GPIO_NUM_32:
    case GPIO_NUM_33:
    case GPIO_NUM_34:
    case GPIO_NUM_35:
      *adc_unit = esp_i2s::ADC_UNIT_1;
      return 1; // OK

    // ADC 2
    case GPIO_NUM_0:
      log_w("GPIO 0 for ADC should not be used for dev boards due to external auto program circuits.");
    case GPIO_NUM_4:
    case GPIO_NUM_2:
    case GPIO_NUM_15:
    case GPIO_NUM_13:
    case GPIO_NUM_12:
    case GPIO_NUM_14:
    case GPIO_NUM_27:
    case GPIO_NUM_25:
    case GPIO_NUM_26:
      *adc_unit = esp_i2s::ADC_UNIT_2;
      return 1; // OK
#endif

#if (CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3)
    case GPIO_NUM_1:
    case GPIO_NUM_2:
    case GPIO_NUM_3:
    case GPIO_NUM_4:
    case GPIO_NUM_5:
    case GPIO_NUM_6:
    case GPIO_NUM_7:
    case GPIO_NUM_8:
    case GPIO_NUM_9:
    case GPIO_NUM_10:
      *adc_unit = esp_i2s::ADC_UNIT_1;
      return 1; // OK
#endif

#if CONFIG_IDF_TARGET_ESP32S2
    case GPIO_NUM_11:
    case GPIO_NUM_12:
    case GPIO_NUM_13:
    case GPIO_NUM_14:
    case GPIO_NUM_15:
    case GPIO_NUM_16:
    case GPIO_NUM_17:
    case GPIO_NUM_18:
    case GPIO_NUM_19:
    case GPIO_NUM_20:
      *adc_unit = esp_i2s::ADC_UNIT_2;
      return 1; // OK
#endif

#if (CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32H2)
    case GPIO_NUM_0:
    case GPIO_NUM_1:
    case GPIO_NUM_2:
    case GPIO_NUM_3:
    case GPIO_NUM_4:
      *adc_unit = esp_i2s::ADC_UNIT_1;
      return 1; // OK
    case GPIO_NUM_5:
      *adc_unit = esp_i2s::ADC_UNIT_2;
      return 1; // OK
#endif
    default:
      log_e("GPIO %d not usable for ADC!", gpio_num);
      log_i("Please refer to documentation https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html");
      return 0; // ERR
  }
}

int I2SClass::_gpioToAdcChannel(gpio_num_t gpio_num, esp_i2s::adc_channel_t* adc_channel){
 switch(gpio_num){
#if CONFIG_IDF_TARGET_ESP32
    // ADC 1
    case GPIO_NUM_36: *adc_channel =  esp_i2s::ADC_CHANNEL_0; return 1; // OK
    case GPIO_NUM_37: *adc_channel =  esp_i2s::ADC_CHANNEL_1; return 1; // OK
    case GPIO_NUM_38: *adc_channel =  esp_i2s::ADC_CHANNEL_2; return 1; // OK
    case GPIO_NUM_39: *adc_channel =  esp_i2s::ADC_CHANNEL_3; return 1; // OK
    case GPIO_NUM_32: *adc_channel =  esp_i2s::ADC_CHANNEL_4; return 1; // OK
    case GPIO_NUM_33: *adc_channel =  esp_i2s::ADC_CHANNEL_5; return 1; // OK
    case GPIO_NUM_34: *adc_channel =  esp_i2s::ADC_CHANNEL_6; return 1; // OK
    case GPIO_NUM_35: *adc_channel =  esp_i2s::ADC_CHANNEL_7; return 1; // OK

    // ADC 2
    case GPIO_NUM_0:
      log_w("GPIO 0 for ADC should not be used for dev boards due to external auto program circuits.");
      *adc_channel =  esp_i2s::ADC_CHANNEL_1; return 1; // OK
    case GPIO_NUM_4:  *adc_channel =  esp_i2s::ADC_CHANNEL_0; return 1; // OK
    case GPIO_NUM_2:  *adc_channel =  esp_i2s::ADC_CHANNEL_2; return 1; // OK
    case GPIO_NUM_15: *adc_channel =  esp_i2s::ADC_CHANNEL_3; return 1; // OK
    case GPIO_NUM_13: *adc_channel =  esp_i2s::ADC_CHANNEL_4; return 1; // OK
    case GPIO_NUM_12: *adc_channel =  esp_i2s::ADC_CHANNEL_5; return 1; // OK
    case GPIO_NUM_14: *adc_channel =  esp_i2s::ADC_CHANNEL_6; return 1; // OK
    case GPIO_NUM_27: *adc_channel =  esp_i2s::ADC_CHANNEL_7; return 1; // OK
    case GPIO_NUM_25: *adc_channel =  esp_i2s::ADC_CHANNEL_8; return 1; // OK
    case GPIO_NUM_26: *adc_channel =  esp_i2s::ADC_CHANNEL_9; return 1; // OK
#endif

#if (CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3)
    case GPIO_NUM_1: *adc_channel =  esp_i2s::ADC_CHANNEL_0; return 1; // OK
    case GPIO_NUM_2: *adc_channel =  esp_i2s::ADC_CHANNEL_1; return 1; // OK
    case GPIO_NUM_3: *adc_channel =  esp_i2s::ADC_CHANNEL_2; return 1; // OK
    case GPIO_NUM_4: *adc_channel =  esp_i2s::ADC_CHANNEL_3; return 1; // OK
    case GPIO_NUM_5: *adc_channel =  esp_i2s::ADC_CHANNEL_4; return 1; // OK
    case GPIO_NUM_6: *adc_channel =  esp_i2s::ADC_CHANNEL_5; return 1; // OK
    case GPIO_NUM_7: *adc_channel =  esp_i2s::ADC_CHANNEL_6; return 1; // OK
    case GPIO_NUM_8: *adc_channel =  esp_i2s::ADC_CHANNEL_7; return 1; // OK
    case GPIO_NUM_9: *adc_channel =  esp_i2s::ADC_CHANNEL_8; return 1; // OK
    case GPIO_NUM_10: *adc_channel =  esp_i2s::ADC_CHANNEL_9; return 1; // OK
#endif

#if CONFIG_IDF_TARGET_ESP32S2
    case GPIO_NUM_11: *adc_channel =  esp_i2s::ADC_CHANNEL_0; return 1; // OK
    case GPIO_NUM_12: *adc_channel =  esp_i2s::ADC_CHANNEL_1; return 1; // OK
    case GPIO_NUM_13: *adc_channel =  esp_i2s::ADC_CHANNEL_2; return 1; // OK
    case GPIO_NUM_14: *adc_channel =  esp_i2s::ADC_CHANNEL_3; return 1; // OK
    case GPIO_NUM_15: *adc_channel =  esp_i2s::ADC_CHANNEL_4; return 1; // OK
    case GPIO_NUM_16: *adc_channel =  esp_i2s::ADC_CHANNEL_5; return 1; // OK
    case GPIO_NUM_17: *adc_channel =  esp_i2s::ADC_CHANNEL_6; return 1; // OK
    case GPIO_NUM_18: *adc_channel =  esp_i2s::ADC_CHANNEL_7; return 1; // OK
    case GPIO_NUM_19: *adc_channel =  esp_i2s::ADC_CHANNEL_8; return 1; // OK
    case GPIO_NUM_20: *adc_channel =  esp_i2s::ADC_CHANNEL_9; return 1; // OK
#endif

#if (CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32H2)
    case GPIO_NUM_0: *adc_channel =  esp_i2s::ADC_CHANNEL_0; return 1; // OK
    case GPIO_NUM_1: *adc_channel =  esp_i2s::ADC_CHANNEL_1; return 1; // OK
    case GPIO_NUM_2: *adc_channel =  esp_i2s::ADC_CHANNEL_2; return 1; // OK
    case GPIO_NUM_3: *adc_channel =  esp_i2s::ADC_CHANNEL_3; return 1; // OK
    case GPIO_NUM_4: *adc_channel =  esp_i2s::ADC_CHANNEL_4; return 1; // OK
    case GPIO_NUM_5: *adc_channel =  esp_i2s::ADC_CHANNEL_0; return 1; // OK
#endif
    default:
      log_e("GPIO %d not usable for ADC!", gpio_num);
      log_i("Please refer to documentation https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html");
      return 0; // ERR
  }
}
#endif // SOC_I2S_SUPPORTS_ADC_DAC

#if I2S_INTERFACES_COUNT > 0
  I2SClass I2S(I2S_DEVICE, I2S_CLOCK_GENERATOR, PIN_I2S_SD, PIN_I2S_SCK, PIN_I2S_FS); // default - half duplex
#endif

#if I2S_INTERFACES_COUNT > 1
  // TODO set default pins for second module
  //I2SClass I2S1(I2S_DEVICE+1, I2S_CLOCK_GENERATOR, PIN_I2S_SD, PIN_I2S_SCK, PIN_I2S_FS); // default - half duplex
#endif
