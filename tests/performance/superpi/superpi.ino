/*
  Based on "Calculation of PI(= 3.14159...) using FFT and AGM" by T.Ooura, Nov. 1999.
  https://github.com/Fibonacci43/SuperPI
  Modified for Arduino by Lucas Saavedra Vaz, 2024.
*/

#include <Arduino.h>

#include "pi_fftcs.h"

// Number of runs to average
#define N_RUNS 3

// Number of decimal digits to calculate
#define DIGITS (1 << 14)

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  log_d("Starting PI calculation");
  Serial.printf("Runs: %d\n", N_RUNS);
  Serial.printf("Digits: %d\n", DIGITS);
  Serial.flush();
  for (int i = 0; i < N_RUNS; i++) {
    Serial.printf("Run %d", i);
    unsigned long start = millis();
    pi_calc(DIGITS);
    unsigned long elapsed = millis() - start;
    Serial.printf("Time: %lu.%03lu s\n", elapsed / 1000, elapsed % 1000);
    Serial.flush();
  }

  log_d("PI calculation test done");
}

void loop() {
  vTaskDelete(NULL);
}
