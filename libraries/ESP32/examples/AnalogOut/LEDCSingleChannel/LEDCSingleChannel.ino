/*
 LEDC Software Fade on shared channel with multiple pins

 This example shows how to software fade LED
 using the ledcWriteChannel function on multiple pins.
 This example is useful if you need to control synchronously
 multiple LEDs on different pins.

 Code adapted from original Arduino Fade example:
 https://www.arduino.cc/en/Tutorial/Fade

 This example code is in the public domain.
 */

// use 8 bit precision for LEDC timer
#define LEDC_TIMER_8_BIT 8

// use 5000 Hz as a LEDC base frequency
#define LEDC_BASE_FREQ 5000

// LED pins
#define LED_PIN_1 4
#define LED_PIN_2 5

// LED channel that will be used instead of automatic selection.
#define LEDC_CHANNEL 0

int brightness = 0;  // how bright the LED is
int fadeAmount = 5;  // how many points to fade the LED by

void setup() {
  // Use single LEDC channel 0 for both pins
  ledcAttachChannel(LED_PIN_1, LEDC_BASE_FREQ, LEDC_TIMER_8_BIT, LEDC_CHANNEL);
  ledcAttachChannel(LED_PIN_2, LEDC_BASE_FREQ, LEDC_TIMER_8_BIT, LEDC_CHANNEL);
}

void loop() {
  // set the brightness on LEDC channel 0
  ledcWriteChannel(LEDC_CHANNEL, brightness);

  // change the brightness for next time through the loop:
  brightness = brightness + fadeAmount;

  // reverse the direction of the fading at the ends of the fade:
  if (brightness <= 0 || brightness >= 255) {
    fadeAmount = -fadeAmount;
  }
  // wait for 30 milliseconds to see the dimming effect
  delay(30);
}
