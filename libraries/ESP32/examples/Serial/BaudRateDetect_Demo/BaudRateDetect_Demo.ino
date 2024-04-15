/*
   This Sketch demonstrates how to detect and set the baud rate when the UART0 is connected to
   some port that is sending data. It can be used with the Arduino IDE Serial Monitor to send the data.

   Serial.begin(0) will start the baud rate detection. Valid range is 300 to 230400 baud.
   It will try to detect for 20 seconds, by default, while reading RX.
   This timeout of 20 seconds can be changed in the begin() function through <<timeout_ms>> parameter:

   void HardwareSerial::begin(baud, config, rxPin, txPin, invert, <<timeout_ms>>, rxfifo_full_thrhd)

   It is necessary that the other end sends some data within <<timeout_ms>>, otherwise the detection won't work.

   IMPORTANT NOTE: baud rate detection seem to only work with ESP32 and ESP32-S2.
   In other other SoCs, it doesn't work.

*/

// Open the Serial Monitor with testing baud start typing and sending characters
void setup() {
  Serial.begin(0);  // it will try to detect the baud rate for 20 seconds

  Serial.print("\n==>The baud rate is ");
  Serial.println(Serial.baudRate());

  //after 20 seconds timeout, when not detected, it will return zero - in this case, we set it back to 115200.
  if (Serial.baudRate() == 0) {
    // Trying to set Serial to a safe state at 115200
    Serial.end();
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    delay(1000);
    log_e("Baud rate detection failed.");
  }
}

void loop() {
}
