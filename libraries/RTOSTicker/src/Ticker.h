/* 
  Ticker.h - esp32 library that calls functions periodically

  Copyright (c) 2017 Bert Melis. All rights reserved.
  
  Based on the original work of:
  Copyright (c) 2014 Ivan Grokhotkov. All rights reserved.
  The original version is part of the esp8266 core for Arduino environment.

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

#ifndef RTOSTICKER_H
#define RTOSTICKER_H

#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"



class RTOSTicker
{
public:
  typedef std::function<void(void)> _callbackFunction_t;

  RTOSTicker();
  ~RTOSTicker();

  void attach(float seconds, _callbackFunction_t callback);
  void attach_ms(uint32_t milliseconds, _callbackFunction_t callback);
  void once(float seconds, _callbackFunction_t callback);
  void once_ms(uint32_t milliseconds, _callbackFunction_t callback);

  void detach();
  bool active();

protected:	

  enum TimerState
  {
	  INITIAL,
	  REPEAT,
	  ONCE
  };

  TimerHandle_t _timerHandle = nullptr;
  _callbackFunction_t _callbackFunction = nullptr;
  TimerState timerState =  INITIAL;

  void _attach_ms(uint32_t milliseconds, bool repeat, _callbackFunction_t callback);

  static void internalCallback(TimerHandle_t callbackTimer);

};


#endif  // RTOSTICKER_H
