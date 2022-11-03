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
// 19 18  22      23    ESP32
// 19 18   4       5    ESP32-x (C3,S2,S3)
#ifndef PIN_I2S_SCK
  #define PIN_I2S_SCK GPIO_NUM_19
#endif

#ifndef PIN_I2S_FS
  #define PIN_I2S_FS GPIO_NUM_18
#endif

#ifndef PIN_I2S_SD
  #if CONFIG_IDF_TARGET_ESP32
    #define PIN_I2S_SD GPIO_NUM_22
  #else
    #define PIN_I2S_SD GPIO_NUM_4
  #endif
#endif

#ifndef PIN_I2S_SD_OUT
  #if CONFIG_IDF_TARGET_ESP32
    #define PIN_I2S_SD_OUT GPIO_NUM_22
  #else
    #define PIN_I2S_SD_OUT GPIO_NUM_4
  #endif
#endif

#ifndef PIN_I2S_SD_IN
    #if CONFIG_IDF_TARGET_ESP32
    #define PIN_I2S_SD_IN GPIO_NUM_23
  #else
    #define PIN_I2S_SD_IN GPIO_NUM_5
  #endif
#endif

#if SOC_I2S_NUM > 1
  // I2S1 is available only for ESP32 and ESP32-S3
  #if CONFIG_IDF_TARGET_ESP32
    // ESP32 pins
    //SCK WS  SD(OUT) SDIN
    // 18 22  25       26
    #ifndef PIN_I2S1_SCK
      #define PIN_I2S1_SCK GPIO_NUM_18
    #endif

    #ifndef PIN_I2S1_FS
      #define PIN_I2S1_FS GPIO_NUM_22
    #endif

    #ifndef PIN_I2S1_SD
      #define PIN_I2S1_SD GPIO_NUM_25
    #endif

    #ifndef PIN_I2S1_SD_OUT
      #define PIN_I2S1_SD_OUT GPIO_NUM_25
    #endif

    #ifndef PIN_I2S1_SD_IN
      #define PIN_I2S1_SD_IN GPIO_NUM_26
    #endif
  #endif

  #if CONFIG_IDF_TARGET_ESP32S3
    // ESP32-S3 pins
    //SCK WS  SD(OUT) SDIN
    // 36 37   39       40
    #ifndef PIN_I2S1_SCK
      #define PIN_I2S1_SCK GPIO_NUM_36
    #endif

    #ifndef PIN_I2S1_FS
      #define PIN_I2S1_FS GPIO_NUM_37
    #endif

    #ifndef PIN_I2S1_SD
      #define PIN_I2S1_SD GPIO_NUM_39
    #endif

    #ifndef PIN_I2S1_SD_OUT
      #define PIN_I2S1_SD_OUT GPIO_NUM_39
    #endif

    #ifndef PIN_I2S1_SD_IN
      #define PIN_I2S1_SD_IN GPIO_NUM_40
    #endif
  #endif
#endif

#define CHANNEL_NUMBER 2

typedef enum {
  I2S_PHILIPS_MODE, // Most common I2S mode, FS signal spans across whole channel period
  I2S_RIGHT_JUSTIFIED_MODE, // Right channel data are copied to both channels
  I2S_LEFT_JUSTIFIED_MODE, // Left channel data are copied to both channels
  ADC_DAC_MODE, // Receive and transmit raw analog signal
  PDM_STEREO_MODE, // Pulse Density Modulation - stereo / 2 channels
  PDM_MONO_MODE, // Pulse Density Modulation - mono / 1 channel
  MODE_MAX // Helper value - not actual mode
} i2s_mode_t;

const char i2s_mode_text[][32] = {
  "I2S_PHILIPS_MODE",
  "I2S_RIGHT_JUSTIFIED_MODE",
  "I2S_LEFT_JUSTIFIED_MODE",
  "ADC_DAC_MODE",
  "PDM_STEREO_MODE",
  "PDM_MONO_MODE"
};

class I2SClass : public Stream
{
public:
  /*
   * Constructor - initializes object with default values
   *
   * Parameters:
   *   uint8_t deviceIndex  In case the SoC has more I2S modules, specify which one is instantiated. Possible values are "0" (for all ESPs) and "1" (only for ESP32 and ESP32-S3)
   *   uint8_t clockGenerator  Has no meaning for ESP and is kept only for compatibility
   *   uint8_t sdPin  Shared data pin used for simplex mode
   *   uint8_t sckPin  Clock pin
   *   uint8_t fsPin  Frame Sync (Word Select) pin
   *
   * Default settings:
   *   Input data pin (used for duplex mode) is initialized with PIN_I2S_SD_IN
   *   Out data pin (used for duplex mode) is initialized with PIN_I2S_SD
   *   Mode = I2S_PHILIPS_MODE
   *   Buffer size = 128
   */
  I2SClass(uint8_t deviceIndex, uint8_t clockGenerator, uint8_t sdPin, uint8_t sckPin, uint8_t fsPin);

