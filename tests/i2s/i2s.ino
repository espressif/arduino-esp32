#include <unity.h>
#include <I2S.h>
#include "Arduino.h"

I2SClass *I2S_obj;
I2SClass *I2S_obj_arr[SOC_I2S_NUM];

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

// Test begin - all expected possibilities, then few unexpected option
void test_01(void){
  //Serial.printf("[%lu] test 1: I2S_obj=%p\n", millis(), I2S_obj);
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
          #else
            TEST_ASSERT_EQUAL(0, ret); // Expecting 0 == Failure
          #endif
        }else if(mode == PDM_STEREO_MODE || mode == PDM_MONO_MODE){
           #if defined(SOC_I2S_SUPPORTS_PDM_TX) || defined(SOC_I2S_SUPPORTS_PDM_RX)
            TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success
          #else
            TEST_ASSERT_EQUAL(0, ret); // Expecting 0 == Failure
          #endif
        }else{
          TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success
        }
        I2S_obj->end();
      }
    }
  }

  //Serial.printf("*********** [%lu] Begin I2S with nonexistent mode **************\n", millis());
  // Nonexistent mode - should fail
  ret = I2S_obj->begin(MODE_MAX, 8000, 8);
  TEST_ASSERT_EQUAL(0, ret); // Expecting 0 == Failure
  I2S_obj->end();

  //Serial.printf("*********** [%lu] Begin I2S with Unsupported Bits Per Sample **************\n", millis());
  // Unsupported Bits Per Sample - all should fail
  int unsupported_bps[] = {-1,0,1,7,9,15,17,23,25,31,33,255,65536};
  int ubpsp_max = sizeof(unsupported_bps)/sizeof(int);
  for(int ubpsp = 0; ubpsp < ubpsp_max; ++ubpsp){
    ret = I2S_obj->begin(I2S_PHILIPS_MODE, 8000, unsupported_bps[ubpsp]);
    TEST_ASSERT_EQUAL(0, ret); // Expecting 0 == Failure
    I2S_obj->end();
  }

  //Serial.printf("*********** [%lu] Begin I2S with Unusual, yet possible sample rates **************\n", millis());
  // Unusual, yet possible sample rates - all should succeed
  int unusual_sample_rate[] = {4000,64000,88200,96000,128000};
  int usrp_max = sizeof(unusual_sample_rate)/sizeof(int);
  for(int usrp = 0; usrp < usrp_max; ++usrp){
    ret = I2S_obj->begin(I2S_PHILIPS_MODE, unusual_sample_rate[usrp], 32);
    TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success
    I2S_obj->end();
  }
  //Serial.printf("*********** [%lu] test_01 complete **************\n", millis());
}

// Pin setters and geters
void test_02(void){
  int ret = -1;

  // Test on UNINITIALIZED I2S

  ret = I2S_obj->setAllPins(); // Set default pins
  TEST_ASSERT_EQUAL(1, ret); // Expecting 1 == Success
  // TODO
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

  UNITY_BEGIN();
  for(int obj = 0; obj < SOC_I2S_NUM; ++ obj){
    //Serial.printf("*******************************************************\n");
    //Serial.printf("********************* I2S # %d *************************\n", obj);
    //Serial.printf("*******************************************************\n");
    I2S_obj = I2S_obj_arr[obj];
    RUN_TEST(test_01);
    RUN_TEST(test_02);
  }
  UNITY_END();
}

void loop() {
}
