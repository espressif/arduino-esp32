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

#ifndef I2S_CLOCK_GENERATOR
  #define I2S_CLOCK_GENERATOR 0 // does nothing for ESP
#endif

I2SClass::I2SClass(uint8_t deviceIndex, uint8_t clockGenerator, uint8_t sdPin, uint8_t sckPin, uint8_t fsPin) :
  _deviceIndex(deviceIndex),
#if SOC_I2S_NUM > 1
  _mclkPin(I2S_PIN_NO_CHANGE), // By default the MCLK pin is not assigned
  _inSdPin(deviceIndex == 0 ? PIN_I2S_SD_IN : PIN_I2S1_SD_IN),   // input data pin
  _outSdPin(deviceIndex == 0 ? PIN_I2S_SD : PIN_I2S1_SD),     // output data pin
#else
  _mclkPin(I2S_PIN_NO_CHANGE), // By default the MCLK pin is not assigned
  _inSdPin(PIN_I2S_SD_IN),   // input data pin
  _outSdPin(PIN_I2S_SD),     // output data pin
#endif
  _sckPin(sckPin),           // clock pin
  _fsPin(fsPin),             // frame (word) select pin
  _sdPin(sdPin),             // shared data pin

  _state(I2S_STATE_IDLE),
  _bitsPerSample(0),
  _sampleRate(0),
  _mode(I2S_PHILIPS_MODE),

  _driverInstalled(false),
  _initialized(false),
  _callbackTaskHandle(NULL),
  _i2sEventQueue(NULL),
  _i2s_general_mutex(NULL),
  _input_ring_buffer(NULL),
  _output_ring_buffer(NULL),
  _i2s_dma_buffer_frame_size(128), // Number of frames in each DMA buffer. Must be between 8 and 1024. Frame size = number of channels (always 2) * Bytes per sample
  _driveClock(true),
  _peek_buff(0),
  _peek_buff_valid(false),
  _nesting_counter(0),

  _onTransmit(NULL),
  _onReceive(NULL)
{
  _i2s_general_mutex = xSemaphoreCreateRecursiveMutex();
  if(_i2s_general_mutex == NULL){
    log_e("(I2S#%d) I2S could not create internal mutex!", _deviceIndex);
  }
}

int I2SClass::_createCallbackTask(){
  int stack_size = 20000;
  if(_callbackTaskHandle != NULL){
    log_e("(I2S#%d) Callback task already exists!", _deviceIndex);
    return 0; // ERR
  }

  xTaskCreate(
    onDmaTransferComplete,   // Function to implement the task
    "onDmaTransferComplete", // Name of the task
    stack_size,              // Stack size in words
    (void *)&_deviceIndex,   // Task input parameter
    4,                       // Priority of the task
    &_callbackTaskHandle     // Task handle.
    );
  if(_callbackTaskHandle == NULL){
    log_e("(I2S#%d) Could not create callback task", _deviceIndex);
    return 0; // ERR
  }
  return 1; // OK
}

