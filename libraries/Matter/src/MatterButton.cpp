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

#include <MatterButton.h>

void MatterButton::timerCb(void *arg) {
  static_cast<MatterButton *>(arg)->sample();
}

bool MatterButton::readPressed() const {
  const int level = digitalRead(_pin);
  return _activeLow ? (level == LOW) : (level == HIGH);
}

bool MatterButton::begin(uint8_t pin, uint8_t mode) {
  end();
  _pin = pin;
  pinMode(_pin, mode);
  const bool pressed = readPressed();
  _rawCandidate = pressed;
  _stablePressed = pressed;
  _longEmitted = false;
  _clickPending = false;
  _rawChangeMs = millis();
  _pressStartMs = _rawChangeMs;
  _qHead = 0;
  _qTail = 0;
  if (!startTimer()) {
    return false;
  }
  _started = true;
  return true;
}

void MatterButton::end() {
  stopTimer();
  _started = false;
}

void MatterButton::setDebounceMs(uint32_t ms) {
  _debounceMs = ms;
}

void MatterButton::setLongHoldMs(uint32_t ms) {
  _longHoldMs = ms == 0 ? 1 : ms;
}

void MatterButton::setDoubleClickGapMs(uint32_t ms) {
  _doubleClickGapMs = ms;
}

void MatterButton::setPollPeriodMs(uint32_t ms) {
  _pollPeriodMs = ms < 5 ? 5 : ms;
  if (_started) {
    stopTimer();
    startTimer();
  }
}

void MatterButton::setActiveLow(bool activeLow) {
  _activeLow = activeLow;
}

bool MatterButton::startTimer() {
  if (_timer != nullptr) {
    return esp_timer_start_periodic(_timer, (uint64_t)_pollPeriodMs * 1000) == ESP_OK;
  }
  esp_timer_create_args_t args = {};
  args.callback = &MatterButton::timerCb;
  args.arg = this;
  args.dispatch_method = ESP_TIMER_TASK;
  args.name = "matter_btn";
  if (esp_timer_create(&args, &_timer) != ESP_OK) {
    _timer = nullptr;
    return false;
  }
  if (esp_timer_start_periodic(_timer, (uint64_t)_pollPeriodMs * 1000) != ESP_OK) {
    esp_timer_delete(_timer);
    _timer = nullptr;
    return false;
  }
  return true;
}

void MatterButton::stopTimer() {
  if (_timer == nullptr) {
    return;
  }
  esp_timer_stop(_timer);
  esp_timer_delete(_timer);
  _timer = nullptr;
}

void MatterButton::pushEvent(matterButtonEvent_t event) {
  portENTER_CRITICAL(&_mux);
  const uint8_t next = (uint8_t)((_qHead + 1) % kQueueSize);
  if (next != _qTail) {
    _queue[_qHead] = (uint8_t)event;
    _qHead = next;
  }
  portEXIT_CRITICAL(&_mux);
}

bool MatterButton::isPressed() const {
  portENTER_CRITICAL(&_mux);
  const bool pressed = _stablePressed;
  portEXIT_CRITICAL(&_mux);
  return pressed;
}

matterButtonEvent_t MatterButton::poll() {
  matterButtonEvent_t event = MATTER_BUTTON_NONE;
  portENTER_CRITICAL(&_mux);
  if (_qTail != _qHead) {
    event = (matterButtonEvent_t)_queue[_qTail];
    _qTail = (uint8_t)((_qTail + 1) % kQueueSize);
  }
  portEXIT_CRITICAL(&_mux);
  return event;
}

void MatterButton::sample() {
  const uint32_t now = millis();
  const bool pressed = readPressed();

  if (pressed != _rawCandidate) {
    _rawCandidate = pressed;
    _rawChangeMs = now;
  } else if ((uint32_t)(now - _rawChangeMs) >= _debounceMs && pressed != _stablePressed) {
    portENTER_CRITICAL(&_mux);
    _stablePressed = pressed;
    portEXIT_CRITICAL(&_mux);
    if (_stablePressed) {
      _pressStartMs = now;
      _longEmitted = false;
      pushEvent(MATTER_BUTTON_PRESS);
    } else if (!_longEmitted) {
      if (_doubleClickGapMs == 0) {
        pushEvent(MATTER_BUTTON_CLICK);
      } else if (_clickPending && (uint32_t)(now - _clickAtMs) <= _doubleClickGapMs) {
        _clickPending = false;
        pushEvent(MATTER_BUTTON_DOUBLE_CLICK);
      } else {
        _clickPending = true;
        _clickAtMs = now;
      }
    }
  }

  if (_stablePressed && !_longEmitted && _longHoldMs > 0 && (uint32_t)(now - _pressStartMs) >= _longHoldMs) {
    _longEmitted = true;
    _clickPending = false;
    pushEvent(MATTER_BUTTON_LONG_HOLD);
  }

  if (_clickPending && !_stablePressed && (uint32_t)(now - _clickAtMs) >= _doubleClickGapMs) {
    _clickPending = false;
    pushEvent(MATTER_BUTTON_CLICK);
  }
}
