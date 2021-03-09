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
 * FunctionalInterrupt.cpp
 *
 *  Created on: 8 jul. 2018
 *      Author: Herman
 */

#include "FunctionalInterrupt.h"
#include "Arduino.h"

typedef void (*voidFuncPtr)(void);
typedef void (*voidFuncPtrArg)(void*);

extern "C"
{
	extern void __attachInterruptFunctionalArg(uint8_t pin, voidFuncPtrArg userFunc, void * arg, int intr_type, bool functional);
}

void IRAM_ATTR interruptFunctional(void* arg)
{
    InterruptArgStructure* localArg = (InterruptArgStructure*)arg;
	if (localArg->interruptFunction)
	{
	  localArg->interruptFunction();
	}
}

void attachInterrupt(uint8_t pin, std::function<void(void)> intRoutine, int mode)
{
	// use the local interrupt routine which takes the ArgStructure as argument
	__attachInterruptFunctionalArg (pin, (voidFuncPtrArg)interruptFunctional, new InterruptArgStructure{intRoutine}, mode, true);
}

extern "C"
{
   void cleanupFunctional(void* arg)
   {
	 delete (InterruptArgStructure*)arg;
   }
}




