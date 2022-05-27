/*
  BlinkRGB

  Demonstrates usage of onboard RGB LED on some ESP dev boards.

  Calling digitalWrite(LED_BUILTIN, HIGH) will use hidden RGB driver.
    
  RGBLedWrite demonstrates controll of each channel:
  void neopixelWrite(uint8_t pin, uint8_t red_val, uint8_t green_val, uint8_t blue_val)

  WARNING: After using digitalWrite to drive RGB LED it will be impossible to drive the same pin
    with normal HIGH/LOW level
*/
//#define LED_BRIGHTNESS 64 // Change white brightness (max 255)

// the setup function runs once when you press reset or power the board

void setup() {
  // No need to initialize the RGB LED
}

// the loop function runs over and over again forever
void loop() {
#ifdef BOARD_HAS_NEOPIXEL
  digitalWrite(LED_BUILTIN, HIGH);   // Turn the RGB LED white
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);    // Turn the RGB LED off
  delay(1000);

  neopixelWrite(LED_BUILTIN,LED_BRIGHTNESS,0,0); // Red
  delay(1000);
  neopixelWrite(LED_BUILTIN,0,LED_BRIGHTNESS,0); // Green
  delay(1000);
  neopixelWrite(LED_BUILTIN,0,0,LED_BRIGHTNESS); // Blue
  delay(1000);
  neopixelWrite(LED_BUILTIN,0,0,0); // Off / black
  delay(1000);
#endif
}
