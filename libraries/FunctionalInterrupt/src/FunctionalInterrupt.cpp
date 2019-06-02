#include "FunctionalInterrupt.h"
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
    if (localArg->scheduledFunction)
    {
        schedule_function(
            [scheduledFunction = localArg->scheduledFunction,
                               interruptInfo = *localArg->interruptInfo]()
        {
            scheduledFunction(interruptInfo);
        });
    }
}

void cleanupFunctional(void* arg)
{
    ArgStructure* localArg = static_cast<ArgStructure*>(arg);
    delete localArg;
}

void attachScheduledInterrupt(uint8_t pin, std::function<void(InterruptInfo)> scheduledIntRoutine, int mode)
{
    void* localArg = detachInterruptArg(pin);
    if (localArg)
    {
        cleanupFunctional(localArg);
    }

    ArgStructure* as = new ArgStructure;
    as->interruptInfo = new InterruptInfo(pin);
    as->scheduledFunction = scheduledIntRoutine;

    attachInterruptArg(pin, interruptFunctional, as, mode);
}

void detachFunctionalInterrupt(uint8_t pin)
{
    void* localArg = detachInterruptArg(pin);
    if (localArg)
    {
        cleanupFunctional(localArg);
    }
}
