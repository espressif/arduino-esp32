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
//I2S0 is available for all ESP32 SoCs
//SCK WS  SD(OUT) SDIN
// 19 21  22      23    ESP32
// 19 21   4       5    ESP32-x (C3,S2,S3)
#ifndef PIN_I2S_SCK
  #define PIN_I2S_SCK 19
#endif

#ifndef PIN_I2S_FS
  #define PIN_I2S_FS 21
#endif

#ifndef PIN_I2S_SD
  #if CONFIG_IDF_TARGET_ESP32
    #define PIN_I2S_SD 22
  #else
    #define PIN_I2S_SD 4
  #endif
#endif

#ifndef PIN_I2S_SD_OUT
  #if CONFIG_IDF_TARGET_ESP32
    #define PIN_I2S_SD_OUT 22
  #else
    #define PIN_I2S_SD_OUT 4
  #endif
#endif

#ifndef PIN_I2S_SD_IN
    #if CONFIG_IDF_TARGET_ESP32
    #define PIN_I2S_SD_IN 23
  #else
    #define PIN_I2S_SD_IN 5
  #endif
#endif

#if SOC_I2S_NUM > 1
  // I2S1 is available only for ESP32 and ESP32-S3
  #if CONFIG_IDF_TARGET_ESP32
    // ESP32 pins
    //SCK WS  SD(OUT) SDIN
    // 18 22  25       26
    #ifndef PIN_I2S1_SCK
      #define PIN_I2S1_SCK 18
    #endif

    #ifndef PIN_I2S1_FS
      #define PIN_I2S1_FS 22
    #endif

    #ifndef PIN_I2S1_SD
      #define PIN_I2S1_SD 25
    #endif

    #ifndef PIN_I2S1_SD_OUT
      #define PIN_I2S1_SD_OUT 25
    #endif

    #ifndef PIN_I2S1_SD_IN
      #define PIN_I2S1_SD_IN 26
    #endif
  #endif

  #if CONFIG_IDF_TARGET_ESP32S3
    // ESP32-S3 pins
    //SCK WS  SD(OUT) SDIN
    // 36 37   39       40
    #ifndef PIN_I2S1_SCK
      #define PIN_I2S1_SCK 36
    #endif

    #ifndef PIN_I2S1_FS
      #define PIN_I2S1_FS 37
    #endif

    #ifndef PIN_I2S1_SD
      #define PIN_I2S1_SD 39
    #endif

    #ifndef PIN_I2S1_SD_OUT
      #define PIN_I2S1_SD_OUT 39
    #endif

    #ifndef PIN_I2S1_SD_IN
      #define PIN_I2S1_SD_IN 40
    #endif
  #endif
#endif

typedef enum {
  I2S_PHILIPS_MODE, // Most common I2S mode, WS signal spans across whole channel period
  I2S_RIGHT_JUSTIFIED_MODE, // TODO check oscilloscope what it does
  I2S_LEFT_JUSTIFIED_MODE, // TODO check oscilloscope what it does
  ADC_DAC_MODE, // Receive and transmit raw analog signal
  PDM_STEREO_MODE, // Pulse Density Modulation - stereo / 2 channels
  PDM_MONO_MODE // Pulse Density Modulation - mono / 1 channel
} i2s_mode_t;

class I2SClass : public Stream
{
public:
  /*
   * Constructor - initializes object with default values
   *
   * Parameters:
   *   uint8_t deviceIndex  In case the SoC has more I2S module, specify which one is instantiated. Possible values are "0" (for all ESPs) and "1" (only for ESP32 and ESP32-S3)
   *   uint8_t clockGenerator  Has no meaning for ESP and is kept only for compatibility
   *   uint8_t sdPin  Shared data pin used for simplex mode
   *   uint8_t sckPin  Clock pin
   *   uint8_t fsPin  Frame (word) select pin
   *
   * Default settings:
   *   Input data pin (used for duplex mode) is initialized with PIN_I2S_SD_IN
   *   Out data pin (used for duplex mode) is initialized with PIN_I2S_SD
   *   Mode = I2S_PHILIPS_MODE
   *   Buffer size = 128
   */
  I2SClass(uint8_t deviceIndex, uint8_t clockGenerator, uint8_t sdPin, uint8_t sckPin, uint8_t fsPin);

  /*
   * Init in MASTER mode: the SCK and FS pins are driven as outputs using the sample rate
   * Parameters:
   *   int mode  Operation mode (Phillips, Left/Right Justified, ADC+DAC,PDM) see i2s_mode_t for exact enumerations
   *   int sampleRate  sampling frequency in Hz. Common values are 8000,11025,16000,22050,32000,44100,64000,88200,128000
   *   int bitsPerSample  Number of bits per one sample (one channel). Possible values are 8,16,24,32
   * Returns: 1 on success; 0 on error
  */
  int begin(int mode, int sampleRate, int bitsPerSample);

  /* Init in SLAVE mode: the SCK and FS pins are inputs and must be controlled(generated) be external source (MASTER device).
   * Parameters:
   *   int mode  Operation mode (Phillips, Left/Right Justified, ADC+DAC,PDM) see i2s_mode_t for exact enumerations
   *   int bitsPerSample  Number of bits per one sample (one channel). Possible values are 8,16,24,32
   * Returns: 1 on success; 0 on error
   */
  int begin(int mode, int bitsPerSample);

  /*
   * Change pin setup for each pin separately.
   * Can be called only on initialized object (after begin).
   * The change takes effect immediately and does not need driver restart.
   * Parameter: int pin  number of GPIO which should be used for the requested pin setup
   * Returns: 1 on success; 0 on error
   */
  int setSckPin(int sckPin); // Set Clock pin
  int setFsPin(int fsPin); // Set Frame Sync (Word Select) pin
  int setDataPin(int sdPin); // Set shared Data pin for simplex mode
  int setDataOutPin(int outSdPin); // Set Data Output pin for duplex mode
  int setDataInPin(int inSdPin); // Set Data Input pin for duplex mode

  /*
   * Change pin setup for all pins at one call using default values set constants in I2S.h
   * Can be called only on initialized object (after begin)
   * The change takes effect immediately and does not need driver restart.
   * Returns: 1 on success; 0 on error
   */
  int setAllPins();

  /*
   * Change pin setup for all pins at one call.
   * Can be called only on initialized object (after begin).
   * The change takes effect immediately and does not need driver restart.
   * Parameters:
   *   int sckPin  Clock pin
   *   int fsPin  Frame Sync (Word Select) pin
   *   int sdPin  Shared Data pin for simplex mode
   *   int outSdPin  Data Output pin for duplex mode
   *   int inSdPin  Data Input pin for duplex mode
   * Returns: 1 on success; 0 on error
   */
  int setAllPins(int sckPin, int fsPin, int sdPin, int outSdPin, int inSdPin);

  /*
   * Get current pin GPIO number
   * Returns: the GPIO number of requested pin
   */
  int getSckPin(); // Get Clock pin
  int getFsPin(); // Get Frame Sync (Word Select) pin
  int getDataPin(); // Get shared Data pin for simplex mode
  int getDataOutPin(); // Get Data Output pin for duplex mode
  int getDataInPin(); // Get Data Input pin for duplex mode

  /*
   * Change mode (default is Half Duplex)
   * Returns: 1 on success; 0 on error
   */
  int setDuplex();
  int setSimplex();

  /*
   * Get current mode
   * Returns: 1 if current mode is Duplex; 0 If current mode is not Duplex
   */
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

  static void onDmaTransferComplete(void *device_index);
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
#if SOC_I2S_NUM > 1
  extern I2SClass I2S1;
#endif

#endif
