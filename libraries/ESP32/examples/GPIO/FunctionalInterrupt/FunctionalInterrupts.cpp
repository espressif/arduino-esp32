#include "FunctionalInterrupts.h"
#include "Schedule.h"
#include "Arduino.h"

#if defined(ESP8266)

// Duplicate typedefs from core_esp8266_wiring_digital.cpp
// Keep in sync
typedef void (*voidFuncPtr)(void);
typedef void (*voidFuncPtrArg)(void*);

typedef struct {
	uint8_t mode;
	voidFuncPtr fn;
	void* arg;
} interrupt_handler_t;

// Helper functions for Functional interrupt routines
extern "C" interrupt_handler_t* __getInterruptHandler(uint8_t pin);

#elif defined(ESP32)

// Duplicate typedefs from esp32-hal-gpio.c
// Keep in sync
typedef void (*voidFuncPtr)(void);
typedef void (*voidFuncPtrArg)(void*);
typedef struct {
	voidFuncPtr fn;
	void* arg;
} InterruptHandle_t;

// Helper functions for Functional interrupt routines
extern "C" InterruptHandle_t* __getInterruptHandler(uint8_t pin);

#endif

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
		schedule_function(std::bind(localArg->functionInfo->reqScheduledFunction,InterruptInfo(*(localArg->interruptInfo))));
	}
	if (localArg->functionInfo->reqFunction)
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

#if defined(ESP8266)
	interrupt_handler_t* handler = __getInterruptHandler(pin);
#elif defined(ESP32)
	InterruptHandle_t* handler = __getInterruptHandler(pin);
#endif
	if (handler->arg)
	{
		cleanupFunctional(handler->arg);
	}

	FunctionInfo* fi = new FunctionInfo;
	fi->reqFunction = intRoutine;

	ArgStructure* as = new ArgStructure;
	as->functionInfo = fi;

	::attachInterruptArg (pin, static_cast<voidFuncPtrArg>(interruptFunctional), as, mode);
}

void attachScheduledInterrupt(uint8_t pin, std::function<void(InterruptInfo)> scheduledIntRoutine, int mode)
{
#if defined(ESP8266)
	interrupt_handler_t* handler = __getInterruptHandler(pin);
#elif defined(ESP32)
	InterruptHandle_t* handler = __getInterruptHandler(pin);
#endif
	if (handler->arg)
	{
		cleanupFunctional(handler->arg);
	}

	InterruptInfo* ii = new InterruptInfo(pin);

	FunctionInfo* fi = new FunctionInfo;
	fi->reqScheduledFunction = scheduledIntRoutine;

	ArgStructure* as = new ArgStructure;
	as->interruptInfo = ii;
	as->functionInfo = fi;

	::attachInterruptArg (pin, static_cast<voidFuncPtrArg>(interruptFunctional), as, mode);
}

void detachFunctionalInterrupt(uint8_t pin)
{
#if defined(ESP8266)
	interrupt_handler_t* handler = __getInterruptHandler(pin);
#elif defined(ESP32)
	InterruptHandle_t* handler = __getInterruptHandler(pin);
#endif
	if (handler->arg)
	{
		cleanupFunctional(handler->arg);
	}
	::detachInterrupt (pin);
}

