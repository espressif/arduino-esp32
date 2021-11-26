#define UNIT_TESTS_I2C_SLAVE_ADDR 0x42

enum msg_t{
NOP,              // No Operation - stay idle
UUT_RDY,          // Unit Under Test reports it is ready
START_TEST_1,
TEST_1_FINISHED,
START_TEST_2,
TEST_2_FINISHED,
START_TEST_3,
TEST_3_FINISHED,
START_TEST_4,
TEST_4_FINISHED,
START_TEST_5,
TEST_5_FINISHED,
START_TEST_6,
TEST_6_FINISHED,
START_TEST_7,
TEST_7_FINISHED,
START_TEST_8,
TEST_8_FINISHED,
START_TEST_9,
TEST_9_FINISHED,
START_TEST_10,
TEST_10_FINISHED
};

static char txt_msg[22][32] = {
"NOP",              // 00 No Operation - stay idle
"UUT_RDY",          // 01 Unit Under Test reports it is ready
"START_TEST_1",     // 02
"TEST_1_FINISHED",  // 03
"START_TEST_2",     // 04
"TEST_2_FINISHED",  // 05
"START_TEST_3",     // 06
"TEST_3_FINISHED",  // 07
"START_TEST_4",     // 08
"TEST_4_FINISHED",  // 09
"START_TEST_5",     // 10
"TEST_5_FINISHED",  // 11
"START_TEST_6",     // 12
"TEST_6_FINISHED",  // 13
"START_TEST_7",     // 14
"TEST_7_FINISHED",  // 15
"START_TEST_8",     // 16
"TEST_8_FINISHED",  // 17
"START_TEST_9",     // 18
"TEST_9_FINISHED",  // 19
"START_TEST_10",    // 20
"TEST_10_FINISHED"  // 21
};