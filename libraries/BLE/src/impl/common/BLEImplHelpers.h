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

/**
 * @file
 * @brief Small shared impl-layer helpers: the PIMPL handle-guard macro
 *        (@c BLE_CHECK_IMPL) and the little-endian value-decode used by the
 *        remote read paths.
 */

/**
 * PIMPL null-guard for BLE handle classes -- the single sanctioned pattern for
 * accessing @c _impl inside a public method.
 *
 * A public handle owns @c std::shared_ptr<Impl> _impl, which is null only for a
 * default-constructed or moved-from handle. Every public method that touches the
 * implementation begins with this one macro: it early-returns on a null handle
 * and binds a reference named @c impl to the live implementation. Members are
 * then reached as @c impl.field -- with NO cast and NO hand-written
 * @c "auto &impl = *_impl;". For void methods pass no argument; for value-return
 * methods pass the value to return when the handle is invalid.
 *
 * Transparency: under the two-layer model the concrete
 * @c "struct BLEFoo::Impl : BLEFooImplCommon" is defined per backend, so
 * @c impl.field resolves both stack-agnostic base members (declared in
 * @c impl/common/BLEFooImpl.h) and backend-specific members (declared in
 * @c impl/<stack>/<Stack>BLEFoo.h) through the same reference with no
 * @c static_cast. That is what "just use impl" means -- the macro is the only
 * place @c *_impl is spelled in a public method.
 *
 * Usage tiers (keep access consistent):
 *   - In the public facade (the @c .cpp files directly under @c src), this macro
 *     is MANDATORY as the first statement of every method that touches @c _impl.
 *     Do not hand-write a null check or @c "auto &impl = *_impl;" there.
 *   - In backend translation units (under @c src/impl), prefer the macro too; a
 *     hand-written @c "if (!_impl)" + @c _impl-> is allowed ONLY when the null
 *     path must do extra work the macro cannot express (e.g. zeroing an
 *     out-parameter, or guarding on @c "!_impl || !_impl->x" together). Do not
 *     mix the macro and a manual @c *_impl alias in the same method.
 *   - For a trivial validity check, write @c "return _impl != nullptr;"
 *     directly (e.g. @c operator bool) rather than binding an unused @c impl.
 *
 * @code
 *   void BLEFoo::bar() {
 *     BLE_CHECK_IMPL();                 // returns void if _impl is null
 *     impl.field = 42;                  // common base member, cast-free
 *   }
 *
 *   BTStatus BLEFoo::baz() {
 *     BLE_CHECK_IMPL(BTStatus::InvalidState);
 *     impl.nimbleOnlyField = 1;         // backend-specific member, cast-free
 *     return BTStatus::OK;
 *   }
 * @endcode
 */
#define BLE_CHECK_IMPL(...) \
  if (!_impl)               \
    return __VA_ARGS__;     \
  auto &impl = *_impl

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Decode up to @p count little-endian octets into a @c uint32_t.
 *
 * GATT values are transmitted LSB-first; used by the remote characteristic and
 * descriptor numeric read helpers.
 *
 * @param data  Source bytes (LSB first).
 * @param len   Number of readable bytes at @p data.
 * @param count Octets to read (1..4).
 * @return The assembled value, or 0 if fewer than @p count bytes are available.
 */
inline uint32_t bleReadLEUint(const uint8_t *data, size_t len, size_t count) {
  if (len < count) {
    return 0;
  }
  uint32_t value = 0;
  for (size_t i = 0; i < count; i++) {
    value |= static_cast<uint32_t>(data[i]) << (8 * i);
  }
  return value;
}
