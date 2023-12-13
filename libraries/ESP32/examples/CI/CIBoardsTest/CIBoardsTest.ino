#include <Wire.h>
#include <SPI.h>

void setup() {
  // UART initialization
  Serial.begin(9600);

  // I2C initialization
  Wire.begin();

  // SPI initialization
  SPI.begin();
}

void loop() {
  // UART echo
  if (Serial.available()) {
    Serial.write(Serial.read());
  }

  // I2C read/write
  Wire.beginTransmission(0x68);  // I2C address of device
  Wire.write(0x00);              // register to read/write
  Wire.write(0xFF);              // data to write (if writing)
  Wire.endTransmission();

  Wire.requestFrom(0x68, 1);     // number of bytes to read

  while (Wire.available()) {
    Serial.println(Wire.read());
  }

  // SPI read/write
  digitalWrite(SS, LOW);         // select slave device
  SPI.transfer(0x01);            // data to write
  digitalWrite(SS, HIGH);        // deselect slave device

  digitalWrite(SS, LOW);         // select slave device
  byte data = SPI.transfer(0x00);// data to read
  digitalWrite(SS, HIGH);        // deselect slave device

  Serial.println(data);

  delay(1000);                   // wait for 1 second before repeating loop
}
