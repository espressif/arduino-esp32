/*
 * BLExceptions.h
 *
 *  Created on: Nov 27, 2017
 *      Author: kolban
 */

#ifndef COMPONENTS_CPP_UTILS_BLEEXCEPTIONS_H_
#define COMPONENTS_CPP_UTILS_BLEEXCEPTIONS_H_
#include "soc/soc_caps.h"
#if SOC_BLE_SUPPORTED

#include "sdkconfig.h"

#if CONFIG_CXX_EXCEPTIONS != 1
#error "C++ exception handling must be enabled within make menuconfig. See Compiler Options > Enable C++ Exceptions."
#endif

#include <exception>


class BLEDisconnectedException : public std::exception {
	const char* what() const throw () {
		return "BLE Disconnected";
	}
};

class BLEUuidNotFoundException : public std::exception {
	const char* what() const throw () {
		return "No such UUID";
	}
};

#endif /* SOC_BLE_SUPPORTED */
#endif /* COMPONENTS_CPP_UTILS_BLEEXCEPTIONS_H_ */