  /*
   * Init in MASTER mode: the SCK and FS pins are driven as outputs using the sample rate.
   * Initializes IDF I2S driver, creates ring buffers and callback handler task.
   * Parameters:
   *   int mode  Operation mode (Phillips, Left/Right Justified, ADC+DAC,PDM) see i2s_mode_t for exact enumerations
   *   int sampleRate  sampling frequency in Hz. Common values are 8000,11025,16000,22050,32000,44100,64000,88200,128000
   *   int bitsPerSample  Number of bits per one sample (one channel). Possible values are 8,16,24,32
   * Returns: 1 on success; 0 on error
  */
  int begin(int mode, int sampleRate, int bitsPerSample);

  /* Init in SLAVE mode: the SCK and FS pins are inputs and must be controlled(generated) be external source (MASTER device).
   * Initializes IDF I2S driver, creates ring buffers and callback handler task.
   * Parameters:
   *   int mode  Operation mode (Phillips, Left/Right Justified, ADC+DAC,PDM) see i2s_mode_t for exact enumerations
   *   int bitsPerSample  Number of bits per one sample (one channel). Possible values are 8,16,24,32
   * Returns: 1 on success; 0 on error
   */
  int begin(int mode, int bitsPerSample);

  /*
   De-initialize IDF I2S driver, frees ring buffers and terminates callback handler task.
   */
  void end();

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
   * Change mode (default is Simplex)
   * Returns: 1 on success; 0 on error
   */
  int setDuplex();
  int setSimplex();

  /*
   * Get current mode
   * Returns: 1 if current mode is Duplex; 0 If current mode is not Duplex
   */
  int isDuplex();

  /*
   * Returns: number of Bytes available to read from ring buffer.
   * Note: The ring buffer is filled automatically by handler task from IDF I2S driver.
   */
  virtual int available();

  /*
   * Reads a single sample from ring buffer and keeps it available for future read (i.e. does not remove the sample from ring buffer)
   * Returns: First sample from ring buffer
   * Note: The ring buffer is filled automatically by handler task from IDF I2S driver.
   */
  virtual int peek();

  /*
   * Reads a single sample from ring buffer and removes it from the ring buffer.
   * Returns: First sample from ring buffer
   * Note: The ring buffer is filled automatically by handler task from IDF I2S driver.
   */
  virtual int read();

  /*
   * Reads an array of samples from ring buffer and removes them from the ring buffer.
   * Parameters:
   *   [OUT] void* buffer  Buffer into which the samples will be copied. The buffer must allocated before calling this function!
   *   [IN] size_t size  Requested number of bytes to be read
   * Returns: Number of bytes that were actually read.
   * Note: Always check the returned value!
   */
  int read(void* buffer, size_t size);

  /*
   * Returns: number of bytes that can be written into the ring buffer.
   */
  virtual int availableForWrite();

  /*
   * Returns: number of samples that can be written into the ring buffer.
   */
  int availableSamplesForWrite();

  /*
   * Write single sample of 8 bit size.
   * This function is blocking - if there is not enough space in ring buffer the function will wait until it can write the sample.
   * Parameter:
   *   uint8_t data  The sample to be sent
   * Returns: 1 on successful write; 0 on error = did not write the sample to ring buffer
   * Note: This functions is used in many examples for it's simplicity, but it's use is discouraged for performance reasons.
   * Please consider sending data in arrays using function `size_t write(const uint8_t *buffer, size_t size)`
   */
  virtual size_t write(uint8_t data);

  /*
   * Write single sample of up to 32 bit size.
   * This function is blocking - if there is not enough space in ring buffer the function will wait until it can write the sample.
   * Parameter:
   *   int32_t data  The sample to be sent
   * Returns: Number of written bytes, if successful the value will be equal to bitsPerSample/8
   * Note: This functions is used in many examples for it's simplicity, but it's use is discouraged for performance reasons.
   * Please consider sending data in arrays using function `size_t write(const uint8_t *buffer, size_t size)`
   */
  size_t write(int32_t);

  /*
   * Write array of samples.
   * This function is non-blocking - the function might write only portion of samples into ring buffer, or potentially none at all. Do check the returned value at all times!
   * Parameters:
   *   uint8_t* buffer  Array of samples
   *   size_t size  Number of bytes in array
   * Returns: Number of bytes successfully written to ring buffer.
   * Note: This is the preferred function for writing samples.
   */
  virtual size_t write(const uint8_t *buffer, size_t size);

