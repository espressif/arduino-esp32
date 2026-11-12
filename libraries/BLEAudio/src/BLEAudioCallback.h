/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <functional>
#include <utility>
#include "core/BLEMutex.h"

/**
 * @brief Role callback set from the application task and invoked from the audio host task.
 *
 * The callable is copied under a recursive mutex and invoked without holding
 * it, so a concurrent set()/clear() cannot race with the invocation and the
 * callback may call back into the role. Callbacks that return a value use
 * snapshot() so the caller can keep the default when none is set.
 */
template<typename Fn> class BLEAudioCallback {
public:
  BLEAudioCallback() = default;
  BLEAudioCallback(const BLEAudioCallback &) = delete;
  BLEAudioCallback &operator=(const BLEAudioCallback &) = delete;

  /** @brief Replace (or clear, with an empty function) the callback. */
  void set(Fn fn) {
    BLELockGuard lock(_mtx.get());
    _fn = std::move(fn);
  }

  /** @brief Assignment form, so role code can keep `cb = std::move(fn)`. */
  BLEAudioCallback &operator=(Fn fn) {
    set(std::move(fn));
    return *this;
  }

  void clear() {
    set(Fn());
  }

  /** @brief Whether a callback is set at this instant. */
  explicit operator bool() const {
    BLELockGuard lock(_mtx.get());
    return static_cast<bool>(_fn);
  }

  /**
   * @brief Invoke the callback, if any.
   *
   * The copy is taken under the lock; the call runs unlocked so the callback
   * can call back into the role without deadlocking.
   */
  template<typename... Args> void operator()(Args &&...args) const {
    Fn fn = snapshot();
    if (fn) {
      fn(std::forward<Args>(args)...);
    }
  }

  /** @brief Lock-protected copy for callbacks whose return value matters. */
  Fn snapshot() const {
    BLELockGuard lock(_mtx.get());
    return _fn;
  }

private:
  mutable BLESharedMutex _mtx;
  Fn _fn;
};
