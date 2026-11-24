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

/**
 * @brief Board-button events produced by @ref MatterButton.
 *
 * Not a Matter Generic Switch cluster. Drain from @c loop() via @ref MatterButton::poll.
 */
enum matterButtonEvent_t : uint8_t {
  MATTER_BUTTON_NONE = 0,      ///< No queued event.
  MATTER_BUTTON_PRESS,         ///< Debounced press (active).
  MATTER_BUTTON_CLICK,         ///< Short release; delayed if double-click is enabled.
  MATTER_BUTTON_DOUBLE_CLICK,  ///< Second short release within the gap.
  MATTER_BUTTON_LONG_HOLD,     ///< Still held; cancels a pending click.
};

/**
 * @brief Board-button helper for Matter example sketches.
 *
 * An @c esp_timer samples the pin; @c loop() only drains events via @ref poll.
 * Not a Matter Generic Switch and not a member of ArduinoMatter.
 *
 * Defaults: 50 ms debounce, 5 s long-hold, double-click off (click on release),
 * 20 ms sample, active-low @c INPUT_PULLUP (BOOT).
 *
 * @note Do not call Matter APIs from the timer callback.
 */
class MatterButton {
public:
  /**
   * @brief Construct an uninitialized button helper.
   */
  MatterButton() = default;
  /**
   * @brief Stop the sampler and release the timer.
   */
  ~MatterButton() {
    end();
  }
  MatterButton(const MatterButton &) = delete;
  MatterButton &operator=(const MatterButton &) = delete;

  /**
   * @brief Start sampling @a pin.
   *
   * @c INPUT_PULLUP and active-low (BOOT) by default.
   *
   * @param pin GPIO pin (typically @c BOOT_PIN).
   * @param mode Pin mode; default @c INPUT_PULLUP.
   * @return true if the timer started, false otherwise.
   */
  bool begin(uint8_t pin, uint8_t mode = INPUT_PULLUP);
  /**
   * @brief Stop sampling and release the timer.
   */
  void end();

  /**
   * @brief Set debounce time in milliseconds (default 50).
   * @param ms Debounce duration.
   */
  void setDebounceMs(uint32_t ms);
  /**
   * @brief Set long-hold time in milliseconds (default 5000).
   * @param ms Hold duration before @c MATTER_BUTTON_LONG_HOLD.
   */
  void setLongHoldMs(uint32_t ms);
  /**
   * @brief Set double-click gap in milliseconds.
   *
   * 0 (default): emit @c MATTER_BUTTON_CLICK on release.
   * Non-zero: wait this long for a second click before emitting click or double-click.
   *
   * @param ms Gap duration, or 0 to disable double-click.
   */
  void setDoubleClickGapMs(uint32_t ms);
  /**
   * @brief Set pin sample period in milliseconds (default 20).
   * @param ms Poll period.
   */
  void setPollPeriodMs(uint32_t ms);
  /**
   * @brief Set whether a low GPIO level means pressed (BOOT is active-low).
   * @param activeLow true for active-low, false for active-high.
   */
  void setActiveLow(bool activeLow);

  /**
   * @brief Next queued event (up to 8), or @c MATTER_BUTTON_NONE.
   *
   * Drain from @c loop() only. Do not call Matter APIs from the timer callback.
   *
   * @return Queued event, or @c MATTER_BUTTON_NONE if empty.
   */
  matterButtonEvent_t poll();
  /**
   * @brief Whether the debounced pin is currently pressed.
   * @return true if pressed.
   */
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
