/*
Gpio Interrupts with sleep mode
=====================================
This code displays how to use automatic light sleep with a GPIO interrupts.

With a light sleep mode the only available interrupts are ONLOW_WE and ONHIGH_WE.
RISING/FALLING/CHANGE/ONLOW/ONHIGH would be not fired.
Keep in mind that the interrupt ONLOW_WE/ONLOW would be fired repetitively as long as
the input is held in a LOW state, so a single button press by a fraction of second can
easily trigger your interrupt handler a few hundreds times.
The same is valid for ONHIGH_WE/ONHIGH and HIGH level.
To catch every button press only once - we are going to change the interrupt level
(from ONHIGH_WE to ONLOW_WE and vice versa).
Since ONHIGH_WE interrupt handler is detached right on the first execution - it can be
also treated as a RISING interrupt handler.
The same way ONLOW_WE can be treated as a FALLING interrupt handler.
If CHANGE interrupt is needed - just put your logic in both ONHIGH_WE and ONLOW_WE handlers.

This code is under Public Domain License.

Hardware Connections
======================
A button from IO10 to ground (or a jumper wire to mimic that button).
Optionally - an ammeter/scope connected in series with a CPU/DevKit board to measure power consumption.

Author:
Taras Shcherban <shcherban91@gmail.com>
*/

#include "Arduino.h"
#include <atomic>

std::atomic_int interruptsCounter = 0;

#define BTN_INPUT 10

void lowIsrHandler();

void IRAM_ATTR highIsrHandler()
{
    // button was released - attach button press interrupt back
    attachInterrupt(BTN_INPUT, lowIsrHandler, ONLOW_WE);
}

void IRAM_ATTR lowIsrHandler()
{
    // button is pressed - count it
    interruptsCounter++;

    // attach interrupt to catch an event of button releasing
    // implicitly detaches previous interrupt and stops this function from being called
    // while the input is held in a LOW state
    attachInterrupt(BTN_INPUT, highIsrHandler, ONHIGH_WE);
}

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        ; // wait for serial port to connect

    // CPU will automatically go into light sleep if no ongoing activity (active task, peripheral activity etc.)
    setAutomaticLightSleep(true);

    pinMode(BTN_INPUT, INPUT_PULLUP);
    attachInterrupt(BTN_INPUT, lowIsrHandler, ONLOW_WE);

    // this function is required for GPIO to be able to wakeup CPU from a lightSleep mode
    esp_sleep_enable_gpio_wakeup();
}

void loop()
{
    Serial.printf("Button press count: %d\n", (int)interruptsCounter);

    // Serial output is being suspended during sleeping, so without a Flush call logs
    // will be printed to the terminal with a delay depending on how much CPU spends in a sleep state
    Serial.flush();

    // This is a sleep-aware waiting using delay(). Blocking in this manner
    // allows CPU to go into light sleep mode until there is some task to do.
    // if you remove that delay completely - CPU will have to call loop() function constantly,
    // so no power saving will be available
    delay(5000);
}
