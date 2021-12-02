#pragma once

#include "I2S.h"

enum msg_t{
NOP,              // 00 No Operation - stay idle
UUT_RDY,          // 01 Unit Under Test reports it is ready
UUT_ERR,          // 02 Unit Under Test reports error - cannot proceed with the current setup. Error will be counted by UUT.
C_RDY,            // 03 Counterpart reports it is ready
C_ERR,            // 04 Counterpart reports reports error - cannot proceed with the current setup. Error will be counted by UUT upon receiving this message.
START_TEST_1,     // 05
TEST_1_FINISHED,  // 06
START_TEST_2,     // 07
TEST_2_FINISHED,  // 08
START_TEST_3,     // 09
TEST_3_FINISHED,  // 10
START_TEST_4,     // 11
TEST_4_FINISHED,  // 12
START_TEST_5,     // 13
TEST_5_FINISHED,  // 14
START_TEST_6,     // 15
TEST_6_FINISHED,  // 16
START_TEST_7,     // 17
TEST_7_FINISHED,  // 18
START_TEST_8,     // 19
TEST_8_FINISHED,  // 20
START_TEST_9,     // 21
TEST_9_FINISHED,  // 22
START_TEST_10,    // 23
TEST_10_FINISHED  // 24
};

static char txt_msg[26][32] = {
"NOP",              // 00 No Operation - stay idle
"UUT_RDY",          // 01 Unit Under Test reports it is ready
"UUT_ERR",          // 02
"Counterpart ready",// 03
"Counterpart error",// 04
"START_TEST_1",     // 05
"TEST_1_FINISHED",  // 06
"START_TEST_2",     // 07
"TEST_2_FINISHED",  // 08
"START_TEST_3",     // 09
"TEST_3_FINISHED",  // 10
"START_TEST_4",     // 11
"TEST_4_FINISHED",  // 12
"START_TEST_5",     // 13
"TEST_5_FINISHED",  // 14
"START_TEST_6",     // 15
"TEST_6_FINISHED",  // 16
"START_TEST_7",     // 17
"TEST_7_FINISHED",  // 18
"START_TEST_8",     // 19
"TEST_8_FINISHED",  // 20
"START_TEST_9",     // 21
"TEST_9_FINISHED",  // 22
"START_TEST_10",    // 23
"TEST_10_FINISHED"  // 24
};

//const i2s_mode_t mode[] = {I2S_PHILIPS_MODE, I2S_RIGHT_JUSTIFIED_MODE, I2S_LEFT_JUSTIFIED_MODE, PDM_STEREO_MODE, PDM_MONO_MODE}; // Note: ADC_DAC_MODE is missing - it will be tested separately
const i2s_mode_t mode[] = {I2S_PHILIPS_MODE}; // debug
//const long sampleRate[] = {8000, 11025, 16000, 22050, 24000, 32000, 44100};
const long sampleRate[] = {8000}; // debug
//const int bitsPerSample[] = {8, 16, 24, 32};
const int bitsPerSample[] = {8}; // debug

//const uint8_t  bps8[]  = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,127,128,129,255};
//const uint8_t  bps8[]  = {0,1,255}; // debug
//const uint8_t  bps8[]  = {170,170,170,170,170,170}; // debug
const uint8_t  bps8[]  = {170}; // debug
const uint16_t bps16[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,127,128,129,255,256,32767,32768,32769,65534,65535};
const uint32_t bps24[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,127,128,129,255,256,32767,32768,32769,65534,65535,16777214,16777215};
const uint32_t bps32[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,127,128,129,255,256,32767,32768,32769,65534,65535,65536,16777214,16777215,16777216,4294967294,4294967295};

void send_and_print(enum msg_t msg);
enum msg_t read_and_print();
enum msg_t wait_and_read();
void halt();
