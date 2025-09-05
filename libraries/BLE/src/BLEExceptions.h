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
