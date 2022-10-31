#include <unity.h>
#include <I2S.h>
#include "Arduino.h"

I2SClass *I2S_obj;
I2SClass *I2S_obj_arr[SOC_I2S_NUM];
int i2s_index;

#define TEST_SCK 1
#define TEST_FS 2
#define TEST_SD 3
#define TEST_SD_OUT 4
#define TEST_SD_IN 5

/* These functions are intended to be called before and after each test. */
void setUp(void) {
  // use default pins
  I2S.setAllPins();
  #if SOC_I2S_NUM > 1
    I2S1.setAllPins();
  #endif
}

void tearDown(void){
  I2S.end();
  #if SOC_I2S_NUM > 1
    I2S1.end();
  #endif
}

// Test begin in master mode - all expected possibilities, then few unexpected option
void test_01(void){
  //Serial.printf("[%lu] test_01: I2S_obj=%p\n", millis(), I2S_obj);
  int sample_rate[] = {8000,11025,16000,22050,32000,44100,64000,88200,128000};
  //int sample_rate[] = {8000};
  int srp_max = sizeof(sample_rate)/sizeof(int);

  int bps[] = {8,16,24,32};
  //int bps[] = {8};
  int bpsp_max = sizeof(bps)/sizeof(int);

  int ret = -1;

  //Serial.printf("*********** [%lu] Begin I2S with golden parameters **************\n", millis());
  for(int mode = 0; mode < MODE_MAX; ++mode){
    for(int srp = 0; srp < srp_max; ++srp){
      for(int bpsp = 0; bpsp < bpsp_max; ++bpsp){
        //Serial.printf("[%lu] begin: mode %d \"%s\", sample rate %d, bps %d\n", millis(), mode, i2s_mode_text[mode], sample_rate[srp], bps[bpsp]);
        ret = I2S_obj->begin(mode, sample_rate[srp], bps[bpsp]);

        if(mode == ADC_DAC_MODE){
          #if defined(SOC_I2S_SUPPORTS_ADC) && defined(SOC_I2S_SUPPORTS_DAC)
            TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success
            TEST_ASSERT_EQUAL(true, I2S_obj->isInitialized()); // Expecting true == Success
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

  //Serial.printf("*********** [%lu] Begin I2S with nonexistent mode **************\n", millis());
  // Nonexistent mode - should fail
  ret = I2S_obj->begin(MODE_MAX, 8000, 8);
  TEST_ASSERT_EQUAL(0, ret); // Expecting 0 == Failure
  TEST_ASSERT_EQUAL(false, I2S_obj->isInitialized()); // Expecting false == Failure
  I2S_obj->end();

  //Serial.printf("*********** [%lu] Begin I2S with Unsupported Bits Per Sample **************\n", millis());
  // Unsupported Bits Per Sample - all should fail
  int unsupported_bps[] = {-1,0,1,7,9,15,17,23,25,31,33,255,65536};
  int ubpsp_max = sizeof(unsupported_bps)/sizeof(int);
  for(int ubpsp = 0; ubpsp < ubpsp_max; ++ubpsp){
    ret = I2S_obj->begin(I2S_PHILIPS_MODE, 8000, unsupported_bps[ubpsp]);
    TEST_ASSERT_EQUAL(0, ret); // Expecting 0 == Failure
    TEST_ASSERT_EQUAL(false, I2S_obj->isInitialized()); // Expecting false == Failure
    I2S_obj->end();
  }

  //Serial.printf("*********** [%lu] Begin I2S with Unusual, yet possible sample rates **************\n", millis());
  // Unusual, yet possible sample rates - all should succeed
  int unusual_sample_rate[] = {4000,64000,88200,96000,128000};
  int usrp_max = sizeof(unusual_sample_rate)/sizeof(int);
  for(int usrp = 0; usrp < usrp_max; ++usrp){
    ret = I2S_obj->begin(I2S_PHILIPS_MODE, unusual_sample_rate[usrp], 32);
    TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success
    TEST_ASSERT_EQUAL(true, I2S_obj->isInitialized()); // Expecting true == Success
    I2S_obj->end();
  }
  //Serial.printf("*********** [%lu] test_01 complete **************\n", millis());
}

// Test begin in slave mode - all expected possibilities, then few unexpected option
void test_02(void){
  //Serial.printf("[%lu] test_02: I2S_obj=%p\n", millis(), I2S_obj);

  int bps[] = {8,16,24,32};
  //int bps[] = {8};
  int bpsp_max = sizeof(bps)/sizeof(int);

  int ret = -1;

  //Serial.printf("*********** [%lu] Begin I2S with golden parameters **************\n", millis());
  for(int mode = 0; mode < MODE_MAX; ++mode){
    for(int bpsp = 0; bpsp < bpsp_max; ++bpsp){
      //Serial.printf("[%lu] begin: mode %d \"%s\", bps %d\n", millis(), mode, i2s_mode_text[mode], bps[bpsp]);
      ret = I2S_obj->begin(mode, bps[bpsp]);

      if(mode == ADC_DAC_MODE){
        #if defined(SOC_I2S_SUPPORTS_ADC) && defined(SOC_I2S_SUPPORTS_DAC)
          TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success
          TEST_ASSERT_EQUAL(true, I2S_obj->isInitialized()); // Expecting true == Success
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

  //Serial.printf("*********** [%lu] Begin I2S with nonexistent mode **************\n", millis());
  // Nonexistent mode - should fail
  ret = I2S_obj->begin(MODE_MAX, 8000, 8);
  TEST_ASSERT_EQUAL(0, ret); // Expecting 0 == Failure
  TEST_ASSERT_EQUAL(false, I2S_obj->isInitialized()); // Expecting false == Failure
  I2S_obj->end();

  //Serial.printf("*********** [%lu] Begin I2S with Unsupported Bits Per Sample **************\n", millis());
  // Unsupported Bits Per Sample - all should fail
  int unsupported_bps[] = {-1,0,1,7,9,15,17,23,25,31,33,255,65536};
  int ubpsp_max = sizeof(unsupported_bps)/sizeof(int);
  for(int ubpsp = 0; ubpsp < ubpsp_max; ++ubpsp){
    ret = I2S_obj->begin(I2S_PHILIPS_MODE, unsupported_bps[ubpsp]);
    TEST_ASSERT_EQUAL(0, ret); // Expecting 0 == Failure
    TEST_ASSERT_EQUAL(false, I2S_obj->isInitialized()); // Expecting false == Failure
    I2S_obj->end();
  }

  //Serial.printf("*********** [%lu] test_02 complete **************\n", millis());
}

// Pin setters and geters
// set all pins and check if they are set as expected
//
// set all pins to default
// get all pins and check is the are default
//
// set wrong pin numbers - expect fail
void test_03(void){
  //Serial.printf("[%lu] test_03: I2S_obj=%p\n", millis(), I2S_obj);
  int ret = -1;
  bool end = false;
  while(!end){
    end = I2S_obj->isInitialized(); // initialize after first set of test and set end flag for next cycle
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

  //Serial.printf("*********** [%lu] test_03 complete **************\n", millis());
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }
  
  //Serial.printf("Num of I2S module =%d\n", SOC_I2S_NUM);
  //Serial.printf("I2S0=%p\n", &I2S);
  I2S_obj_arr[0] = &I2S;
  #if SOC_I2S_NUM > 1
    I2S_obj_arr[1] = &I2S1;
    //Serial.printf("I2S1=%p\n", &I2S1);
  #endif
  i2s_index = 0;
  UNITY_BEGIN();
  for(; i2s_index < SOC_I2S_NUM; ++ i2s_index){
    //Serial.printf("*******************************************************\n");
    //Serial.printf("********************* I2S # %d *************************\n", i2s_index);
    //Serial.printf("*******************************************************\n");
    I2S_obj = I2S_obj_arr[i2s_index];
    RUN_TEST(test_01); // begin master
    RUN_TEST(test_02); // begin slave
    RUN_TEST(test_03); // pin setters and geters
  }
  UNITY_END();
}

void loop() {
}
