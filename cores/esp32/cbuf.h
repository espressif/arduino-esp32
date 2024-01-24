/*
 cbuf.h - Circular buffer implementation
 Copyright (c) 2014 Ivan Grokhotkov. All rights reserved.
 This file is part of the esp8266 core for Arduino environment.

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

#ifndef __cbuf_h
#define __cbuf_h

#include <stddef.h>
#include <stdint.h>
#include <string.h>

class cbuf
{
public:
    cbuf(size_t size);
    ~cbuf();

    size_t resizeAdd(size_t addSize);
    size_t resize(size_t newSize);
    size_t available() const;
    size_t size();

    size_t room() const;

    inline bool empty() const
    {
        return _begin == _end;
    }

    inline bool full() const
    {
        return wrap_if_bufend(_end + 1) == _begin;
    }

    int peek();
    size_t peek(char *dst, size_t size);

    int read();
    size_t read(char* dst, size_t size);

    size_t write(char c);
    size_t write(const char* src, size_t size);

    void flush();
    size_t remove(size_t size);

    cbuf *next;

protected:
    inline char* wrap_if_bufend(char* ptr) const
    {
        return (ptr == _bufend) ? _buf : ptr;
    }

    size_t _size;
    char* _buf;
    const char* _bufend;
    char* _begin;
    char* _end;

};

#endif//__cbuf_h
