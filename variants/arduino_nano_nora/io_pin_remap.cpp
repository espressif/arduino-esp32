#if defined(BOARD_HAS_PIN_REMAP) && !defined(ARDUINO_CORE_BUILD)
// -DARDUINO_CORE_BUILD must be set for core files only, to avoid extra
// remapping steps that would create all sorts of issues in the core.
// Removing -DBOARD_HAS_PIN_REMAP at least does correctly restore the
// use of GPIO numbers in the API.
#error This build system is not supported. Please rebuild without BOARD_HAS_PIN_REMAP.
#endif

#if !defined(BOARD_HAS_PIN_REMAP)
// This board uses pin mapping but the build system has disabled it
#warning The build system forces the Arduino API to use GPIO numbers on a board that has custom pin mapping.
#elif defined(BOARD_USES_HW_GPIO_NUMBERS)
// The user has chosen to disable pin mappin.
#warning The Arduino API will use GPIO numbers for this build.
#endif

#if defined(BOARD_HAS_PIN_REMAP) && !defined(BOARD_USES_HW_GPIO_NUMBERS)

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
