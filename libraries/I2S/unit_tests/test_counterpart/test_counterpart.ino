#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "unit_tests_common.h"
#include "driver/uart.h"

void setup()
{
  Serial.begin(115200); // Serial monitor
  while(!Serial){
    delay(10);
  }

  #ifdef ESP_PLATFORM
    Serial1.begin(115200, SERIAL_8N1, 22, 21);
  #else
    Serial.println("This is intended to be used only on ESP!");
    while(1){ ; }
  #endif

  while(!Serial1){
    delay(10);
  }
  Serial.println("I2C initialized successfully");
  Serial.println("I2S unit tests counterpart starting...");
}


void loop()
{
  enum msg_t data = START_TEST_1;

  Serial1.write(data);
  Serial.printf("Sent msg num %d = \"%s\"\n", data, txt_msg[data]);

  // Read data from UART.
  uint8_t data_read[128];
  while(!Serial1.available()){
    ; // wait
  }

  int recv;
  while(Serial1.available()){
    recv = Serial1.read();
  }
  Serial.print("Received response number ");
  Serial.print(recv);
  Serial.print(" = ");
  Serial.println(txt_msg[recv]);

  // We will continue in later additions...
  while(1){
    ; // halt
  }
}
