/* 
  Ticker.cpp - esp32 library that calls functions periodically

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

#include "Ticker.h"

RTOSTicker::RTOSTicker()
{
}


RTOSTicker::~RTOSTicker()
{
	if (_timerHandle)
	{
		xTimerDelete(_timerHandle, portMAX_DELAY);
	}
}

void RTOSTicker::attach(float seconds, _callbackFunction_t callback)
{
	_attach_ms(seconds*1000, true, callback);
}
void RTOSTicker::attach_ms(uint32_t milliseconds, _callbackFunction_t callback)
{
	_attach_ms(milliseconds, true, callback);
}
void RTOSTicker::once(float seconds, _callbackFunction_t callback)
{
	_attach_ms(seconds*1000, false, callback);
}
void RTOSTicker::once_ms(uint32_t milliseconds, _callbackFunction_t callback)
{
	_attach_ms(milliseconds, false, callback);
}

void RTOSTicker::_attach_ms(uint32_t milliseconds, bool repeat, _callbackFunction_t callback)
{
	_callbackFunction = callback;
	if (_timerHandle)
	{
		if (((timerState == REPEAT) && !repeat) || ((timerState == ONCE) && repeat))
		{
			xTimerDelete(_timerHandle, portMAX_DELAY);
			_timerHandle = nullptr;
			timerState = INITIAL;
		}
	}
	if (!_timerHandle)
	{
		_timerHandle = xTimerCreate
	            ( "RTOSTicker",
	              1, // will be updated on changeperiod
	              repeat, // once
	              this, // timerID is this to refer to object
	              internalCallback );
		timerState = repeat ? REPEAT : ONCE;
	}
	// changeperiod will also start timer
	xTimerChangePeriod( _timerHandle,
	                    milliseconds /  portTICK_PERIOD_MS,
						portMAX_DELAY );
}

void RTOSTicker::detach() {
  if (_timerHandle)
  {
	  xTimerStop(_timerHandle, portMAX_DELAY);
  }
}

void RTOSTicker::internalCallback(TimerHandle_t callbackTimer)
{
	if (callbackTimer)
	{
		auto callbackID = pvTimerGetTimerID(callbackTimer);
		if (callbackID)
		{
			((RTOSTicker*)callbackID)->_callbackFunction();
		}
	}
}

