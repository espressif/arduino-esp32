#ifndef BOARD_USES_HW_GPIO_NUMBERS

#include "Arduino.h"

static const int8_t TO_GPIO_NUMBER[NUM_DIGITAL_PINS] = {
    [D0]        = 44, // RX
    [D1]        = 43, // TX
    [D2]        = 5,
    [D3]        = 6,  // CTS
    [D4]        = 7,  // DSR
    [D5]        = 8,
    [D6]        = 9,
    [D7]        = 10,
    [D8]        = 17,
    [D9]        = 18,
    [D10]       = 21, // SS
    [D11]       = 38, // MOSI
    [D12]       = 47, // MISO
    [D13]       = 48, // SCK, LED_BUILTIN
    [LED_RED]   = 46,
    [LED_GREEN] = 0,
    [LED_BLUE]  = 45, // RTS
    [A0]        = 1,  // DTR
    [A1]        = 2,
    [A2]        = 3,
    [A3]        = 4,
    [A4]        = 11, // SDA
    [A5]        = 12, // SCL
    [A6]        = 13,
    [A7]        = 14,
};

int8_t digitalPinToGPIONumber(int8_t digitalPin)
{
    if ((digitalPin < 0) || (digitalPin >= NUM_DIGITAL_PINS))
        return -1;
    return TO_GPIO_NUMBER[digitalPin];
}

int8_t gpioNumberToDigitalPin(int8_t gpioNumber)
{
    if (gpioNumber < 0)
        return -1;

    // slow linear table lookup
    for (int8_t digitalPin = 0; digitalPin < NUM_DIGITAL_PINS; ++digitalPin) {
        if (TO_GPIO_NUMBER[digitalPin] == gpioNumber)
            return digitalPin;
    }

    // not found
    return -1;
}

#endif
