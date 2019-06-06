/*
  Passing paramters to Ticker callbacks

  Apart from void(void) functions, the Ticker library supports
  functions taking one argument. This argument's size has to be less or
  equal to 4 bytes (so char, short, int, float, void*, char* types will do).

  This sample runs two tickers that both call one callback function,
  but with different arguments.

  The built-in LED will be pulsing.
*/

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
