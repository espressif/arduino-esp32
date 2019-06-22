#include "FunctionalInterrupt.h"
#include <Schedule.h>
#include <Arduino.h>

namespace {

    struct ArgStructure
    {
        std::function<void()> function = nullptr;
    };

    void ICACHE_RAM_ATTR interruptFunctional(void* arg)
    {
        ArgStructure* localArg = static_cast<ArgStructure*>(arg);
        localArg->function();
    }

    void cleanupFunctional(void* arg)
    {
        ArgStructure* localArg = static_cast<ArgStructure*>(arg);
        delete localArg;
    }

}

void attachInterrupt(uint8_t pin, std::function<void(void)> intRoutine, int mode)
{
    void* localArg = detachInterruptArg(pin);
    if (localArg)
    {
        cleanupFunctional(localArg);
    }

    if (intRoutine)
    {
        ArgStructure* arg = new ArgStructure;
        arg->function = std::move(intRoutine);

        attachInterruptArg(pin, interruptFunctional, arg, mode);
    }
}

void attachScheduledInterrupt(uint8_t pin, std::function<void(InterruptInfo)> scheduledIntRoutine, int mode)
{
    void* localArg = detachInterruptArg(pin);
    if (localArg)
    {
        cleanupFunctional(localArg);
    }

    if (scheduledIntRoutine)
    {
        ArgStructure* arg = new ArgStructure;
        arg->function = [scheduledIntRoutine = std::move(scheduledIntRoutine), pin]()
        {
            InterruptInfo interruptInfo(pin);
            interruptInfo.value = digitalRead(pin);
            interruptInfo.micro = micros();
            schedule_function([scheduledIntRoutine, interruptInfo]() { scheduledIntRoutine(std::move(interruptInfo)); });
        };

        attachInterruptArg(pin, interruptFunctional, arg, mode);
    }
}

void detachFunctionalInterrupt(uint8_t pin)
{
    void* localArg = detachInterruptArg(pin);
    if (localArg)
    {
        cleanupFunctional(localArg);
    }
}
