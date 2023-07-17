#include <unity.h>
#include <I2S.h>
#include "Arduino.h"

// Internal connections
#include "hal/gpio_hal.h"
#include "esp_rom_gpio.h"

I2SClass *I2S_obj;
I2SClass *I2S_obj_arr[SOC_I2S_NUM];
int i2s_index;

#define TEST_SCK 1
#define TEST_FS 2
#define TEST_SD 3
#define TEST_SD_OUT 4
#define TEST_SD_IN 5

// Supported sample rates (do not change)
int sample_rate[] = {8000,11025,16000,22050,32000,44100,64000,88200,128000};
int srp_max = sizeof(sample_rate)/sizeof(int);

// Supported bits per sample (do not change)
int bps[] = {8,16,24,32};
int bpsp_max = sizeof(bps)/sizeof(int);

//                                     Frame 1             | |              Frame 2             | |               Frame 3
//                            R channel  |       L channel | |     R channel  |       L channel | |      R channel  |        L channel
uint8_t  data_8bit[]  = {            0xFF,             0xAA,              0x55,             0x00,               0xCC,             0x33};
uint16_t data_16bit[] = {          0xFFFF,           0xAAAA,            0x5555,           0x0000,             0xCCCC,           0x3333};
uint8_t  data_24bit[] = {0xFF, 0xFF, 0xFF, 0xAA, 0xAA, 0xAA,  0x55, 0x55, 0x55, 0x00, 0x00, 0x00,   0xCC, 0xCC, 0xCC, 0x33, 0x33, 0x33};
uint32_t data_32bit[] = {      0xFFFFFFFF,       0xAAAAAAAA,        0x55555555,       0x00000000,         0xCCCCCCCC,       0x33333333};

/* These functions are intended to be called before and after each test. */
void setUp(void) {
  // use default pins
  I2S.setAllPins();
  #if SOC_I2S_NUM > 1
    I2S_1.setAllPins();
  #endif
}

void tearDown(void){
  I2S.end();
  #if SOC_I2S_NUM > 1
    I2S_1.end();
  #endif
}

// test which will always pass - this is used for chips wich do not have I2S module
void test_pass(void){
  TEST_ASSERT_EQUAL(1, 1); // always pass
}