  /*
   * Write array of samples.
   * This function is non-blocking - the function might write only portion of samples into ring buffer, or potentially none at all. Do check the returned value at all times!
   * Parameters:
   *   void* buffer  Array of samples
   *   size_t size  Number of bytes in array
   * Returns: Number of bytes successfully written to ring buffer.
   * Note: This is the preferred function for writing samples.
   */
  size_t write(const void *buffer, size_t size);

  // Internally used functions
  size_t write_blocking(const void *buffer, size_t size);
  size_t write_nonblocking(const void *buffer, size_t size);

  /*
   * Force-write data from ring buffer to IDF I2S driver
   * This function is useful when sending low amount of data, however such use will lead to low quality audio.
   * Note: The ring buffer is emptied (sent) automatically by handler task from IDF I2S driver.
   */
  virtual void flush();

  /*
   * Callback handle which will be used each time when the IDF I2S driver transmits data from buffer.
   */
  void onTransmit(void(*)(void));

  /*
   * Callback handle which will be used each time when the IDF I2S driver receives data into buffer.
   */
  void onReceive(void(*)(void));

  /*
   * Change the size of DMA buffers. The unit is number of sample frames (CHANNEL_NUMBER * (bits_per_sample/8))
   * The resulting Bytes size of ring buffers can be calculated:
   * ring_buffer_bytes_size = (CHANNEL_NUMBER * (bits_per_sample/8)) * DMABufferFrameSize * _I2S_DMA_BUFFER_COUNT
   * Example:
   *  - This library statically set to dual channel, therefore CHANNEL_NUMBER is always 2
   *  - For this example let's have bits_per_sample set to 16
   *  - Default value of bufferSize is 128
   *  - Default value of _I2S_DMA_BUFFER_COUNT is 2
   * ring_buffer_bytes_size = (CHANNEL_NUMBER * (bits_per_sample / 8)) * DMABufferFrameSize * _I2S_DMA_BUFFER_COUNT
   *        1024            = (       2       * (     16         / 8)) *        128         *         2
   */
  int setDMABufferFrameSize(int DMABufferFrameSize);

  /*
   * Get size of single DMA buffer.
   * The unit is number of sample frames: Bytes size of 1 frame = (CHANNEL_NUMBER * (bits_per_sample / 8))
   * For more info see setDMABufferFrameSize
   */
  int getDMABufferFrameSize();

    /*
   * Get size of single DMA buffer. The unit is number of samples: Byte size of 1 sample = (bits_per_sample / 8)
   * For more info see setDMABufferFrameSize
   */
  int getDMABufferSampleSize();

  /*
   * Get size of single DMA buffer in Bytes.
   * For more info see setDMABufferFrameSize
   */
  int getDMABufferByteSize();

  /*
   * Get ring buffer size. The unit is number of samples: 1 sample = (bits_per_sample / 8)
   * For more info see setDMABufferFrameSize
   */
  int getRingBufferSampleSize();

  /*
   * Get ring buffer size in Bytes.
   * For more info see setDMABufferFrameSize
   */
  int getRingBufferByteSize();

  /*
    Get the ID number of I2S module used for particular object.
    Object I2S returns value 0
    Object I2S1 returns value 1
    Only ESP32 and ESP32-S3 have two modules, other SoCs have only one I2S module
    controlled by object I2S and the return value will always be 0, the second object I2S1 does not exist.
  */
  int getI2SNum();

  /*
   * Returns true if I2S module is correctly initialized and ready for use (function begin() was called and returned 1)
   * Returns false if I2S module has not yet been initialized (function begin() was called returned 1), or it has been de-initialized (function end() was called)
   */
  bool isInitialized();

  /*
   * Returns 0 on un-initialized object, or if the object is initialized as slave.
   * On initialized master object returns sample rate in Hz (same value which was passed as argument with begin() function)
  */
  int getSampleRate();

  /*
   * Returns 0 on un-initialized object.
   * On initialized master object returns bits per sample (same value which was passed as argument with begin() function)
   */
  int getBitsPerSample();

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
  int _i2s_dma_buffer_frame_size;
  bool _driveClock;
  uint32_t _peek_buff;
  bool _peek_buff_valid;
  void _tx_done_routine(uint8_t* prev_item);
  void _rx_done_routine();

  uint16_t _nesting_counter;
  inline bool _take_mux();
  inline bool _give_mux();
  void _post_read_data_fix(void *input, size_t *size);
  void _fix_and_write(void *output, size_t size, size_t *bytes_written = NULL, size_t *actual_bytes_written = NULL);

  void (*_onTransmit)(void);
  void (*_onReceive)(void);
};

extern I2SClass I2S;

#if SOC_I2S_NUM > 1
  extern I2SClass I2S_1;
#endif

#endif
