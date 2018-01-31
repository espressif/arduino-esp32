#include <Arduino.h>
#include <Ticker.h>

Ticker tickerSetHigh;
Ticker tickerSetLow;

void setPin(int state) {
  digitalWrite(LED_BUILTIN, state);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(1, LOW);
  
  // every 25 ms, call setPin(0) 
  tickerSetLow.attach_ms(25, setPin, 0);
  
  // every 26 ms, call setPin(1)
  tickerSetHigh.attach_ms(26, setPin, 1);
}

void loop() {

}