// Test begin in master mode - all expected possibilities, then few unexpected option
void test_01(void){
  Serial.printf("[%lu] test_01: I2S_obj=%p\n", millis(), I2S_obj);
  int ret = -1;

  Serial.printf("*********** [%lu] Begin I2S with golden parameters **************\n", millis());
  for(int mode = 3; mode < MODE_MAX; ++mode){
  //for(int mode = 0; mode < MODE_MAX; ++mode){
    Serial.printf("[%lu] begin: mode %d \"%s\"\n", millis(), mode, i2s_mode_text[mode]);
    for(int srp = 0; srp < srp_max; ++srp){
      for(int bpsp = 0; bpsp < bpsp_max; ++bpsp){
        Serial.printf("[%lu] begin: mode %d \"%s\", sample rate %d, bps %d\n", millis(), mode, i2s_mode_text[mode], sample_rate[srp], bps[bpsp]);
        #if defined(SOC_I2S_SUPPORTS_ADC) && defined(SOC_I2S_SUPPORTS_DAC)
          if(mode == ADC_DAC_MODE && bps[bpsp] == 16){
            #if CONFIG_IDF_TARGET_ESP32
              I2S_obj->setDataInPin(32); // Default data pin does not support DAC
            #else
              I2S_obj->setDataInPin(4); // Default data pin does not support DAC
           #endif
         }
        #endif
        ret = I2S_obj->begin(mode, sample_rate[srp], bps[bpsp]);

        if(mode == ADC_DAC_MODE){
          #if defined(SOC_I2S_SUPPORTS_ADC) && defined(SOC_I2S_SUPPORTS_DAC)
            if(bps[bpsp] == 16){
              TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success
              TEST_ASSERT_EQUAL(true, I2S_obj->isInitialized()); // Expecting true == Success
              I2S_obj->setDataInPin(-1); // Set to default data pin
            }else{
              TEST_ASSERT_EQUAL(0, ret); // Expecting 0 == Failure
              TEST_ASSERT_EQUAL(false, I2S_obj->isInitialized()); // Expecting false == Failure
            }
          #else
            TEST_ASSERT_EQUAL(0, ret); // Expecting 0 == Failure
            TEST_ASSERT_EQUAL(false, I2S_obj->isInitialized()); // Expecting false == Failure
          #endif
        }else if(mode == PDM_STEREO_MODE || mode == PDM_MONO_MODE){
           #if defined(SOC_I2S_SUPPORTS_PDM_TX) || defined(SOC_I2S_SUPPORTS_PDM_RX)
            TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success
            TEST_ASSERT_EQUAL(true, I2S_obj->isInitialized()); // Expecting true == Success
          #else
            TEST_ASSERT_EQUAL(0, ret); // Expecting 0 == Failure
            TEST_ASSERT_EQUAL(false, I2S_obj->isInitialized()); // Expecting false == Failure
          #endif
        }else{
          TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success
          TEST_ASSERT_EQUAL(true, I2S_obj->isInitialized()); // Expecting true == Success
        }
        I2S_obj->end();
      }
    }
  }

  Serial.printf("*********** [%lu] Begin I2S with nonexistent mode **************\n", millis());
  // Nonexistent mode - should fail
  ret = I2S_obj->begin(MODE_MAX, 8000, 8);
  TEST_ASSERT_EQUAL(0, ret); // Expecting 0 == Failure
  TEST_ASSERT_EQUAL(false, I2S_obj->isInitialized()); // Expecting false == Failure
  I2S_obj->end();

  Serial.printf("*********** [%lu] Begin I2S with Unsupported Bits Per Sample **************\n", millis());
  // Unsupported Bits Per Sample - all should fail
  int unsupported_bps[] = {-1,0,1,7,9,15,17,23,25,31,33,255,65536};
  int ubpsp_max = sizeof(unsupported_bps)/sizeof(int);
  for(int ubpsp = 0; ubpsp < ubpsp_max; ++ubpsp){
    ret = I2S_obj->begin(I2S_PHILIPS_MODE, 8000, unsupported_bps[ubpsp]);
    TEST_ASSERT_EQUAL(0, ret); // Expecting 0 == Failure
    TEST_ASSERT_EQUAL(false, I2S_obj->isInitialized()); // Expecting false == Failure
    I2S_obj->end();
  }

  Serial.printf("*********** [%lu] Begin I2S with Unusual, yet possible sample rates **************\n", millis());
  // Unusual, yet possible sample rates - all should succeed
  int unusual_sample_rate[] = {4000,64000,88200,96000,128000};
  int usrp_max = sizeof(unusual_sample_rate)/sizeof(int);
  for(int usrp = 0; usrp < usrp_max; ++usrp){
    ret = I2S_obj->begin(I2S_PHILIPS_MODE, unusual_sample_rate[usrp], 32);
    TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success
    TEST_ASSERT_EQUAL(true, I2S_obj->isInitialized()); // Expecting true == Success
    I2S_obj->end();
  }
  Serial.printf("*********** [%lu] test_01 complete **************\n", millis());
}

