/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2017 Neil Kolban
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * BLExceptions.h
 *
 *  Created on: Nov 27, 2017
 *      Author: kolban
 */

#ifndef COMPONENTS_CPP_UTILS_BLEEXCEPTIONS_H_
#define COMPONENTS_CPP_UTILS_BLEEXCEPTIONS_H_

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)

#if CONFIG_CXX_EXCEPTIONS != 1
#error "C++ exception handling must be enabled within make menuconfig. See Compiler Options > Enable C++ Exceptions."
#endif

#include <exception>

class BLEDisconnectedException : public std::exception {
  const char *what() const throw() {
    return "BLE Disconnected";
  }
};

class BLEUuidNotFoundException : public std::exception {
  const char *what() const throw() {
    return "No such UUID";
  }
};

#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */

#endif /* COMPONENTS_CPP_UTILS_BLEEXCEPTIONS_H_ */
