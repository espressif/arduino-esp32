// ESP32 Touch Test
// Just test touch pin - Touch2 is T2 which is on GPIO 2.

void setup() {
  Serial.begin(115200);
  delay(1000);  // give me time to bring up serial monitor
  Serial.println("ESP32 Touch Test");
}

void loop() {
  Serial.println(touchRead(T2));  // get value using T2
  delay(1000);
}