// Test begin in slave mode - all expected possibilities, then few unexpected option
void test_02(void){
  Serial.printf("[%lu] test_02: I2S_obj=%p\n", millis(), I2S_obj);
  int ret = -1;

  Serial.printf("*********** [%lu] Begin I2S with golden parameters **************\n", millis());
  for(int mode = 0; mode < MODE_MAX; ++mode){
    for(int bpsp = 0; bpsp < bpsp_max; ++bpsp){
      Serial.printf("[%lu] begin: mode %d \"%s\", bps %d\n", millis(), mode, i2s_mode_text[mode], bps[bpsp]);
      #if defined(SOC_I2S_SUPPORTS_ADC) && defined(SOC_I2S_SUPPORTS_DAC)
        if(mode == ADC_DAC_MODE && bps[bpsp] == 16){
          #if CONFIG_IDF_TARGET_ESP32
            I2S_obj->setDataInPin(32); // Default data pin does not support DAC
          #else
            I2S_obj->setDataInPin(4); // Default data pin does not support DAC
          #endif
        }
      #endif
      ret = I2S_obj->begin(mode, bps[bpsp]);

      if(mode == ADC_DAC_MODE){
        #if defined(SOC_I2S_SUPPORTS_ADC) && defined(SOC_I2S_SUPPORTS_DAC)
          if(bps[bpsp] == 16){
              I2S_obj->setDataInPin(4); // Default data pin does not support DAC
              TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success
              TEST_ASSERT_EQUAL(true, I2S_obj->isInitialized()); // Expecting true == Success
              I2S_obj->setDataInPin(-1); // Set to default data pin
            }else{
              TEST_ASSERT_EQUAL(0, ret); // Expecting 0 == Failure
              TEST_ASSERT_EQUAL(false, I2S_obj->isInitialized()); // Expecting false == Failure
            }
          TEST_ASSERT_EQUAL(0, ret); // Expecting 0 == Failure
          TEST_ASSERT_EQUAL(false, I2S_obj->isInitialized()); // Expecting false == Failure
        #endif
      }else if(mode == PDM_STEREO_MODE || mode == PDM_MONO_MODE){
         #if defined(SOC_I2S_SUPPORTS_PDM_TX) || defined(SOC_I2S_SUPPORTS_PDM_RX)
          TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success
          TEST_ASSERT_EQUAL(true, I2S_obj->isInitialized()); // Expecting true == Success
        #else
          TEST_ASSERT_EQUAL(0, ret); // Expecting 0 == Failure
          TEST_ASSERT_EQUAL(false, I2S_obj->isInitialized()); // Expecting false == Failure
        #endif
      }else{
        TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success
        TEST_ASSERT_EQUAL(true, I2S_obj->isInitialized()); // Expecting true == Success
      }
      I2S_obj->end();
    }
  }

  Serial.printf("*********** [%lu] Begin I2S with nonexistent mode **************\n", millis());
  // Nonexistent mode - should fail
  ret = I2S_obj->begin(MODE_MAX, 8000, 8);
  TEST_ASSERT_EQUAL(0, ret); // Expecting 0 == Failure
  TEST_ASSERT_EQUAL(false, I2S_obj->isInitialized()); // Expecting false == Failure
  I2S_obj->end();

  Serial.printf("*********** [%lu] Begin I2S with Unsupported Bits Per Sample **************\n", millis());
  // Unsupported Bits Per Sample - all should fail
  int unsupported_bps[] = {-1,0,1,7,9,15,17,23,25,31,33,255,65536};
  int ubpsp_max = sizeof(unsupported_bps)/sizeof(int);
  for(int ubpsp = 0; ubpsp < ubpsp_max; ++ubpsp){
    ret = I2S_obj->begin(I2S_PHILIPS_MODE, unsupported_bps[ubpsp]);
    TEST_ASSERT_EQUAL(0, ret); // Expecting 0 == Failure
    TEST_ASSERT_EQUAL(false, I2S_obj->isInitialized()); // Expecting false == Failure
    I2S_obj->end();
  }

  Serial.printf("*********** [%lu] test_02 complete **************\n", millis());
}

