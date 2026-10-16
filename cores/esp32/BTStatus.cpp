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

#include "BTStatus.h"

const char *BTStatus::toString() const {
  switch (_code) {
    case OK:               return "OK";
    case Fail:             return "Fail";
    case NotInitialized:   return "NotInitialized";
    case InvalidParam:     return "InvalidParam";
    case InvalidState:     return "InvalidState";
    case NoMemory:         return "NoMemory";
    case Timeout:          return "Timeout";
    case NotConnected:     return "NotConnected";
    case AlreadyConnected: return "AlreadyConnected";
    case NotFound:         return "NotFound";
    case AuthFailed:       return "AuthFailed";
    case PermissionDenied: return "PermissionDenied";
    case Busy:             return "Busy";
    case NotSupported:     return "NotSupported";
    default:               return "Unknown";
  }
}
