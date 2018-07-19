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
	extern void __attachInterruptArg(uint8_t pin, voidFuncPtrArg userFunc, void * arg, int intr_type);
}

void interruptFunctional(void* arg)
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
	__attachInterruptArg (pin, (voidFuncPtrArg)interruptFunctional, new InterruptArgStructure{intRoutine}, mode);
}

extern "C"
{
   void cleanupFunctional(void* arg)
   {
	 delete (InterruptArgStructure*)arg;
   }
}