// Pin setters and geters
// set all pins and check if they are set as expected
//
// set all pins to default
// get all pins and check is the are default
//
// set wrong pin numbers - expect fail
void test_03(void){
  Serial.printf("[%lu] test_03: I2S_obj=%p\n", millis(), I2S_obj);
  int ret = -1;
  bool end = false;
  while(!end){
    end = I2S_obj->isInitialized(); // initialize after first set of test and set end flag for next cycle
    Serial.printf("[%lu] test_03: intialized = %d\n", millis(), end);
    //////////////////////////////////////////////////////////////////////////////////////////////////////
    // Test on UNINITIALIZED I2S
    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Set pins to test values and check

    ret = I2S_obj->setSckPin(TEST_SCK); // Set Clock pin
    TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success
    ret = I2S_obj->getSckPin(); // Get Clock pin
    TEST_ASSERT_EQUAL(TEST_SCK, ret);

    ret = I2S_obj->setFsPin(TEST_FS); // Set Frame Sync (Word Select) pin
    TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success
    ret = I2S_obj->getFsPin(); // Get Clock pin
    TEST_ASSERT_EQUAL(TEST_FS, ret);

    ret = I2S_obj->setDataPin(TEST_SD); // Set shared Data pin for simplex mode
    TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success
    ret = I2S_obj->getDataPin(); // Get Clock pin
    TEST_ASSERT_EQUAL(TEST_SD, ret);

    ret = I2S_obj->setDataOutPin(TEST_SD_OUT); // Set Data Output pin for duplex mode
    TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success
    ret = I2S_obj->getDataOutPin(); // Get Clock pin
    TEST_ASSERT_EQUAL(TEST_SD_OUT, ret);

    ret = I2S_obj->setDataInPin(TEST_SD_IN); // Set Data Input pin for duplex mode
    TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success
    ret = I2S_obj->getDataInPin(); // Get Clock pin
    TEST_ASSERT_EQUAL(TEST_SD_IN, ret);

    //////////////////////////////////////////////////////////////////////////////////////////////////////
    // set All pins to default values and check

    ret = I2S_obj->setAllPins(); // Set pins to default
    TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success

    ret = I2S_obj->getSckPin(); // Get Clock pin
    TEST_ASSERT_EQUAL(i2s_index ? PIN_I2S1_SCK : PIN_I2S_SCK, ret);
    ret = I2S_obj->getFsPin(); // Get Frame Sync (Word Select) pin
    TEST_ASSERT_EQUAL(i2s_index ? PIN_I2S1_FS : PIN_I2S_FS, ret);
    ret = I2S_obj->getDataPin(); // Get shared Data pin for simplex mode
    TEST_ASSERT_EQUAL(i2s_index ? PIN_I2S1_SD : PIN_I2S_SD, ret);
    ret = I2S_obj->getDataOutPin(); // Get Data Output pin for duplex mode
    TEST_ASSERT_EQUAL(i2s_index ? PIN_I2S1_SD_OUT : PIN_I2S_SD_OUT, ret);
    ret = I2S_obj->getDataInPin(); // Get Data Input pin for duplex mode
    TEST_ASSERT_EQUAL(i2s_index ? PIN_I2S1_SD_IN : PIN_I2S_SD_IN, ret);
    ret = I2S_obj->setAllPins(); // Set default pins to test values

    //////////////////////////////////////////////////////////////////////////////////////////////////////
    // Set All pins to test values and check

    ret = I2S_obj->setSckPin(TEST_SCK); // Set Clock pin
    TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success
    ret = I2S_obj->getSckPin(); // Get Clock pin
    TEST_ASSERT_EQUAL(TEST_SCK, ret);

    ret = I2S_obj->setFsPin(TEST_FS); // Set Frame Sync (Word Select) pin
    TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success
    ret = I2S_obj->getFsPin(); // Get Clock pin
    TEST_ASSERT_EQUAL(TEST_FS, ret);

    ret = I2S_obj->setDataPin(TEST_SD); // Set shared Data pin for simplex mode
    TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success
    ret = I2S_obj->getDataPin(); // Get Clock pin
    TEST_ASSERT_EQUAL(TEST_SD, ret);

    ret = I2S_obj->setDataOutPin(TEST_SD_OUT); // Set Data Output pin for duplex mode
    TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success
    ret = I2S_obj->getDataOutPin(); // Get Clock pin
    TEST_ASSERT_EQUAL(TEST_SD_OUT, ret);

    ret = I2S_obj->setDataInPin(TEST_SD_IN); // Set Data Input pin for duplex mode
    TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success
    ret = I2S_obj->getDataInPin(); // Get Clock pin
    TEST_ASSERT_EQUAL(TEST_SD_IN, ret);

    ret = I2S_obj->setAllPins(); // Set pins to default
    ret = I2S_obj->setAllPins(TEST_SCK, TEST_FS, TEST_SD, TEST_SD_OUT, TEST_SD_IN); // Set pins to test values
    ret = I2S_obj->getSckPin(); // Get Clock pin
    TEST_ASSERT_EQUAL(TEST_SCK, ret);
    ret = I2S_obj->getFsPin(); // Get Clock pin
    TEST_ASSERT_EQUAL(TEST_FS, ret);
    ret = I2S_obj->getDataPin(); // Get Clock pin
    TEST_ASSERT_EQUAL(TEST_SD, ret);
    ret = I2S_obj->getDataOutPin(); // Get Clock pin
    TEST_ASSERT_EQUAL(TEST_SD_OUT, ret);
    ret = I2S_obj->getDataInPin(); // Get Clock pin
    TEST_ASSERT_EQUAL(TEST_SD_IN, ret);

    // Test on INITIALIZED I2S
    I2S_obj->begin(I2S_PHILIPS_MODE, 32000, 32);
  }

  Serial.printf("*********** [%lu] test_03 complete **************\n", millis());
}


