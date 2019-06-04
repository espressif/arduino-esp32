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

#ifndef TICKER_H
#define TICKER_H

extern "C" {
#include "esp_timer.h"
}
#include <functional>
#include <Schedule.h>

class Ticker
{
public:
	Ticker();
	~Ticker();

	typedef void (*callback_with_arg_t)(void*);
	typedef std::function<void(void)> callback_function_t;

	void attach(float seconds, callback_function_t callback)
	{
		_callback_function = std::move(callback);
		_attach_s(seconds, true, _static_callback, this);
	}

	void attach_ms(uint32_t milliseconds, callback_function_t callback)
	{
		_callback_function = std::move(callback);
		_attach_ms(milliseconds, true, _static_callback, this);
	}

	void attach_us_scheduled(uint32_t micros, callback_function_t callback)
	{
		attach_us(micros, [callback]() { schedule_function(callback); });
	}

	void attach_us(uint32_t micros, callback_function_t callback)
	{
		_callback_function = std::move(callback);
		_attach_us(micros, true, _static_callback, this);
	}

	template<typename TArg>
	void attach(float seconds, void (*callback)(TArg), TArg arg)
	{
		static_assert(sizeof(TArg) <= sizeof(void*), "attach() callback argument size must be <= sizeof(void*)");
		// C-cast serves two purposes:
		// static_cast for smaller integer types,
		// reinterpret_cast + const_cast for pointer types
		_attach_s(seconds, true, reinterpret_cast<callback_with_arg_t>(callback), (void*)arg);
	}

	template<typename TArg>
	void attach_ms(uint32_t milliseconds, void (*callback)(TArg), TArg arg)
	{
		static_assert(sizeof(TArg) <= sizeof(void*), "attach() callback argument size must be <= sizeof(void*)");
		_attach_ms(milliseconds, true, reinterpret_cast<callback_with_arg_t>(callback), (void*)arg);
	}

	template<typename TArg>
	void attach_us(uint32_t micros, void (*callback)(TArg), TArg arg)
	{
		static_assert(sizeof(TArg) <= sizeof(void*), "attach() callback argument size must be <= sizeof(void*)");
		_attach_us(micros, true, reinterpret_cast<callback_with_arg_t>(callback), (void*)arg);
	}

	void once(float seconds, callback_function_t callback)
	{
		_callback_function = std::move(callback);
		_attach_s(seconds, false, _static_callback, this);
	}

	void once_ms(uint32_t milliseconds, callback_function_t callback)
	{
		_callback_function = std::move(callback);
		_attach_ms(milliseconds, false, _static_callback, this);
	}

	void once_us_scheduled(uint32_t micros, callback_function_t callback)
	{
		once_us(micros, [callback]() { schedule_function(callback); });
	}

	void once_us(uint32_t micros, callback_function_t callback)
	{
		_callback_function = std::move(callback);
		_attach_us(micros, false, _static_callback, this);
	}

	template<typename TArg>
	void once(float seconds, void (*callback)(TArg), TArg arg)
	{
		static_assert(sizeof(TArg) <= sizeof(void*), "attach() callback argument size must be <= sizeof(void*)");
		_attach_s(seconds, false, reinterpret_cast<callback_with_arg_t>(callback), (void*)arg);
	}

	template<typename TArg>
	void once_ms(uint32_t milliseconds, void (*callback)(TArg), TArg arg)
	{
		static_assert(sizeof(TArg) <= sizeof(void*), "attach() callback argument size must be <= sizeof(void*)");
		_attach_ms(milliseconds, false, reinterpret_cast<callback_with_arg_t>(callback), (void*)arg);
	}

	template<typename TArg>
	void once_us(uint32_t micros, void (*callback)(TArg), TArg arg)
	{
		static_assert(sizeof(TArg) <= sizeof(void*), "attach() callback argument size must be <= sizeof(void*)");
		_attach_us(micros, false, reinterpret_cast<callback_with_arg_t>(callback), (void*)arg);
	}

	void detach();
	bool active() const;

protected:
	void _attach_ms(uint32_t milliseconds, bool repeat, callback_with_arg_t callback, void* arg);
	static void _static_callback(void* arg);

	callback_function_t _callback_function = nullptr;

	esp_timer_handle_t _timer;

private:
	void _attach_us(uint32_t micros, bool repeat, callback_with_arg_t callback, void* arg);
	void _attach_s(float seconds, bool repeat, callback_with_arg_t callback, void* arg);
};


#endif//TICKER_H
