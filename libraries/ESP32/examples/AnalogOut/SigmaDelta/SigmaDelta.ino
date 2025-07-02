void setup() {
  //setup on pin 18 with frequency 312500 Hz
  sigmaDeltaAttach(18, 312500);
  //set pin 18 to off
  sigmaDeltaWrite(18, 0);
}

void loop() {
  //slowly ramp-up the value
  //will overflow at 256
  static uint8_t i = 0;
  sigmaDeltaWrite(18, i++);
  delay(100);
}