// test simplex / duplex switch functions (not actual data transfer)
void test_04(void){
  Serial.printf("*********** [%lu] test_04 starting **************\n", millis());
  int ret = -1;
  bool end = false;
  while(!end){
    end = I2S_obj->isInitialized(); // initialize after first set of test and set end flag for next cycle

    ret = I2S_obj->isDuplex();
    TEST_ASSERT_EQUAL(0, ret); // Expecting 0 == simplex (default)

    ret = I2S_obj->setDuplex();
    TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success

    ret = I2S_obj->isDuplex();
    TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == duplex

    ret = I2S_obj->setSimplex();
    TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success

    ret = I2S_obj->isDuplex();
    TEST_ASSERT_EQUAL(0, ret); // Expecting 0 == simplex (default)
    Serial.printf("Tests done, now begin\n");
    // Test on INITIALIZED I2S
    I2S_obj->begin(I2S_PHILIPS_MODE, 32000, 32);
  }
  Serial.printf("*********** [%lu] test_04 complete **************\n", millis());
}

#if CONFIG_IDF_TARGET_ESP32
#define I2S0_DATA_OUT_IDX   I2S0O_DATA_OUT23_IDX
#define I2S0_DATA_IN_IDX   I2S0I_DATA_IN15_IDX
#elif CONFIG_IDF_TARGET_ESP32S2
#define I2S0_DATA_OUT_IDX   I2S0O_DATA_OUT23_IDX
#define I2S0_DATA_IN_IDX   I2S0I_DATA_IN15_IDX
#elif CONFIG_IDF_TARGET_ESP32C3
#define I2S0_DATA_OUT_IDX   I2SO_SD_OUT_IDX
#define I2S0_DATA_IN_IDX   I2SI_SD_IN_IDX
#elif CONFIG_IDF_TARGET_ESP32S3
#define I2S0_DATA_OUT_IDX   I2S0O_SD_OUT_IDX
#define I2S0_DATA_IN_IDX   I2S0I_SD_IN_IDX
#endif

