#ifndef FUNCTIONALINTERRUPTS_H
#define FUNCTIONALINTERRUPTS_H

#include <stddef.h>
#include <stdint.h>
#include <functional>
#include "ScheduledFunctions.h"

// Structures for communication

struct InterruptInfo {
	InterruptInfo(uint8_t _pin) : pin(_pin) {}
	const uint8_t pin;
	uint8_t value = 0;
	uint32_t micro = 0;
};

struct FunctionInfo {
    std::function<void(void)> reqFunction = nullptr;
	std::function<void(InterruptInfo)> reqScheduledFunction = nullptr;
};

struct ArgStructure {
	~ArgStructure()
	{
		delete functionInfo;
		delete interruptInfo;
	}
	InterruptInfo* interruptInfo = nullptr;
	FunctionInfo* functionInfo = nullptr;
};

void attachInterrupt(uint8_t pin, std::function<void(void)> intRoutine, int mode);
void attachScheduledInterrupt(uint8_t pin, std::function<void(InterruptInfo)> scheduledIntRoutine, int mode);
void detachFunctionalInterrupt(uint8_t pin);

#endif //INTERRUPTS_H
