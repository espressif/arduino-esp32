#ifndef FUNCTIONALINTERRUPT_H
#define FUNCTIONALINTERRUPT_H

#include <functional>

// Structures for communication

struct InterruptInfo
{
    InterruptInfo(uint8_t _pin) : pin(_pin) {}
    const uint8_t pin;
    uint8_t value = 0;
    uint32_t micro = 0;
};

void attachInterrupt(uint8_t pin, std::function<void(void)> intRoutine, int mode);
void attachScheduledInterrupt(uint8_t pin, std::function<void(InterruptInfo)> scheduledIntRoutine, int mode);
void detachFunctionalInterrupt(uint8_t pin);

#endif //FUNCTIONALINTERRUPT_H
