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
 * PIMPL null-guard macro for BLE handle classes.
 *
 * Combines the null check and a local reference alias in one line.
 * For void-return methods pass no argument; for value-return methods
 * pass the early-return value.
 *
 * @code
 *   void BLEFoo::bar() {
 *     BLE_CHECK_IMPL();          // returns void if _impl is null
 *     impl.field = 42;           // use impl. instead of _impl->
 *   }
 *
 *   BTStatus BLEFoo::baz() {
 *     BLE_CHECK_IMPL(BTStatus::InvalidState);
 *     impl.field = 42;
 *     return BTStatus::OK;
 *   }
 * @endcode
 */
#define BLE_CHECK_IMPL(...) \
  if (!_impl)               \
    return __VA_ARGS__;     \
  auto &impl = *_impl
