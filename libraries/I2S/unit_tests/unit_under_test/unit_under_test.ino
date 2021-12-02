#include "I2S.h"
#include "unit_tests_common.h"

int errors;

void setup() {
  Serial.begin(115200); // Serial monitor
  while(!Serial){
    delay(10);
  }

  #ifdef ESP_PLATFORM
    Serial1.begin(115200, SERIAL_8N1, 22, 21);
  #else
    Serial1.begin(115200);
  #endif
  while(!Serial1){
    delay(10);
  }
  errors = 0;
  Serial.println("I2S .ino: Both serial interfaces connected");
  Serial.println("######################################################################");
  Serial.println("I2S .ino: Testing starting");


  Serial.println("debug: print msg[0] as text");
  Serial.println(txt_msg[0]);
}

void loop() {
  while(!Serial1.available()){
    Serial.print(".");
    delay(1000);
    //; // do nothing
  }
  int data = Serial1.read();
  Serial.print("Received number "); Serial.print(data); Serial.print(": ");
  enum msg_t msg = (msg_t)data;

  switch(msg){
    case START_TEST_1:
      // Verify that unexpected calls return with error and don't crash
      // Unexpected calls are for example begin() on initialized object...
      Serial.print("Start smoke test "); Serial.println(txt_msg[msg]);
      errors += test_01();
      Serial1.write(TEST_1_FINISHED);
      break;
    case START_TEST_2:
      // Verify data transfer - send predefined data sequence and wait for confirmation from counterpart
      // Receive predefined data sequence and send confirmation to counterpart
      // Repeat test for all digital modes
      Serial.print("Verify data transfer "); Serial.println(txt_msg[msg]);
      errors += test_02();
      Serial1.write(TEST_2_FINISHED);
      break;
    case START_TEST_3:
      // Verify buffers - proper available returns, peek and flush
      Serial.print("Verify buffers "); Serial.println(txt_msg[msg]);
      //errors += test_03();
      Serial1.write(TEST_3_FINISHED);
      break;
    case START_TEST_4:
      // Verify callbacks
      Serial.print("Verify callbacks "); Serial.println(txt_msg[msg]);
      //errors += test_04();
      Serial1.write(TEST_4_FINISHED);
      break;
    case START_TEST_5:
      // Verify pin setups
      Serial.print("Verify pin setups "); Serial.println(txt_msg[msg]);
      //errors += test_05();
      Serial1.write(TEST_5_FINISHED);
      break;
    case START_TEST_6:
      // Verify ADC and DAC
      Serial.print("Verify ADC and DAC "); Serial.println(txt_msg[msg]);
      //errors += test_06();
      Serial1.write(TEST_6_FINISHED);
      break;
    case START_TEST_7:
      // duplex
      Serial.print("Verify duplex "); Serial.println(txt_msg[msg]);
      //errors += test_07();
      Serial1.write(TEST_7_FINISHED);
      break;
/*
    case START_TEST_8:
      Serial.print("TODO: call test"); Serial.println(txt_msg[msg]);
      errors += test_08();
      Serial1.write(TEST_8_FINISHED);
      break;
    case START_TEST_9:
      Serial.print("TODO: call test"); Serial.println(txt_msg[msg]);

      errors += test_09();
      Serial1.write(TEST_9_FINISHED);
      break;
    case START_TEST_10:
      Serial.print("TODO: call test"); Serial.println(txt_msg[msg]);
      errors += test_10();
      Serial1.write(TEST_10_FINISHED);
      break;
*/
    case NOP:
      break;
    default:
      Serial.print("Unknown command number "); Serial.println(msg);
      break;
  } // switch

  msg = NOP;
}