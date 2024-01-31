/*
 * This example demonstrates used of Ticker with arguments.
 * You can call the same callback function with different argument on different times.
 * Based on the argument the callback can perform different tasks.
 */

#include <Arduino.h>
#include <Ticker.h>

// Arguments for the function must remain valid (not run out of scope) otherwise the function would read garbage data.
int LED_PIN_1 = 4;
#ifdef LED_BUILTIN
  int LED_PIN_2 = LED_BUILTIN;
#else
  int LED_PIN_2 = 8;
#endif

Ticker tickerSetHigh;
Ticker tickerSetLow;

// Argument to callback must always be passed a reference
void swapState(int *pin) {
  static int led_1_state = 1;
  static int led_2_state = 1;
  if(*pin == LED_PIN_1){
    Serial.printf("[%lu ms] set pin %d to state: %d\n", millis(), *pin, led_1_state);
    digitalWrite(*pin, led_1_state);
    led_1_state = led_1_state ? 0 : 1; // reverse for next pass
  }else if(*pin == LED_PIN_2){
    Serial.printf("[%lu ms] set pin %d to state: %d\n", millis(), *pin, led_2_state);
    digitalWrite(*pin, led_2_state);
    led_2_state = led_2_state ? 0 : 1; // reverse for next pass
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN_1, OUTPUT);
  pinMode(LED_PIN_2, OUTPUT);
  //digitalWrite(1, LOW);
  
  // Blink LED every 500 ms on LED_PIN_1
  tickerSetLow.attach_ms(500, swapState, &LED_PIN_1);
  
  // Blink LED every 1000 ms on LED_PIN_2
  tickerSetHigh.attach_ms(1000, swapState, &LED_PIN_2);
}

void loop() {

}
