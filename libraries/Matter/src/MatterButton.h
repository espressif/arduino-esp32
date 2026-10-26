// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <Arduino.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

// Board button helper. An esp_timer samples the pin; loop() only drains events.
// Not a Matter Generic Switch and not a member of ArduinoMatter.

enum matterButtonEvent_t : uint8_t {
  MATTER_BUTTON_NONE = 0,
  MATTER_BUTTON_PRESS,         // debounced press (active)
  MATTER_BUTTON_CLICK,         // short release; delayed if double-click is enabled
  MATTER_BUTTON_DOUBLE_CLICK,  // second short release within the gap
  MATTER_BUTTON_LONG_HOLD,     // still held; cancels a pending click
};

class MatterButton {
public:
  MatterButton() = default;
  ~MatterButton() {
    end();
  }
  MatterButton(const MatterButton &) = delete;
  MatterButton &operator=(const MatterButton &) = delete;

  // INPUT_PULLUP and active-low (BOOT) by default.
  bool begin(uint8_t pin, uint8_t mode = INPUT_PULLUP);
  void end();

  void setDebounceMs(uint32_t ms);
  void setLongHoldMs(uint32_t ms);
  // 0 (default): emit CLICK on release. Non-zero: wait this long for a second click.
  void setDoubleClickGapMs(uint32_t ms);
  void setPollPeriodMs(uint32_t ms);
  void setActiveLow(bool activeLow);

  // Next queued event (up to 8), or MATTER_BUTTON_NONE. Drain from loop() only.
  matterButtonEvent_t poll();
  bool isPressed() const;

private:
  static void timerCb(void *arg);
  void sample();
  bool readPressed() const;
  void pushEvent(matterButtonEvent_t event);
  bool startTimer();
  void stopTimer();

  uint8_t _pin = 0;
  bool _activeLow = true;
  uint32_t _debounceMs = 50;
  uint32_t _longHoldMs = 5000;
  uint32_t _doubleClickGapMs = 0;
  uint32_t _pollPeriodMs = 20;

  bool _started = false;
  bool _rawCandidate = false;
  bool _stablePressed = false;
  bool _longEmitted = false;
  bool _clickPending = false;
  uint32_t _rawChangeMs = 0;
  uint32_t _pressStartMs = 0;
  uint32_t _clickAtMs = 0;

  static const uint8_t kQueueSize = 8;
  volatile uint8_t _queue[kQueueSize] = {};
  volatile uint8_t _qHead = 0;
  volatile uint8_t _qTail = 0;
  mutable portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;

  esp_timer_handle_t _timer = nullptr;
};
