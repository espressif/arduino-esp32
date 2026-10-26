/*
   SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD

   SPDX-License-Identifier: Apache-2.0

   Hardware Timer FunctionalInterrupt Example
   ==========================================

   Attaches a capturing lambda to a hardware timer using
   timerAttachInterrupt() from FunctionalInterrupt.h.

   Press the BOOT button to stop and free the timer.
*/

#include <Arduino.h>
#include <FunctionalInterrupt.h>

hw_timer_t *timer = NULL;
volatile uint32_t isrCounter = 0;

// Capturing lambda: non-capturing lambdas can be ambiguous with the C overload.
// IRAM_ATTR keeps the std::function object in IRAM when CONFIG_ARDUINO_ISR_IRAM is set.
IRAM_ATTR std::function<void()> onTimer = [&]() {
  isrCounter = isrCounter + 1;
};

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(BOOT_PIN, INPUT_PULLUP);

  Serial.println("Hardware Timer FunctionalInterrupt Example");
  Serial.println("Press BOOT to stop the timer.");

  timer = timerBegin(1000000);
  if (timer == NULL) {
    Serial.println("Failed to start timer");
    return;
  }

  timerAttachInterrupt(timer, onTimer);
  // Alarm every second, auto-reload, unlimited count
  timerAlarm(timer, 1000000, true, 0);
}

void loop() {
  static uint32_t lastCount = 0;
  uint32_t count = isrCounter;

  if (count != lastCount) {
    lastCount = count;
    Serial.printf("Timer lambda fired %" PRIu32 " times\n", count);
  }

  if (digitalRead(BOOT_PIN) == LOW) {
    if (timer) {
      timerEnd(timer);
      timer = NULL;
      Serial.println("Timer stopped");
    }
  }

  delay(10);
}
