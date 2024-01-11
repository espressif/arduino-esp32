/*
Cpu automatic LightSleep
=====================================
This code displays how to use automatic light sleep with an active WiFi connection
and tune WiFi modem sleep modes

This code is under Public Domain License.

Hardware Connections (optional)
======================
Use an ammeter/scope connected in series with a CPU/DevKit board to measure power consumption

Author:
Taras Shcherban <shcherban91@gmail.com>
*/

#include "Arduino.h"

#include <WiFi.h>
#include <HTTPClient.h>

const char *wifi_ssid = "kriegsmaschine";
const char *wifi_password = "ap3NhxtcmszTdok36ijka";

void doHttpRequest();

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        ; // wait for serial port to connect

    // CPU will automatically go into light sleep if no ongoing activity (active task, peripheral activity etc.)
    setAutomaticLightSleep(true);

    WiFi.begin(wifi_ssid, wifi_password);

    // Additionally to automatic CPU sleep a modem can also be setup for a power saving.
    // If a WiFi is active - selected modem sleep mode will determine how much CPU will be sleeping.
    // There are two functions available:setSleep(mode) and setSleep(mode, listenInterval)
    // mode - supports one of three values:
    //     * WIFI_PS_NONE - No power save
    //     * WIFI_PS_MIN_MODEM - Minimum modem power saving. In this mode, station wakes up to receive beacon every DTIM period
    //     * WIFI_PS_MAX_MODEM - Maximum modem power saving. In this mode, interval to receive beacons is determined by the listenInterval parameter
    // listenInterval - effective only with a WIFI_PS_MAX_MODEM mode. determines how often will modem (and consequently CPU) wakeup to catch WiFi beacon packets.
    //     The larger the interval - the less power needed for WiFi connection support. However that comes at a cost of increased networking latency, i.e.
    //     If your device responds to some external requests (web-server, ping etc.) - larger listenInterval means device takes more to respond.
    //     Reasonable range is 2..9, going larger would not benefit too much in terms of power consumption. Too large value might result in WiFi connection drop.
    //     listenInterval is measured in DTIM intervals, which is determined by your access point (typically ~100ms).
    WiFi.setSleep(WIFI_PS_MAX_MODEM, 4);
}

void loop()
{
    doHttpRequest();

    // Serial output is being suspended during sleeping, so without a Flush call logs
    // will be printed to the terminal with a delay depending on how much CPU spends in a sleep state
    Serial.flush();

    // This is a sleep-aware waiting using delay(). Blocking in this manner
    // allows CPU to go into light sleep mode until there is some task to do.
    // if you remove that delay completely - CPU will have to call loop() function constantly,
    // so no power saving will be available
    delay(5000);
}

// simulate some job - make an HTTP GET request
void doHttpRequest()
{
    if (!WiFi.isConnected())
        return;

    HTTPClient http;

    Serial.print("[HTTP] request...\n");
    http.begin("http://example.com/index.html");

    int httpCode = http.GET();
    if (httpCode > 0)
    {
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);

        if (httpCode == HTTP_CODE_OK)
        {
            Serial.printf("[HTTP] GET... payload size: %d\n", http.getSize());
        }
    }
    else
    {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
}