// Simple data transmit, returned written bytes check and buffer check.
void test_05(void){
  Serial.printf("*********** [%lu] test_05 starting **************\n", millis());
  I2S_obj->setAllPins();
  // set input and output on same pin and connect internally
  I2S_obj->setDataOutPin(PIN_I2S_SD);
  I2S_obj->setDataInPin(PIN_I2S_SD);
  gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[PIN_I2S_SD], PIN_FUNC_GPIO);
  gpio_set_direction(PIN_I2S_SD, GPIO_MODE_INPUT_OUTPUT);
  esp_rom_gpio_connect_out_signal(PIN_I2S_SD, I2S0_DATA_OUT_IDX, 0, 0);
  esp_rom_gpio_connect_in_signal(PIN_I2S_SD, I2S0_DATA_IN_IDX, 0);

  int ret = -1;

  for(int mode = 0; mode < MODE_MAX; ++mode){
    Serial.printf("[%lu] data transfer begin: mode %d \"%s\"\n", millis(), mode, i2s_mode_text[mode]);
    for(int srp = 0; srp < srp_max; ++srp){
      for(int bpsp = 0; bpsp < bpsp_max; ++bpsp){
        //Serial.printf("[%lu] begin: mode %d \"%s\", sample rate %d, bps %d\n", millis(), mode, i2s_mode_text[mode], sample_rate[srp], bps[bpsp]);
        ret = I2S_obj->begin(mode, sample_rate[srp], bps[bpsp]);

        // Output buffer after init (before any write) should be empty and available for write should equal to the size of buffer
        int DMA_buffer_frame_size = I2S_obj->getDMABufferFrameSize();
        int DMA_buffer_sample_size = I2S_obj->getDMABufferSampleSize();
        int buffer_byte_size = I2S_obj->getRingBufferByteSize();
        int available_for_write = I2S_obj->availableForWrite();
        int available_samples_for_write = I2S_obj->availableSamplesForWrite();
        TEST_ASSERT_EQUAL(DMA_buffer_frame_size * 2, DMA_buffer_sample_size);
        TEST_ASSERT_EQUAL(DMA_buffer_frame_size * 2 * (bps[bpsp]/8), buffer_byte_size);
        TEST_ASSERT_EQUAL(available_for_write, buffer_byte_size);
        TEST_ASSERT_EQUAL(available_samples_for_write, DMA_buffer_sample_size);

        switch(bps[bpsp]){
          case 8:
            ret = I2S_obj->write((int8_t)data_8bit[0]);
            break;
          case 16:
            ret = I2S_obj->write((int16_t)data_16bit[0]);
            break;
          case 24:
            // 24 bits per sample mode will ignore the MSB from the 32-bit sample and return 3 on success,
            // therefore a 32-bit function inside the following case can be used.
          case 32:
            ret = I2S_obj->write((int32_t)data_32bit[0]);
            break;
        } // switch
        TEST_ASSERT_EQUAL(ret, bps[bpsp]/8);
        TEST_ASSERT_EQUAL(available_for_write-ret, I2S_obj->availableForWrite());
        available_for_write -= ret; // update for next tests

        // write rest of the
        size_t expected_bytes_written = 0;
        switch(bps[bpsp]){
          case 8:
            expected_bytes_written = sizeof(data_8bit) - sizeof(data_8bit[0]);
            ret = I2S_obj->write((uint8_t*)(&data_8bit[1]), expected_bytes_written);
            break;
          case 16:
            expected_bytes_written = sizeof(data_16bit) - sizeof(data_16bit[0]);
            ret = I2S_obj->write((uint8_t*)(&data_16bit[1]), expected_bytes_written);
            break;
          case 24:
            expected_bytes_written =  sizeof(data_24bit) - 3;
            ret = I2S_obj->write((uint8_t*)(&data_24bit[3]), expected_bytes_written);
            break;
          case 32:
            expected_bytes_written = sizeof(data_32bit) - sizeof(data_32bit[0]);
            ret = I2S_obj->write((uint8_t*)(&data_32bit[1]), expected_bytes_written);
            break;
        } // switch
        TEST_ASSERT_EQUAL(ret, expected_bytes_written);
        TEST_ASSERT_EQUAL(available_for_write-ret, I2S_obj->availableForWrite());
      } // for all bits per sample
    } // for all sample rates
  } // for all modes

  Serial.printf("*********** [%lu] test_05 complete **************\n", millis());
}

