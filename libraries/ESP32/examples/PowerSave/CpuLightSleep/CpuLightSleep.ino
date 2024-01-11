/*
Cpu automatic LightSleep
=====================================
This code displays how to use automatic light sleep
and demonstrates a difference between busywait and
sleep-aware wait using delay().
Another examples of a 'sleep-friendly' blocking might be
FreeRTOS mutexes/semaphores, queues.

This code is under Public Domain License.

Hardware Connections (optional)
======================
Use an ammeter/scope connected in series with a CPU/DevKit board to measure power consumption

Author:
Taras Shcherban <shcherban91@gmail.com>
*/

#include "Arduino.h"

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        ; // wait for serial port to connect

    // CPU will automatically go into light sleep if no ongoing activity (active task, peripheral activity etc.)
    setAutomaticLightSleep(true);
}

void loop()
{
    Serial.printf("Time since boot: %ld ms; going to block for a 5 seconds...\n", millis());

    // Serial output is being suspended during sleeping, so without a Flush call logs
    // will be printed to the terminal with a delay depending on how much CPU spends in a sleep state
    Serial.flush();

    // This is a sleep-aware waiting using delay(). Blocking in this manner
    // allows CPU to go into light sleep mode until there is some task to do.
    // if you remove that delay completely - CPU will have to call loop() function constantly,
    // so no power saving will be available
    delay(5000);
}
