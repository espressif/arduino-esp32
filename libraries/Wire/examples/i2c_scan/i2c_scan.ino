#include <Wire.h>

void scan(){
Serial.println("\n Scanning I2C Addresses");
uint8_t cnt=0;
for(uint8_t i=0;i<0x7F;i++){
  Wire.beginTransmission(i);
  uint8_t ec=Wire.endTransmission(true); // if device exists on bus, it will aCK
  if(ec==0){ // Device ACK'd
    if(i<16)Serial.print('0');
    Serial.print(i,HEX);
    cnt++;
  }
  else Serial.print(".."); // no one answered
  Serial.print(' ');
  if ((i&0x0f)==0x0f)Serial.println();
  }
Serial.print("Scan Completed, ");
Serial.print(cnt);
Serial.println(" I2C Devices found.");
}

void setup(){
Serial.begin(115200);
Wire.begin();
scan();
}

void loop(){
}