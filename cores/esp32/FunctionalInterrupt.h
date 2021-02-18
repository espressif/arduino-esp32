// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
/*
 * FunctionalInterrupt.h
 *
 *  Created on: 8 jul. 2018
 *      Author: Herman
 */

#ifndef CORE_CORE_FUNCTIONALINTERRUPT_H_
#define CORE_CORE_FUNCTIONALINTERRUPT_H_

#include <functional>

struct InterruptArgStructure {
	std::function<void(void)> interruptFunction;
};

void attachInterrupt(uint8_t pin, std::function<void(void)> intRoutine, int mode);


#endif /* CORE_CORE_FUNCTIONALINTERRUPT_H_ */
