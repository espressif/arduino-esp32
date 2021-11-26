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
  Serial.print("Received number "); Serial.println(data);
  enum msg_t msg = (msg_t)data;

  switch(msg){
    case START_TEST_1:
      // Verify that unexpected calls return with error and don't crash
      // Unexpected calls are for example begin() on initialized object...
      Serial.print("Received code "); Serial.print(msg);  Serial.print(" = "); Serial.println(txt_msg[msg]);
      Serial.println("Start smoke test");
      errors += test_01();
      Serial1.write(TEST_1_FINISHED);
      break;
    case START_TEST_2:
      Serial.print("TODO: call test"); Serial.println(txt_msg[msg]);
      // Verify data transfer - send predefined data sequence and wait for confirmation from counterpart
      // Receive predefined data sequence and send confirmation to counterpart
      // Repeat test for all digital modes
      break;
    case START_TEST_3:
      Serial.print("TODO: call test"); Serial.println(txt_msg[msg]);
      // Verify buffers - proper available returns, peek and flush
      break;
    case START_TEST_4:
      Serial.print("TODO: call test"); Serial.println(txt_msg[msg]);
      // Verify callbacks
      break;
    case START_TEST_5:
      Serial.print("TODO: call test"); Serial.println(txt_msg[msg]);
      // Verify pin setups
      break;
    case START_TEST_6:
      Serial.print("TODO: call test"); Serial.println(txt_msg[msg]);
      // Verify ADC and DAC
      break;
    case START_TEST_7:
      Serial.print("TODO: call test"); Serial.println(txt_msg[msg]);
      break;
    case START_TEST_8:
      Serial.print("TODO: call test"); Serial.println(txt_msg[msg]);
      break;
    case START_TEST_9:
      Serial.print("TODO: call test"); Serial.println(txt_msg[msg]);
      break;
    case START_TEST_10:
      Serial.print("TODO: call test"); Serial.println(txt_msg[msg]);
      break;
    case NOP:
      break;
    default:
      Serial.print("Unknown command number "); Serial.println(msg);
      break;
  } // switch

  msg = NOP;
}