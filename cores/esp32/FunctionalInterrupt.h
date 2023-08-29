/*
 * FunctionalInterrupt.h
 *
 *  Created on: 8 jul. 2018
 *      Author: Herman
 */

#ifndef CORE_CORE_FUNCTIONALINTERRUPT_H_
#define CORE_CORE_FUNCTIONALINTERRUPT_H_

#include <functional>
#include <stdint.h>

struct InterruptArgStructure {
    std::function<void(void)> interruptFunction;
};

// The extra set of parentheses here prevents macros defined
// in io_pin_remap.h from applying to this declaration.
void (attachInterrupt)(uint8_t pin, std::function<void(void)> intRoutine, int mode);

#endif /* CORE_CORE_FUNCTIONALINTERRUPT_H_ */
