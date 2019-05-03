#include "FunctionalInterrupts.h"
#include <Schedule.h>
#include <Arduino.h>

void ICACHE_RAM_ATTR interruptFunctional(void* arg)
{
	ArgStructure* localArg = static_cast<ArgStructure*>(arg);
	if (localArg->interruptInfo)
	{
		localArg->interruptInfo->value = digitalRead(localArg->interruptInfo->pin);
		localArg->interruptInfo->micro = micros();
	}
	if (localArg->functionInfo->reqScheduledFunction)
	{
		schedule_function(
			[reqScheduledFunction = localArg->functionInfo->reqScheduledFunction,
				interruptInfo = *localArg->interruptInfo]() { reqScheduledFunction(interruptInfo); });
	}
	else if (localArg->functionInfo->reqFunction)
	{
	  localArg->functionInfo->reqFunction();
	}
}

void cleanupFunctional(void* arg)
{
	ArgStructure* localArg = static_cast<ArgStructure*>(arg);
	delete localArg;
}

void attachInterrupt(uint8_t pin, std::function<void(void)> intRoutine, int mode)
{
	// use the local interrupt routine which takes the ArgStructure as argument

	void* localArg = detachInterruptArg(pin);
	if (localArg) cleanupFunctional(localArg);

	FunctionInfo* fi = new FunctionInfo;
	fi->reqFunction = intRoutine;

	ArgStructure* as = new ArgStructure;
	as->functionInfo = fi;

	attachInterruptArg (pin, interruptFunctional, as, mode);
}

void attachScheduledInterrupt(uint8_t pin, std::function<void(InterruptInfo)> scheduledIntRoutine, int mode)
{
	void* localArg = detachInterruptArg(pin);
	if (localArg) cleanupFunctional(localArg);

	InterruptInfo* ii = new InterruptInfo(pin);

	FunctionInfo* fi = new FunctionInfo;
	fi->reqScheduledFunction = scheduledIntRoutine;

	ArgStructure* as = new ArgStructure;
	as->interruptInfo = ii;
	as->functionInfo = fi;

	attachInterruptArg(pin, interruptFunctional, as, mode);
}

void detachFunctionalInterrupt(uint8_t pin)
{
	void* localArg = detachInterruptArg(pin);
	if (localArg) cleanupFunctional(localArg);
}
