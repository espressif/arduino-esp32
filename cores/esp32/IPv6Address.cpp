/*
 IPv6Address.cpp - Base class that provides IPv6Address
 Copyright (c) 2011 Adrian McEwen.  All right reserved.

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

#include <Arduino.h>
#include <IPv6Address.h>
#include <Print.h>

IPv6Address::IPv6Address()
{
    memset(_address.bytes, 0, sizeof(_address.bytes));
}

IPv6Address::IPv6Address(const uint8_t *address)
{
    memcpy(_address.bytes, address, sizeof(_address.bytes));
}

IPv6Address::IPv6Address(const uint32_t *address)
{
    memcpy(_address.bytes, (const uint8_t *)address, sizeof(_address.bytes));
}

IPv6Address& IPv6Address::operator=(const uint8_t *address)
{
    memcpy(_address.bytes, address, sizeof(_address.bytes));
    return *this;
}

bool IPv6Address::operator==(const uint8_t* addr) const
{
    return memcmp(addr, _address.bytes, sizeof(_address.bytes)) == 0;
}

size_t IPv6Address::printTo(Print& p) const
{
    size_t n = 0;
    for(int i = 0; i < 16; i+=2) {
        if(i){
            n += p.print(':');
        }
        n += p.printf("%02x", _address.bytes[i]);
        n += p.printf("%02x", _address.bytes[i+1]);

    }
    return n;
}

String IPv6Address::toString() const
{
    char szRet[40];
    sprintf(szRet,"%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
            _address.bytes[0], _address.bytes[1], _address.bytes[2], _address.bytes[3],
            _address.bytes[4], _address.bytes[5], _address.bytes[6], _address.bytes[7],
            _address.bytes[8], _address.bytes[9], _address.bytes[10], _address.bytes[11],
            _address.bytes[12], _address.bytes[13], _address.bytes[14], _address.bytes[15]);
    return String(szRet);
}

bool IPv6Address::fromString(const char *address)
{
    //format 0011:2233:4455:6677:8899:aabb:ccdd:eeff
    if(strlen(address) != 39){
        return false;
    }
    char * pos = (char *)address;
    size_t i = 0;
    for(i = 0; i < 16; i+=2) {
        if(!sscanf(pos, "%2hhx", &_address.bytes[i]) || !sscanf(pos+2, "%2hhx", &_address.bytes[i+1])){
            return false;
        }
        pos += 5;
    }
    return true;
}
