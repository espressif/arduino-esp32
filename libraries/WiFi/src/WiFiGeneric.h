/*
 ESP8266WiFiGeneric.h - esp8266 Wifi support.
 Based on WiFi.h from Ardiono WiFi shield library.
 Copyright (c) 2011-2014 Arduino.  All right reserved.
 Modified by Ivan Grokhotkov, December 2014
 Reworked by Markus Sattler, December 2015

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef ESP32WIFIGENERIC_H_
#define ESP32WIFIGENERIC_H_

#include <esp_err.h>
#include <esp_event_loop.h>
#include <functional>
#include "WiFiType.h"

typedef void (*WiFiEventCb)(system_event_id_t event);
typedef std::function<void(system_event_id_t event, system_event_info_t info)> WiFiEventFuncCb;
typedef void (*WiFiEventSysCb)(system_event_t *event);

typedef size_t wifi_event_id_t;

class WiFiGenericClass
{
  public:
    WiFiGenericClass();

    wifi_event_id_t onEvent(WiFiEventCb cbEvent, system_event_id_t event = SYSTEM_EVENT_MAX);
    wifi_event_id_t onEvent(WiFiEventFuncCb cbEvent, system_event_id_t event = SYSTEM_EVENT_MAX);
    wifi_event_id_t onEvent(WiFiEventSysCb cbEvent, system_event_id_t event = SYSTEM_EVENT_MAX);
    void removeEvent(WiFiEventCb cbEvent, system_event_id_t event = SYSTEM_EVENT_MAX);
    void removeEvent(WiFiEventSysCb cbEvent, system_event_id_t event = SYSTEM_EVENT_MAX);
    void removeEvent(wifi_event_id_t id);

    int32_t channel(void);

    void persistent(bool persistent);

    static bool mode(wifi_mode_t);
    static wifi_mode_t getMode();

    bool enableSTA(bool enable);
    bool enableAP(bool enable);

    static esp_err_t _eventCallback(void *arg, system_event_t *event);

  protected:
    static bool _persistent;
    static wifi_mode_t _forceSleepLastMode;

  public:
    int hostByName(const char *aHostname, IPAddress &aResult);

  protected:
    friend class WiFiSTAClass;
    friend class WiFiScanClass;
    friend class WiFiAPClass;
};

#endif /* ESP32WIFIGENERIC_H_ */
