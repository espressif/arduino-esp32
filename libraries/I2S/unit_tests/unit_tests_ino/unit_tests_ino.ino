#include "Wire.h"
#include "I2S.h"
int x;
void receiveEvent(int size)
{
   x = Wire.read();
}

void requestEvent() {
  Wire.write("hello "); // respond with message of 6 bytes
}

void setup() {
  Serial.begin(115200);
  while(!Serial){
    delay(10);
  }
  Serial.println("I2S .ino: Serial connected");
  Wire.begin(42); // Start I2C as slave with address 42
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
  Serial.println("######################################################################");
  Serial.println("I2S .ino: Testing starting");
}

void loop() {
switch(x){
  case 0:
    Serial.print("TODO: call test"); Serial.println(x);
    // Verify that unexpected calls return with error and don't crash
    // Unexpected calls are for example begin() on initialized object...
    break;
  case 1:
    Serial.print("TODO: call test"); Serial.println(x);
    // Verify data transfer - send predefined data sequence and wait for confirmation from counterpart
    // Receive predefined data sequence and send confirmation to counterpart
    // Repeat test for all digital modes
    break;
  case 2:
    Serial.print("TODO: call test"); Serial.println(x);
    // Verify buffers - proper available returns, peek and flush
    break;
  case 3:
    Serial.print("TODO: call test"); Serial.println(x);
    // Verify callbacks
    break;
  case 4:
    Serial.print("TODO: call test"); Serial.println(x);
    // Verify pin setups
    break;
  case 5:
    Serial.print("TODO: call test"); Serial.println(x);
    // Verify ADC and DAC
    break;
  case 6:
    Serial.print("TODO: call test"); Serial.println(x);
    break;
  case 7:
    Serial.print("TODO: call test"); Serial.println(x);
    break;
  case 8:
    Serial.print("TODO: call test"); Serial.println(x);
    break;
  case 9:
    Serial.print("TODO: call test"); Serial.println(x);
    break;
  case 10:
    Serial.print("TODO: call test"); Serial.println(x);
    break;


  default:
    Serial.print("Unknown command"); Serial.println(x);
    break;
  } // switch
}