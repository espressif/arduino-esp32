/* UART test */
#include <unity.h>

/* setUp / tearDown functions are intended to be called before / after each test. */
void setUp(void) {
}

void tearDown(void){
}

void uart_test_01(void){
  //TODO
}

void uart_test_02(void){
  //TODO
}

void uart_test_03(void){
  //TODO
}

void setup(){
  
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

  UNITY_BEGIN();
  RUN_TEST(uart_test_01);
  RUN_TEST(uart_test_02);
  RUN_TEST(uart_test_03);
  UNITY_END();
}

void loop(){
}