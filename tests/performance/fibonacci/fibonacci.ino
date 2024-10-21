/*
  Fibonacci calculation test for Arduino and ESP32.
  Created by Lucas Saavedra Vaz, 2024
*/

#include <Arduino.h>

// Number of runs to average
#define N_RUNS 3

// Fibonacci number to calculate. Keep between 35 and 45.
#define FIB_N 40

uint64_t fib(uint32_t n) {
  if (n < 2) {
    return n;
  }
  return fib(n - 1) + fib(n - 2);
}

void setup() {
  uint64_t fibonacci;

  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  log_d("Starting fibonacci calculation");
  Serial.printf("Runs: %d\n", N_RUNS);
  Serial.printf("N: %d\n", FIB_N);
  Serial.flush();
  for (int i = 0; i < N_RUNS; i++) {
    Serial.printf("Run %d\n", i);
    unsigned long start = millis();
    fibonacci = fib(FIB_N);
    unsigned long elapsed = millis() - start;
    Serial.printf("Fibonacci(N): %llu\n", fibonacci);
    Serial.printf("Time: %lu.%03lu s\n", elapsed / 1000, elapsed % 1000);
    Serial.flush();
  }

  log_d("Fibonacci calculation test done");
}

void loop() {
  vTaskDelete(NULL);
}
