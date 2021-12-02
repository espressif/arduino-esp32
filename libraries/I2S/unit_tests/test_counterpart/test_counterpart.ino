#include "unit_tests_common.h"

void setup()
{
  Serial.begin(115200); // Serial monitor
  while(!Serial){
    delay(10);
  }
  Serial.println("Serial ready - connecting UART");

  #ifdef ESP_PLATFORM
    Serial1.begin(115200, SERIAL_8N1, 22, 21);
  #else
    Serial.println("This is intended to be used only on ESP!");
    while(1){ ; }
  #endif

  while(!Serial1){
    delay(10);
  }
  Serial.println("I2S unit tests counterpart starting...");
}

void loop()
{
  /*
  send_and_print(START_TEST_1);
  if(TEST_1_FINISHED != read_and_print()){
    halt();
  }
  */

  send_and_print(START_TEST_2);
  test_02();
  int recv = read_and_print();
  if(TEST_2_FINISHED != recv){
    Serial.printf("Was expecting TEST_2_FINISHED from UUT, but received %d\n", recv);
    halt();
  }

  // We will continue in later additions...
  halt();
}