int I2SClass::_installDriver(){
  if(_driverInstalled){
    log_e("(I2S#%d) I2S driver is already installed", _deviceIndex);
    return 0; // ERR
  }

  if(_deviceIndex >= SOC_I2S_NUM){
    log_e("Max allowed I2S device number is %d but requested %d", SOC_I2S_NUM-1, _deviceIndex);
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
        log_e("(I2S#%d) ERROR invalid bps for ADC/DAC. Allowed only 16, requested %d", _deviceIndex, _bitsPerSample);
        // TODO handle data transfer and allow users to use any bps
        return 0; // ERR
      }
      i2s_mode = (esp_i2s::i2s_mode_t)(i2s_mode | esp_i2s::I2S_MODE_DAC_BUILT_IN | esp_i2s::I2S_MODE_ADC_BUILT_IN);
    #else
      log_e("(I2S#%d) This chip does not support ADC / DAC mode", _deviceIndex);
      return 0; // ERR
    #endif
  }else if(_mode == I2S_PHILIPS_MODE ||
           _mode == I2S_RIGHT_JUSTIFIED_MODE ||
           _mode == I2S_LEFT_JUSTIFIED_MODE){ // End of ADC/DAC mode; start of Normal Philips mode
    if(_bitsPerSample != 8 && _bitsPerSample != 16 && _bitsPerSample != 24 &&  _bitsPerSample != 32){
        log_e("(I2S#%d) Invalid bits per sample for normal mode (requested %d)\nAllowed bps = 8 | 16 | 24 | 32", _deviceIndex, _bitsPerSample);
      return 0; // ERR
    }
    if(_bitsPerSample == 24){
      log_w("(I2S#%d) Original Arduino library does not support 24 bits per sample.\nKeep that in mind if you should switch back to Arduino", _deviceIndex);
    }
  }else if(_mode == PDM_STEREO_MODE || _mode == PDM_MONO_MODE){ // end of Normal Philips mode; start of PDM mode
    #if (SOC_I2S_SUPPORTS_PDM_TX || SOC_I2S_SUPPORTS_PDM_RX)
      i2s_mode = (esp_i2s::i2s_mode_t)(i2s_mode | esp_i2s::I2S_MODE_PDM);
      #ifndef SOC_I2S_SUPPORTS_PDM_RX
        i2s_mode = (esp_i2s::i2s_mode_t)(i2s_mode & ~esp_i2s::I2S_MODE_RX); // remove PRM RX which is not supported on ESP32-C3
      #endif
    #else
      log_e("(I2S#%d) This chip does not support PDM", _deviceIndex);
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
    .dma_buf_len = _i2s_dma_buffer_frame_size,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0,
    .mclk_multiple = esp_i2s::I2S_MCLK_MULTIPLE_DEFAULT,
    .bits_per_chan = esp_i2s::I2S_BITS_PER_CHAN_DEFAULT,
#if SOC_I2S_SUPPORTS_TDM
    .chan_mask = (esp_i2s::i2s_channel_t)(esp_i2s::I2S_TDM_ACTIVE_CH0 | esp_i2s::I2S_TDM_ACTIVE_CH1),
    .total_chan = 2,
    .left_align = false,
    .big_edin = false,
    .bit_order_msb = false,
    .skip_msk = false
#endif
  };

  if(_driveClock == false){
    i2s_config.use_apll = true;
    i2s_config.fixed_mclk = 8*_sampleRate;
  }

  if(_mode == ADC_DAC_MODE){
    i2s_config.communication_format = (esp_i2s::i2s_comm_format_t)(esp_i2s::I2S_COMM_FORMAT_STAND_MSB);
  }

  if(_mode == I2S_RIGHT_JUSTIFIED_MODE){
    i2s_config.communication_format = (esp_i2s::i2s_comm_format_t)(esp_i2s::I2S_CHANNEL_FMT_ALL_RIGHT); // Load right channel data in both two channels
  }

  if(_mode == I2S_LEFT_JUSTIFIED_MODE){
    i2s_config.communication_format = (esp_i2s::i2s_comm_format_t)(esp_i2s::I2S_CHANNEL_FMT_ALL_LEFT); // Load left channel data in both two channels
  }

  // Install and start i2s driver
  while(ESP_OK != esp_i2s::i2s_driver_install((esp_i2s::i2s_port_t) _deviceIndex, &i2s_config, _I2S_EVENT_QUEUE_LENGTH, &_i2sEventQueue)){
    // Double the DMA buffer size
    if(2*_i2s_dma_buffer_frame_size <= 1024){
      log_w("(I2S#%d) WARNING i2s driver install failed.\nTrying to double the I2S DMA buffer size from %d to %d", _deviceIndex, _i2s_dma_buffer_frame_size, 2*_i2s_dma_buffer_frame_size);
      setDMABufferFrameSize(2*_i2s_dma_buffer_frame_size); // Double the buffer size
    }else if(_i2s_dma_buffer_frame_size < 1024){
      log_w("(I2S#%d) WARNING i2s driver install failed.\nTrying to decrease I2S DMA buffer size from %d to maximum of 1024", _deviceIndex, _i2s_dma_buffer_frame_size);
      setDMABufferFrameSize(1024);
    }else{ // install failed with max buffer size
      log_e("(I2S#%d) ERROR i2s driver install failed", _deviceIndex);
      return 0; // ERR
    }
  } //try installing with increasing size

  if(_mode == PDM_MONO_MODE){ // mono/single channel
    // Set the clock for MONO. Stereo is not supported yet.
    if(ESP_OK != esp_i2s::i2s_set_clk((esp_i2s::i2s_port_t) _deviceIndex, _sampleRate, (esp_i2s::i2s_bits_per_sample_t)_bitsPerSample, esp_i2s::I2S_CHANNEL_MONO)){
      log_e("(I2S#%d) Setting the I2S Clock has failed!", _deviceIndex);
      return 0; // ERR
    }
  } // mono channel mode

#if (SOC_I2S_SUPPORTS_ADC && SOC_I2S_SUPPORTS_DAC)
  if(_mode == ADC_DAC_MODE){
    esp_i2s::i2s_set_dac_mode(esp_i2s::I2S_DAC_CHANNEL_BOTH_EN);
    esp_i2s::adc_unit_t adc_unit;
    if(!_gpioToAdcUnit((gpio_num_t)_inSdPin, &adc_unit)){
      log_e("(I2S#%d) pin to adc unit conversion failed", _deviceIndex);
      return 0; // ERR
    }
    esp_i2s::adc_channel_t adc_channel;
    if(!_gpioToAdcChannel((gpio_num_t)_inSdPin, &adc_channel)){
      log_e("(I2S#%d) pin to adc channel conversion failed", _deviceIndex);
      return 0; // ERR
    }
    if(ESP_OK != esp_i2s::i2s_set_adc_mode(adc_unit, (esp_i2s::adc1_channel_t)adc_channel)){
      log_e("(I2S#%d) i2s_set_adc_mode failed", _deviceIndex);
      return 0; // ERR
    }
    if(ESP_OK != esp_i2s::i2s_set_pin((esp_i2s::i2s_port_t) _deviceIndex, NULL)){
      log_e("(I2S#%d) i2s_set_pin failed", _deviceIndex);
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
      log_e("(I2S#%d) could not apply pin setting during driver install", _deviceIndex);
      _uninstallDriver();
      return 0; // ERR
    }
  } // if I2S _mode
  return 1; // OK
}

