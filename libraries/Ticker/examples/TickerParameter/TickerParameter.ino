/*
  Passing parameters to Ticker callbacks

  Apart from void(void) functions, the Ticker library supports
  functions taking one argument. This argument's size has to be less or
  equal to 4 bytes (so char, short, int, float, void*, char* types will do).

  This sample runs two tickers that both call one callback function,
  but with different arguments.

  The built-in LED will be pulsing.
*/

#include <Ticker.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 13
#endif

Ticker tickerSetLow;
Ticker tickerSetHigh;
Ticker tickerSetChar;

void setPinLow() {
  digitalWrite(LED_BUILTIN, 0);
}

void setPinHigh() {
  digitalWrite(LED_BUILTIN, 1);
}

void setPin(int state) {
  digitalWrite(LED_BUILTIN, state);
}

void setPinChar(char state) {
  digitalWrite(LED_BUILTIN, state);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  // every 25 ms, call setPinLow()
  tickerSetLow.attach_ms(25, setPinLow);

  // every 26 ms, call setPinHigh()
  tickerSetHigh.attach_ms(26, setPinHigh);

  // every 54 ms, call setPinChar(1)
  tickerSetChar.attach_ms(26, setPinChar, (char)1);
}

void loop() {
}
