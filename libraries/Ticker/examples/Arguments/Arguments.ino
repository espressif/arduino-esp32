#include <Arduino.h>
#include <Ticker.h>

// attach a LED to GPIO 21
#define LED_PIN 21

Ticker tickerSetHigh;
Ticker tickerSetLow;

// Argument to callback must always be passed a reference
void setPin(int *state) {
  digitalWrite(LED_PIN, *state);
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(1, LOW);
  
  // every 25 ms, call setPin(0) 
  int state = 0;
  tickerSetLow.attach_ms(25, setPin, &state);
  
  // every 26 ms, call setPin(1)
  state = 1;
  tickerSetHigh.attach_ms(26, setPin, &state);
}

void loop() {

}
