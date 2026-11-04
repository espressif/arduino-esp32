/*
   SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD

   SPDX-License-Identifier: Apache-2.0

   Hardware Timer FunctionalInterrupt Example
   ==========================================

   Shows the different ways to declare a hardware timer callback:
     1. C function pointer          - timerAttachInterrupt(timer, &fn)
     2. Capturing lambda            - needs FunctionalInterrupt.h
     3. std::function object        - stored in DRAM, not IRAM
     4. std::bind to a member       - class instance as the callback
     5. timerAttachInterruptArg     - C callback with a context pointer

   The sketch re-attaches a different style every 5 alarms. Press BOOT to
   stop and free the timer.
*/

#include <Arduino.h>
#include <FunctionalInterrupt.h>

hw_timer_t *timer = NULL;
volatile uint32_t isrCounter = 0;

enum DemoMode : uint8_t {
  MODE_C_PTR = 0,
  MODE_LAMBDA,
  MODE_STD_FUNCTION,
  MODE_BIND,
  MODE_ARG,
  MODE_COUNT
};

static const char *modeName[MODE_COUNT] = {
  "C function pointer", "capturing lambda", "std::function object", "std::bind member", "timerAttachInterruptArg",
};

static uint8_t currentMode = MODE_C_PTR;

static void ARDUINO_ISR_ATTR bumpCounter() {
  isrCounter = isrCounter + 1;
}

static void ARDUINO_ISR_ATTR bumpCounterArg(void *arg) {
  volatile uint32_t *count = (volatile uint32_t *)arg;
  *count = *count + 1;
}

class TickSource {
public:
  void ARDUINO_ISR_ATTR isr() {
    isrCounter = isrCounter + 1;
  }
};

static TickSource tickSource;

// std::function is a writable C++ object. Do not mark it IRAM_ATTR: on RISC-V
// (ESP32-C6 and similar) that places it in instruction RAM and the constructor
// store-faults at boot.
std::function<void()> onTimerFn = []() {
  isrCounter = isrCounter + 1;
};

static void attachMode(uint8_t mode) {
  switch (mode) {
    case MODE_C_PTR:
      // Exact match for the C overload. No FunctionalInterrupt.h required.
      timerAttachInterrupt(timer, &bumpCounter);
      break;
    case MODE_LAMBDA:
      // Non-capturing lambdas can be ambiguous with the C overload; capture.
      timerAttachInterrupt(timer, [&]() {
        isrCounter = isrCounter + 1;
      });
      break;
    case MODE_STD_FUNCTION: timerAttachInterrupt(timer, onTimerFn); break;
    case MODE_BIND:         timerAttachInterrupt(timer, std::bind(&TickSource::isr, &tickSource)); break;
    case MODE_ARG:
      // C API with a user pointer. Same idea as GPIO attachInterruptArg().
      timerAttachInterruptArg(timer, bumpCounterArg, (void *)&isrCounter);
      break;
    default: break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(BOOT_PIN, INPUT_PULLUP);

  Serial.println("Hardware Timer FunctionalInterrupt Example");
  Serial.println("Re-attaches a different callback style every 5 ticks.");
  Serial.println("Press BOOT to stop the timer.");

  timer = timerBegin(1000000);
  if (timer == NULL) {
    Serial.println("Failed to start timer");
    return;
  }

  attachMode(currentMode);
  Serial.printf("Using %s\n", modeName[currentMode]);
  // Alarm every second, auto-reload, unlimited count
  timerAlarm(timer, 1000000, true, 0);
}

void loop() {
  static uint32_t lastCount = 0;
  uint32_t count = isrCounter;

  if (count != lastCount) {
    lastCount = count;
    Serial.printf("[%s] fired %" PRIu32 " times\n", modeName[currentMode], count);

    if ((count % 5) == 0) {
      currentMode = (currentMode + 1) % MODE_COUNT;
      attachMode(currentMode);
      Serial.printf("Switched to %s\n", modeName[currentMode]);
    }
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
