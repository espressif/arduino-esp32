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

#include <stdint.h>

/**
 * @brief Unified Bluetooth status/error type for BLE and BT Classic.
 *
 * Wraps a compact error code with an Arduino-natural boolean conversion:
 * `explicit operator bool()` returns true on success (OK == 0).
 *
 * Usage:
 * @code
 * if (BLE.begin("MyDevice")) { Serial.println("OK"); }
 *
 * BTStatus s = client.connect(addr);
 * if (!s) { Serial.printf("Failed: %s\n", s.toString()); }
 * if (s == BTStatus::Timeout) { Serial.println("Timed out"); }
 * @endcode
 */
class BTStatus {
public:
  enum Code : int8_t {
    OK = 0,
    Fail = -1,
    NotInitialized = -2,
    InvalidParam = -3,
    InvalidState = -4,
    NoMemory = -5,
    Timeout = -6,
    NotConnected = -7,
    AlreadyConnected = -8,
    NotFound = -9,
    AuthFailed = -10,
    PermissionDenied = -11,
    Busy = -12,
    NotSupported = -13,
  };

  BTStatus() : _code(OK) {}
  BTStatus(Code c) : _code(c) {}

  /** @brief Returns true when status is OK (Arduino convention: success = true). */
  explicit operator bool() const {
    return _code == OK;
  }
  bool operator!() const {
    return _code != OK;
  }

  /**
   * @brief Explicit conversion to a plain `int`.
   *
   * Lets `static_cast<int>(status)` and `int(status)` produce the underlying
   * numeric code — useful for logging (`%d`) or for interop with APIs that
   * expect an integer. The conversion is `explicit` so it can't accidentally
   * fire in arithmetic or conditional contexts; use `.code()` if you want the
   * strongly-typed enum, or `.toString()` for a human-readable message.
   */
  explicit operator int() const {
    return static_cast<int>(_code);
  }

  bool operator==(const BTStatus &o) const {
    return _code == o._code;
  }
  bool operator!=(const BTStatus &o) const {
    return _code != o._code;
  }

  /** @brief Returns the underlying error code. */
  Code code() const {
    return _code;
  }

  /** @brief Returns a human-readable string for the status code. */
  const char *toString() const;

private:
  Code _code;
};

static_assert(sizeof(BTStatus) == 1, "BTStatus must be 1 byte");