// Loop-back data transfer
void test_06(void){
  Serial.printf("*********** [%lu] test_06 starting **************\n", millis());
  gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[PIN_I2S_SD], PIN_FUNC_GPIO);
  gpio_set_direction(PIN_I2S_SD, GPIO_MODE_INPUT_OUTPUT);
  esp_rom_gpio_connect_out_signal(PIN_I2S_SD, I2S0_DATA_OUT_IDX, 0, 0);
  esp_rom_gpio_connect_in_signal(PIN_I2S_SD, I2S0_DATA_IN_IDX, 0);

  int ret = -1;

  for(int mode = 0; mode < MODE_MAX; ++mode){
    Serial.printf("[%lu] data transfer begin: mode %d \"%s\"\n", millis(), mode, i2s_mode_text[mode]);
    for(int srp = 0; srp < srp_max; ++srp){
      for(int bpsp = 0; bpsp < bpsp_max; ++bpsp){
        //Serial.printf("[%lu] begin: mode %d \"%s\", sample rate %d, bps %d\n", millis(), mode, i2s_mode_text[mode], sample_rate[srp], bps[bpsp]);
        ret = I2S_obj->begin(mode, sample_rate[srp], bps[bpsp]);
        size_t expected_bytes_written = 0;
        int available_for_write = I2S_obj->getRingBufferByteSize();
        int max_i = 0;
        switch(bps[bpsp]){
          case 8:
            expected_bytes_written = sizeof(data_8bit);
            ret = I2S_obj->write(data_8bit, expected_bytes_written);
            max_i = sizeof(data_8bit) / sizeof(data_8bit[0]);
            break;
          case 16:
            expected_bytes_written = sizeof(data_16bit);
            ret = I2S_obj->write(data_16bit, expected_bytes_written);
            max_i = sizeof(data_16bit) / sizeof(data_16bit[0]);
            break;
          case 24:
            expected_bytes_written =  sizeof(data_24bit);
            ret = I2S_obj->write(data_24bit, expected_bytes_written);
            max_i = sizeof(data_24bit) / sizeof(data_24bit[0]);
            break;
          case 32:
            expected_bytes_written = sizeof(data_32bit);
            ret = I2S_obj->write(data_32bit, expected_bytes_written);
            max_i = sizeof(data_32bit) / sizeof(data_32bit[0]);
            break;
        } // switch
        TEST_ASSERT_EQUAL(ret, expected_bytes_written);
        TEST_ASSERT_EQUAL(available_for_write-ret, I2S_obj->availableForWrite());
        while(!I2S_obj->available()){ delay(1); }
        uint8_t buffer[sizeof(data_32bit)]; // Create buffer to accommodate the largest possible array
        ret = I2S_obj->read(buffer, expected_bytes_written);
        TEST_ASSERT_EQUAL(ret, expected_bytes_written);
        TEST_ASSERT_EQUAL(0, I2S_obj->available()); // Input buffer should be empty

        // Check data
        for(int i = 0; i < max_i; ++i){
          switch(bps[bpsp]){
            case 8:
              TEST_ASSERT_EQUAL(((uint8_t*)buffer)[i], data_8bit[i]);
              break;
            case 16:
              TEST_ASSERT_EQUAL(((uint16_t*)buffer)[i], data_16bit[i]);
            case 24:
              break;
              TEST_ASSERT_EQUAL(((uint8_t*)buffer)[i], data_24bit[i]);
              break;
            case 32:
              TEST_ASSERT_EQUAL(((uint32_t*)buffer)[i], data_32bit[i]);
              break;
          } // switch
        } // Check data for loop
      } // for all bps
    } // for all sample rates
  } // for all modes
}

// Dual module data transfer test (only for ESP32 and ESP32-S3)
void test_0x(void){
  // TODO
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }
  delay(500);
  //Serial.printf("Num of I2S module =%d\n", SOC_I2S_NUM);
  //Serial.printf("I2S0=%p\n", &I2S);

  UNITY_BEGIN();
    #if SOC_I2S_NUM == 0
      RUN_TEST(test_pass); // This SoC does not have I2S - pass the test without running the tests
    #endif

    #if SOC_I2S_NUM > 0
      I2S_obj_arr[0] = &I2S;
      #if SOC_I2S_NUM > 1
        I2S_obj_arr[1] = &I2S_1;
        //Serial.printf("I2S_1=%p\n", &I2S_1);
      #endif
      i2s_index = 0;
      for(; i2s_index < SOC_I2S_NUM; ++ i2s_index){
        Serial.printf("*******************************************************\n");
        Serial.printf("********************* I2S # %d *************************\n", i2s_index);
        Serial.printf("*******************************************************\n");
        I2S_obj = I2S_obj_arr[i2s_index];
        //RUN_TEST(test_01); // begin master
        //RUN_TEST(test_02); // begin slave
        //RUN_TEST(test_03); // pin setters and geters
        RUN_TEST(test_04); // duplex / simplex

        RUN_TEST(test_05); // Simple data transmit, returned written bytes check and buffer check.
        RUN_TEST(test_06); // Loop-back data transfer
      }
      #if SOC_I2S_NUM > 1
        RUN_TEST(test_0x); // Dual module data transfer test (only for ESP32 and ESP32-S3)
      #endif
    #endif // SOC_I2S_NUM > 0
  UNITY_END();
}

void loop() {
}