// Init in MASTER mode: the SCK and FS pins are driven as outputs using the sample rate
int I2SClass::begin(int mode, int sampleRate, int bitsPerSample){
  if(!_take_mux()){ return 0; /* ERR */ }
  // master mode (driving clock and frame select pins - output)
  int ret = begin(mode, sampleRate, bitsPerSample, true);
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

// Init in SLAVE mode: the SCK and FS pins are inputs, other side controls sample rate
int I2SClass::begin(int mode, int bitsPerSample){
  if(!_take_mux()){ return 0; /* ERR */ }
  // slave mode (not driving clock and frame select pin - input)
  int ret = begin(mode, 96000, bitsPerSample, false);
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

// Core function
int I2SClass::begin(int mode, int sampleRate, int bitsPerSample, bool driveClock){
  if(!_take_mux()){ return 0; /* ERR */ }
  if(_initialized){
    log_e("(I2S#%d) ERROR: Object already initialized! Call I2S.end() to disable", _deviceIndex);
    if(!_give_mux()){ return 0; /* ERR */ }
    return 0; // ERR
  }
  _driveClock = driveClock;
  _mode = mode;
  _sampleRate = (uint32_t)sampleRate;

  _bitsPerSample = bitsPerSample;

  // There is work in progress on this library.
  if(_bitsPerSample == 16 && _sampleRate > 16000 && driveClock){
    log_w("(I2S#%d) The sample rate value %d is not officially supported - audio might be noisy.\nTry using sample rate below or equal to 16000", _deviceIndex, _sampleRate);
  }
  if(_bitsPerSample != 16){
    log_w("(I2S#%d) The %d bit-per-sample is not officially supported - audio quality might suffer.\nTry using 16bps, with sample rate below or equal 16000", _deviceIndex, _bitsPerSample);
  }
  if(_mode != I2S_PHILIPS_MODE){
    log_w("(I2S#%d) The mode %s is not officially supported - audio quality might suffer.\nAt the moment the only supported mode is I2S_PHILIPS_MODE", _deviceIndex, i2s_mode_text[_mode]);
  }

  if (_state != I2S_STATE_IDLE && _state != I2S_STATE_DUPLEX) {
    log_e("(I2S#%d) Error: unexpected _state (%d)", _deviceIndex, _state);
    if(!_give_mux()){ return 0; /* ERR */ }
    return 0; // ERR
  }

  switch (mode) {
    case I2S_PHILIPS_MODE:
    case I2S_RIGHT_JUSTIFIED_MODE:
    case I2S_LEFT_JUSTIFIED_MODE:
      break;

    #if (SOC_I2S_SUPPORTS_ADC && SOC_I2S_SUPPORTS_DAC)
      case ADC_DAC_MODE:
      break;
    #else
      log_e("(I2S#%d) ERROR: ADC/DAC is not supported on this SoC. Change mode or SoC", _deviceIndex);
     if(!_give_mux()){ return 0; /* ERR */ }
     return 0; // ERR
    #endif

#if defined(SOC_I2S_SUPPORTS_PDM_TX) || defined(SOC_I2S_SUPPORTS_PDM_RX)
    case PDM_STEREO_MODE:
    case PDM_MONO_MODE:
      break;
#else
    log_e("(I2S#%d) ERROR: PDM is not supported on this SoC. Change mode or SoC", _deviceIndex);
    if(!_give_mux()){ return 0; /* ERR */ }
    return 0; // ERR
#endif

    default: // invalid mode
      if(mode >= 0 && mode < MODE_MAX){
        log_e("(I2S#%d) ERROR: Mode '%s' is not supported on this SoC", _deviceIndex, i2s_mode_text[mode]);
      }else{
        log_e("(I2S#%d) ERROR: Mode %d is not recognized", _deviceIndex, mode);
      }
      if(!_give_mux()){ return 0; /* ERR */ }
      return 0; // ERR
  }
  if(!_installDriver()){
    log_e("(I2S#%d) ERROR: failed to install driver", _deviceIndex);
    end();
    if(!_give_mux()){ return 0; /* ERR */ }
    return 0; // ERR
  }

  _input_ring_buffer  = xRingbufferCreate(getRingBufferByteSize(), RINGBUF_TYPE_BYTEBUF);
  _output_ring_buffer = xRingbufferCreate(getRingBufferByteSize(), RINGBUF_TYPE_BYTEBUF);
  if(_input_ring_buffer == NULL || _output_ring_buffer == NULL){
    log_e("(I2S#%d) ERROR: could not create one or both internal buffers. Requested size = %d", _deviceIndex, getRingBufferByteSize());
    if(!_give_mux()){ return 0; /* ERR */ }
    return 0; // ERR
  }

  if(!_createCallbackTask()){
    log_e("(I2S#%d) ERROR: failed to create callback task", _deviceIndex);
    end();
    if(!_give_mux()){ return 0; /* ERR */ }
    return 0; // ERR
  }
  _initialized = true;
  if(!_give_mux()){ return 0; /* ERR */ }
  return 1; // OK
}

int I2SClass::_applyPinSetting(){
  if(_driverInstalled){
    esp_i2s::i2s_pin_config_t pin_config = {
      .mck_io_num = _mclkPin,
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
      log_e("(I2S#%d) i2s_set_pin failed; attempted settings: SCK=%d; FS=%d; DIN=%d; DOUT=%d", _deviceIndex, pin_config.bck_io_num, pin_config.ws_io_num, pin_config.data_in_num, pin_config.data_out_num);
      return 0; // ERR
    }else{
      return 1; // OK
    }
  } // if(_driverInstalled)
  return 1; // OK
}

void I2SClass::_setMclkPin(int mclkPin){
  if(!_take_mux()){ return; /* ERR */ }
  if(mclkPin >= 0){
    _mclkPin = mclkPin;
  }else{
    _mclkPin = I2S_PIN_NO_CHANGE;
  }
  if(!_give_mux()){ return; /* ERR */ }
}

void I2SClass::_setSckPin(int sckPin){
  if(!_take_mux()){ return; /* ERR */ }
  if(sckPin >= 0){
    _sckPin = sckPin;
  }else{
    _sckPin = PIN_I2S_SCK;
    #if SOC_I2S_NUM > 1
      if(_deviceIndex==1){_sckPin = PIN_I2S1_SCK;}
    #endif
  }
  if(!_give_mux()){ return; /* ERR */ }
}

void I2SClass::_setFsPin(int fsPin){
  if(!_take_mux()){ return; /* ERR */ }
  if(fsPin >= 0){
    _fsPin = fsPin;
  }else{
    _fsPin = PIN_I2S_FS;
    #if SOC_I2S_NUM > 1
      if(_deviceIndex==1){_sckPin = PIN_I2S1_FS;}
    #endif
  }
  if(!_give_mux()){ return; /* ERR */ }
}

// shared data pin for simplex
void I2SClass::_setDataPin(int sdPin){
  if(!_take_mux()){ return; /* ERR */ }
  if(sdPin >= 0){
    _sdPin = sdPin;
  }else{
    _sdPin = PIN_I2S_SD;
    #if SOC_I2S_NUM > 1
      if(_deviceIndex==1){_sckPin = PIN_I2S1_SD;}
    #endif
  }
  if(!_give_mux()){ return; /* ERR */ }
}

void I2SClass::_setDataInPin(int inSdPin){
  if(!_take_mux()){ return; /* ERR */ }
  if(inSdPin >= 0){
    _inSdPin = inSdPin;
  }else{
    _inSdPin = PIN_I2S_SD_IN;
    #if SOC_I2S_NUM > 1
      if(_deviceIndex==1){_sckPin = PIN_I2S1_SD_IN;}
    #endif
  }
  if(!_give_mux()){ return; /* ERR */ }
}

void I2SClass::_setDataOutPin(int outSdPin){
  if(!_take_mux()){ return; /* ERR */ }
  if(outSdPin >= 0){
    _outSdPin = outSdPin;
  }else{
    _outSdPin = PIN_I2S_SD_OUT;
    #if SOC_I2S_NUM > 1
      if(_deviceIndex==1){_sckPin = PIN_I2S1_SD_OUT;}
    #endif
  }
  if(!_give_mux()){ return; /* ERR */ }
}

int I2SClass::setSckPin(int sckPin){
  if(!_take_mux()){ return 0; /* ERR */ }
  _setSckPin(sckPin);
  int ret = _applyPinSetting();
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

int I2SClass::setFsPin(int fsPin){
  if(!_take_mux()){ return 0; /* ERR */ }
  _setFsPin(fsPin);
  int ret = _applyPinSetting();
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

// shared data pin for simplex
int I2SClass::setDataPin(int sdPin){
  if(!_take_mux()){ return 0; /* ERR */ }
  _setDataPin(sdPin);
  int ret = _applyPinSetting();
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

int I2SClass::setDataInPin(int inSdPin){
  if(!_take_mux()){ return 0; /* ERR */ }
  _setDataInPin(inSdPin);
  int ret = _applyPinSetting();
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

int I2SClass::setDataOutPin(int outSdPin){
  if(!_take_mux()){ return 0; /* ERR */ }
  _setDataOutPin(outSdPin);
  int ret = _applyPinSetting();
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

int I2SClass::setMclkPin(int mclkPin){
  if(!_take_mux()){ return 0; /* ERR */ }
  _setMclkPin(mclkPin);
  int ret = _applyPinSetting();
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

int I2SClass::setAllPins(){
  if(!_take_mux()){ return 0; /* ERR */ }
  int ret = 0;
  if(_deviceIndex==0){
     ret = setAllPins(PIN_I2S_SCK, PIN_I2S_FS, PIN_I2S_SD, PIN_I2S_SD_OUT, PIN_I2S_SD_IN, I2S_PIN_NO_CHANGE);
  }
  #if SOC_I2S_NUM > 1
    if(_deviceIndex==1){
      ret = setAllPins(PIN_I2S1_SCK, PIN_I2S1_FS, PIN_I2S1_SD, PIN_I2S1_SD_OUT, PIN_I2S1_SD_IN, I2S_PIN_NO_CHANGE);
    }
  #endif
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

int I2SClass::setAllPins(int sckPin, int fsPin, int sdPin, int outSdPin, int inSdPin, int mclkPin /*=-1*/ ){
  if(!_take_mux()){ return 0; /* ERR */ }
  _setFsPin(fsPin);
  _setDataPin(sdPin);
  _setDataOutPin(outSdPin);
  _setDataInPin(inSdPin);
  _setMclkPin(mclkPin);
  int ret = _applyPinSetting();
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

int I2SClass::unSetMclkPin(){
  if(!_take_mux()){ return 0; /* ERR */ }
  _setMclkPin(I2S_PIN_NO_CHANGE);
  int ret = _applyPinSetting();
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

int I2SClass::getSckPin(){
  if(!_take_mux()){ return 0; /* ERR */ }
  int ret = _sckPin;
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

int I2SClass::getFsPin(){
  if(!_take_mux()){ return 0; /* ERR */ }
  int ret = _fsPin;
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

int I2SClass::getDataPin(){
  if(!_take_mux()){ return 0; /* ERR */ }
  int ret = _sdPin;
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

int I2SClass::getDataInPin(){
  if(!_take_mux()){ return 0; /* ERR */ }
  int ret = _inSdPin;
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

int I2SClass::getDataOutPin(){
  if(!_take_mux()){ return 0; /* ERR */ }
  int ret = _outSdPin;
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

int I2SClass::getMclkPin(){
  if(!_take_mux()){ return 0; /* ERR */ }
  int ret = _mclkPin;
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

int I2SClass::setDuplex(){
  if(!_take_mux()){ return 0; /* ERR */ }
  _state = I2S_STATE_DUPLEX;
  int ret = _applyPinSetting();
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

int I2SClass::setSimplex(){
  if(!_take_mux()){ return 0; /* ERR */ }
  _state = I2S_STATE_IDLE;
  int ret = _applyPinSetting();
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

int I2SClass::isDuplex(){
  if(!_take_mux()){ return 0; /* ERR */ }
  int ret = (int)(_state == I2S_STATE_DUPLEX);
  if(!_give_mux()){ return 0; /* ERR */ }
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
  if(!_take_mux()){ return; /* ERR */ }
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
    log_w("(I2S#%d) WARNING: ending I2SClass from callback task not permitted, but attempted!", _deviceIndex);
  }
  if(!_give_mux()){ return; /* ERR */ }
}

// Bytes available to read
int I2SClass::available(){
  if(!_take_mux()){ return 0; /* ERR */ }
  int ret = 0;
  if(_input_ring_buffer != NULL){
    ret = getRingBufferByteSize() - (int)xRingbufferGetCurFreeSize(_input_ring_buffer);
  }
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

union i2s_sample_t {
  int8_t b8;
  int16_t b16;
  int32_t b32;
};

int I2SClass::read(){
  if(!_take_mux()){ return 0; /* ERR */ }
  i2s_sample_t sample;
  sample.b32 = 0;
  if(_initialized){
    read(&sample, _bitsPerSample / 8);

    if (_bitsPerSample == 32) {
      if(!_give_mux()){ return 0; /* ERR */ }
      return sample.b32;
    } else if (_bitsPerSample == 24) {
      if(!_give_mux()){ return 0; /* ERR */ }
      return sample.b32;
    } else if (_bitsPerSample == 16) {
      if(!_give_mux()){ return 0; /* ERR */ }
      return sample.b16;
    } else if (_bitsPerSample == 8) {
      if(!_give_mux()){ return 0; /* ERR */ }
      return sample.b8;
    } else {
      if(!_give_mux()){ return 0; /* ERR */ }
      return 0; // sample value
    }
  } // if(_initialized)
  if(!_give_mux()){ return 0; /* ERR */ }
  return 0; // sample value
}

int I2SClass::read(void* buffer, size_t size){
  if(!_take_mux()){ return 0; /* ERR */ }
  size_t requested_size = size;
  if(_initialized){
    if(!_enableReceiver()){
      if(!_give_mux()){ return 0; /* ERR */ }
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
      tmp_buffer = xRingbufferReceiveUpTo(_input_ring_buffer, &item_size, pdMS_TO_TICKS(0), requested_size);
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
        if(!_give_mux()){ return 0; /* ERR */ }
        return item_size;
      }else{
        log_w("(I2S#%d) input buffer is empty - timed out", _deviceIndex);
        if(!_give_mux()){ return 0; /* ERR */ }
        return 0; // 0 Bytes read / ERR
      } // tmp buffer not NULL ?
    } // ring buffer not NULL ?
  } // if(_initialized)
  if(!_give_mux()){ return 0; /* ERR */ }
  return 0; // 0 Bytes read / ERR
}

size_t I2SClass::write(uint8_t data){
  if(!_take_mux()){ return 0; /* ERR */ }
  size_t ret = 0;
  if(_initialized){
    ret = write_blocking((int32_t*)&data, 1);
  }
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

size_t I2SClass::write(int32_t sample){
  if(!_take_mux()){ return 0; /* ERR */ }
  size_t ret = 0;
  if(_initialized){
    ret = write_blocking(&sample, _bitsPerSample/8);
  }
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

size_t I2SClass::write(const uint8_t *buffer, size_t size){
  if(!_take_mux()){ return 0; /* ERR */ }
  size_t ret = 0;
  if(_initialized){
    ret = write((const void*)buffer, size);
  }
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

size_t I2SClass::write(const void *buffer, size_t size){
  if(!_take_mux()){ return 0; /* ERR */ }
  size_t ret = 0;
  if(_initialized){
    //size_t ret = write_blocking(buffer, size);
    ret = write_nonblocking(buffer, size);
  } // if(_initialized)
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

// blocking version of write
// This version of write will wait indefinitely to write requested samples
// into output buffer
size_t I2SClass::write_blocking(const void *buffer, size_t size){
  if(!_take_mux()){ return 0; /* ERR */ }
  if(_initialized){
    if(!_enableTransmitter()){
      if(!_give_mux()){ return 0; /* ERR */ }
      return 0; // There was an error switching to transmitter
    } // _enableTransmitter succeeded ?

    if(_output_ring_buffer != NULL){
      int ret = xRingbufferSend(_output_ring_buffer, buffer, size, portMAX_DELAY);
      if(pdTRUE == ret){
        if(!_give_mux()){ return 0; /* ERR */ }
        return size;
      }else{
        log_e("(I2S#%d) xRingbufferSend() with infinite wait returned with error", _deviceIndex);
        if(!_give_mux()){ return 0; /* ERR */ }
        return 0;
      } // ring buffer send ok ?
    } // ring buffer not NULL ?
  } // if(_initialized)
  return 0;
  log_w("(I2S#%d) I2S not initialized", _deviceIndex);
  if(!_give_mux()){ return 0; /* ERR */ }
  return 0;
}

// non-blocking version of write
// In case there is not enough space in buffer to write requested size
// this function will try to flush the buffer and write requested data with 0 time-out
// The function will return number of successfully written bytes. It is users responsibility
// to take care of the remaining data.
size_t I2SClass::write_nonblocking(const void *buffer, size_t size){
  if(!_take_mux()){ return 0; /* ERR */ }
  if(_initialized){
    if(_state != I2S_STATE_TRANSMITTER && _state != I2S_STATE_DUPLEX){
      if(!_enableTransmitter()){
        if(!_give_mux()){ return 0; /* ERR */ }
        return 0; // There was an error switching to transmitter
      }
    }
    if(availableForWrite() < size){
      flush();
    }
    if(_output_ring_buffer != NULL){
      if(pdTRUE == xRingbufferSend(_output_ring_buffer, buffer, size, 0)){
        if(!_give_mux()){ return 0; /* ERR */ }
        return size;
      }else{
        log_w("(I2S#%d) I2S could not write all data (%d B) into ring buffer!", _deviceIndex, size);
        if(!_give_mux()){ return 0; /* ERR */ }
        return 0;
      }
    }
  } // if(_initialized)
  if(!_give_mux()){ return 0; /* ERR */ } // this should not be needed
  return 0;
}

/*
  Read 1 sample from internal buffer and return it.
  Repeated peeks will return the same sample until read is called.
*/
int I2SClass::peek(){
  if(!_take_mux()){ return 0; /* ERR */ }
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
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}
// Requests data from ring buffer and writes data to I2S module
// note: it is NOT necessary to call the flush after writes. Buffer is flushed automatically after receiveing enough data.
void I2SClass::flush(){
  if(!_take_mux()){ return; /* ERR */ }
  if(_initialized){
    const size_t single_dma_buf_byte_size = _i2s_dma_buffer_frame_size*(_bitsPerSample/8)*2;
    size_t item_size = 0;
    void *item = NULL;
    if(_output_ring_buffer != NULL){
      // TODO while available data to fill entire DMA buff - keep flushing
      item = xRingbufferReceiveUpTo(_output_ring_buffer, &item_size, 0, single_dma_buf_byte_size);
      if (item != NULL){
        _fix_and_write(item, item_size);
        vRingbufferReturnItem(_output_ring_buffer, item);
      }
    }
  } // if(_initialized)
  if(!_give_mux()){ return; /* ERR */ }
}

// Bytes available to write
int I2SClass::availableForWrite(){
  int ret = 0;
  if(!_take_mux()){ return 0; /* ERR */ }
  if(_initialized){
    if(_output_ring_buffer != NULL){
      ret = (int)xRingbufferGetCurFreeSize(_output_ring_buffer);
    }
  } // if(_initialized)
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

int I2SClass::availableSamplesForWrite(){
  int ret = 0;
  if(!_take_mux()){ return 0; /* ERR */ }
    ret =  availableForWrite() / (_bitsPerSample/8);
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

void I2SClass::onTransmit(void(*function)(void)){
  if(!_take_mux()){ return; /* ERR */ }
  _onTransmit = function;
  if(!_give_mux()){ return; /* ERR */ }
}

void I2SClass::onReceive(void(*function)(void)){
  if(!_take_mux()){ return; /* ERR */ }
  _onReceive = function;
  if(!_give_mux()){ return; /* ERR */ }
}


// Change buffer size. The unit is in frames.
// Byte value can be calculated as follows:
// ByteSize = (bits_per_sample / 8) * CHANNEL_NUMBER * DMABufferFrameSize
// Note: CHANNEL_NUMBER is hard-coded to value 2
// Calling this function will automatically restart the driver, which could cause audio output gap.
int I2SClass::setDMABufferFrameSize(int DMABufferFrameSize){
  if(!_take_mux()){ return 0; /* ERR */ }
  int ret = 0;
  if(DMABufferFrameSize >= 8 && DMABufferFrameSize <= 1024){
    _i2s_dma_buffer_frame_size = DMABufferFrameSize;
  }else{
    log_e("(I2S#%d) setDMABufferFrameSize: wrong input! Buffer size must be between 8 and 1024. Requested %d", _deviceIndex, DMABufferFrameSize);
    if(!_give_mux()){ return 0; /* ERR */ }
    return 0; // ERR
  } // check requested buffer size

  if(_initialized){
    _uninstallDriver();
    ret = _installDriver();
    if(!_give_mux()){ return 0; /* ERR */ }
    return ret;
  }else{ // check requested buffer size
    if(!_give_mux()){ return 0; /* ERR */ }
    return 1; // It's ok to change buffer size for uninitialized driver - new size will be used on begin()
  } // if(_initialized)
  if(!_give_mux()){ return 0; /* ERR */ }
  return 0; // ERR
}

// Change buffer size. The unit is in samples.
// Byte value can be calculated as follows:
// ByteSize = CHANNEL_NUMBER * DMABufferSampleSize
// Note: CHANNEL_NUMBER is hard-coded to value 2
// Calling this function will automatically restart the driver, which could cause audio output gap.
int I2SClass::setDMABufferSampleSize(int DMABufferSampleSize){
  return setDMABufferFrameSize(DMABufferSampleSize / CHANNEL_NUMBER);
}

int I2SClass::getDMABufferFrameSize(){
  if(!_take_mux()){ return 0; /* ERR */ }
  int ret = _i2s_dma_buffer_frame_size;
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

int I2SClass::getDMABufferSampleSize(){
  if(!_take_mux()){ return 0; /* ERR */ }
  int ret = _i2s_dma_buffer_frame_size * CHANNEL_NUMBER;
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

int I2SClass::getDMABufferByteSize(){
  if(!_take_mux()){ return 0; /* ERR */ }
  int ret = _i2s_dma_buffer_frame_size * CHANNEL_NUMBER * (_bitsPerSample/8);
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

int I2SClass::getRingBufferSampleSize(){
  if(!_take_mux()){ return 0; /* ERR */ }
  int ret = _i2s_dma_buffer_frame_size * CHANNEL_NUMBER * _I2S_DMA_BUFFER_COUNT;
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

int I2SClass::getRingBufferByteSize(){
  if(!_take_mux()){ return 0; /* ERR */ }
  int ret = _i2s_dma_buffer_frame_size * CHANNEL_NUMBER * (_bitsPerSample/8) * _I2S_DMA_BUFFER_COUNT;
  if(!_give_mux()){ return 0; /* ERR */ }
  return ret;
}

int I2SClass::getI2SNum(){
  return _deviceIndex;
}

bool I2SClass::isInitialized(){
  return _initialized;
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

  if(_output_ring_buffer != NULL && (getRingBufferByteSize() - xRingbufferGetCurFreeSize(_output_ring_buffer) >= getDMABufferByteSize())){ // fill up the I2S DMA buffer
    bytes_written = 0;
    item_size = 0;
    item = xRingbufferReceiveUpTo(_output_ring_buffer, &item_size, pdMS_TO_TICKS(0), getDMABufferByteSize());
    if(item != NULL){
      _fix_and_write(item, item_size, &bytes_written);
      if(item_size != bytes_written){ // save item that was not written correctly for later
        memcpy(prev_item, (void*)&((uint8_t*)item)[bytes_written], item_size-bytes_written);
        prev_item_size = item_size - bytes_written;
        prev_item_offset = 0;
        prev_item_valid = true;
      } // save item that was not written correctly for later
      vRingbufferReturnItem(_output_ring_buffer, item);
    } // Check received item
  } // fill up the I2S DMA buffer
  if(_onTransmit){
    _onTransmit();
  } // user callback
}

void I2SClass::_rx_done_routine(){
  size_t bytes_read = 0;
  size_t request_read = 0;

  if(_input_ring_buffer != NULL){
    uint8_t *_inputBuffer = (uint8_t*)malloc(getDMABufferByteSize());
    size_t avail = xRingbufferGetCurFreeSize(_input_ring_buffer);
    if(avail > 0){
      if(_bitsPerSample == 8){ // 8-bps is received with padding as 16bits - read twice than actually needed
        request_read = avail*2 <= getDMABufferByteSize() ? avail*2 : getDMABufferByteSize();
      }else{
        request_read = avail <= getDMABufferByteSize() ? avail : getDMABufferByteSize();
      }

      esp_err_t ret = esp_i2s::i2s_read((esp_i2s::i2s_port_t) _deviceIndex, _inputBuffer, request_read, &bytes_read, 0);
      if(ret != ESP_OK){
        log_w("(I2S#%d) i2s_read returned with error %d", _deviceIndex, ret);
      }
      if(request_read != bytes_read){
        log_w("(I2S#%d) i2s_read read only %d bytes instead of %d requested", _deviceIndex, bytes_read, request_read);
      }
      _post_read_data_fix(_inputBuffer, &bytes_read);
    }

    if(bytes_read > 0){ // when read more than 0, then send to ring buffer
      if(pdTRUE != xRingbufferSend(_input_ring_buffer, _inputBuffer, bytes_read, 0)){
        log_w("(I2S#%d) I2S failed to send item from DMA to internal buffer\n", _deviceIndex);
      } // xRingbufferSendComplete
    } // if(bytes_read > 0)
    free(_inputBuffer);
    if (_onReceive && avail < getRingBufferByteSize()){ // when user callback is registered && and there is some data in ring buffer to read
      _onReceive();
    } // user callback
  }
}

void I2SClass::_onTransferComplete(){
  uint8_t prev_item[_i2s_dma_buffer_frame_size*4];
  esp_i2s::i2s_event_t i2s_event;

  if(_i2sEventQueue == NULL){
    log_e("(I2S#%d) Event Queue is NULL!", _deviceIndex);
  }else{
    while(true){
      xQueueReceive(_i2sEventQueue, &i2s_event, portMAX_DELAY);
      if(i2s_event.type == esp_i2s::I2S_EVENT_TX_DONE){
        _tx_done_routine(prev_item);
      }else if(i2s_event.type == esp_i2s::I2S_EVENT_RX_DONE){
        _rx_done_routine();
      } // RX Done
    } // infinite loop
  } // _i2sEventQueue NULL check
}

void I2SClass::onDmaTransferComplete(void *deviceIndex){
  uint8_t *index;
  index = (uint8_t*) deviceIndex;
  if(*index == 0){
    I2S._onTransferComplete();
  }
#if SOC_I2S_NUM > 1
  if(*index == 1){
    I2S_1._onTransferComplete();
  }
#endif
  log_w("(I2S#%d) Deleting callback task from inside!", *index);
  vTaskDelete(NULL);
}

inline bool I2SClass::_take_mux(){
  if(_i2s_general_mutex == NULL || xSemaphoreTakeRecursive(_i2s_general_mutex, portMAX_DELAY) != pdTRUE){
    log_e("(I2S#%d) Could not take semaphore %p", _deviceIndex, _i2s_general_mutex);
    return false;
  }
  return true;
}

inline bool I2SClass::_give_mux(){
  if(_i2s_general_mutex == NULL || xSemaphoreGiveRecursive(_i2s_general_mutex) != pdTRUE){
    log_e("(I2S#%d) Could not give semaphore %p", _deviceIndex, _i2s_general_mutex);
    return false;
  }
  return true;
}

// Fixes data in-situ received from esp i2s driver. After fixing they reflect what was on the bus.
// input - bytes as received from i2s_read - this serves as input and output buffer
// size - number of bytes (this may be changed during operation)
// In 8 bit mode data are received as 16 bit number with LSB padded with 0s and actual data on MSB,
//   this function removes the padding (shrinking the size to 1/2)
// In 16 bit mode data are received with wrong endianity, this function swaps the bytes
// In 24 bit mode - not tested if needs fixing.
// In 32 bit mode data are ok - no action is performed.
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
    case 24:
        // TODO check if need fix and if so, implement it
      break;
      default: ;
        // Do nothing
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
  uint32_t offset = 1; // 24bit specific
  switch(_bitsPerSample){
    case 8:
      buff_size = size *2;
      buff = (uint8_t*)calloc(buff_size, sizeof(uint8_t));
      if(buff == NULL){
        log_e("(I2S#%d) callock error", _deviceIndex);
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
        log_e("(I2S#%d) malloc error", _deviceIndex);
        if(bytes_written != NULL){ *bytes_written = 0; }
        return;
      }
      for(int i = 0; i < size/2; i += 2 ){
        ((uint16_t*)buff)[i]   = ((uint16_t*)output)[i+1]; // [1] <- [0]
        ((uint16_t*)buff)[i+1] = ((uint16_t*)output)[i]; // [0] <- [1]
      }
    break;
    case 24:
      buff_size = (size/3)*4; // Increase by 1/3
      buff = (uint8_t*)calloc(buff_size, sizeof(uint8_t));
      for(int i = 0; i < size/3; i+=3){
        // LSB in buffer's 4-Byte Word is 0 to compensate IDF driver behavior
        buff[i+offset] = ((uint8_t*)output)[i];
        buff[i+offset+1] = ((uint8_t*)output)[i+1];
        buff[i+offset+2] = ((uint8_t*)output)[i+2];
        ++offset;
      }
      break;
    case 32:
      buff = (uint8_t*)output;
      break;
    default:
      break; // Do nothing
  } // switch

/*
// Should be taken care of by comm format: I2S_CHANNEL_FMT_ALL_RIGHT / I2S_CHANNEL_FMT_ALL_LEFT
    if(_mode == I2S_RIGHT_JUSTIFIED_MODE){
      for(int i = 0; i < buff_size; i+=2){
        buff[i] = buff[i+1];
      }
    }
    if(_mode == I2S_LEFT_JUSTIFIED_MODE){
      for(int i = 0; i < buff_size; i+=2){
        buff[i+1] = buff[i];
      }
    }
*/

  size_t _bytes_written;
  esp_err_t ret = esp_i2s::i2s_write((esp_i2s::i2s_port_t) _deviceIndex, buff, buff_size, &_bytes_written, 0);
  if(ret != ESP_OK){
    log_e("(I2S#%d) Error: writing data to i2s - function returned with err code %d", _deviceIndex, ret);
  }
  if(ret == ESP_OK && buff_size != _bytes_written){
    log_w("(I2S#%d) Warning: writing data to i2s - written %d B instead of requested %d B", _deviceIndex, _bytes_written, buff_size);
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
      log_w("(I2S#%d) GPIO 0 for ADC should not be used for dev boards due to external auto program circuits.", _deviceIndex);
      // Only to suppress Warnings "may fall through" which exactly what is intended
      *adc_unit = esp_i2s::ADC_UNIT_2;
      return 1; // OK
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
      log_e("(I2S#%d) GPIO %d not usable for ADC!", _deviceIndex, gpio_num);
      #if CONFIG_IDF_TARGET_ESP32
        log_i("Please refer to documentation https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html");
      #elif
         CONFIG_IDF_TARGET_ESP32S2
        log_i("Please refer to documentation https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/api-reference/peripherals/gpio.html");
      #elif
         CONFIG_IDF_TARGET_ESP32S3
        log_i("Please refer to documentation https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/gpio.html");
      #elif
         CONFIG_IDF_TARGET_ESP32C2
        log_i("Please refer to documentation https://docs.espressif.com/projects/esp-idf/en/latest/esp32c2/api-reference/peripherals/gpio.html");
      #elif
         CONFIG_IDF_TARGET_ESP32C3
        log_i("Please refer to documentation https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/gpio.html");
      #endif
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
      log_w("(I2S#%d) GPIO 0 for ADC should not be used for dev boards due to external auto program circuits.", _deviceIndex);
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
      log_e("(I2S#%d) GPIO %d not usable for ADC!", _deviceIndex, gpio_num);
      log_i("Please refer to documentation https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html");
      return 0; // ERR
  }
}
#endif // SOC_I2S_SUPPORTS_ADC_DAC

#if SOC_I2S_NUM > 0
  I2SClass I2S(0, I2S_CLOCK_GENERATOR, PIN_I2S_SD, PIN_I2S_SCK, PIN_I2S_FS); // default - half duplex
#endif

#if SOC_I2S_NUM > 1
  I2SClass I2S_1(1, I2S_CLOCK_GENERATOR, PIN_I2S1_SD, PIN_I2S1_SCK, PIN_I2S1_FS); // default - half duplex
#endif
