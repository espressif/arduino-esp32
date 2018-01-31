#include <Arduino.h>
#include <Ticker.h>

Ticker blinker;
Ticker toggler;
Ticker changer;
float blinkerPace = 0.1;  //seconds
const float togglePeriod = 5; //seconds

void change() {
  blinkerPace = 0.5;
}

void blink() {
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
}

void toggle() {
  static bool isBlinking = false;
  if (isBlinking) {
    blinker.detach();
    isBlinking = false;
  }
  else {
    blinker.attach(blinkerPace, blink);
    isBlinking = true;
  }
  digitalWrite(LED_BUILTIN, LOW);  //make sure LED on on after toggling (pin LOW = led ON)
}

void setup() {
	pinMode(LED_BUILTIN, OUTPUT);
	toggler.attach(togglePeriod, toggle);
  changer.once(30, change);
}

void loop() {
  
}
