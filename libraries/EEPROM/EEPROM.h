/* 
   EEPROM.h -ported by Paolo Becchi to Esp32 

   use a one sector flash partition defined in partition table
  
  from esp8266 EEPROM 

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

#ifndef EEPROM_h
#define EEPROM_h
#ifndef EEPROM_FLASH_PARTITION_NAME
 #define EEPROM_FLASH_PARTITION_NAME "eeprom"
#endif
extern "C" {

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <esp_partition.h>
}

//
//   need to define a flash partition for EEPROM  with above name
//
//           eeprom , data , 0x99, start address, 0x1000
//
class EEPROMClass {
public:
  EEPROMClass(uint32_t sector);
  EEPROMClass(void);

  bool begin(size_t size);
  uint8_t read(int address);
  void write(int address, uint8_t val);
  bool commit();
  void end();

  uint8_t * getDataPtr();

  template<typename T> 
  T &get(int address, T &t) {
    if (address < 0 || address + sizeof(T) > _size)
      return t;

    memcpy((uint8_t*) &t, _data + address, sizeof(T));
    return t;
  }

  template<typename T> 
  const T &put(int address, const T &t) {
    if (address < 0 || address + sizeof(T) > _size)
      return t;

    memcpy(_data + address, (const uint8_t*) &t, sizeof(T));
    _dirty = true;
    return t;
  }

protected:
  uint32_t _sector;
  uint8_t* _data;
  size_t _size;
  bool _dirty;
  const esp_partition_t * _mypart;
};

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_EEPROM)
extern EEPROMClass EEPROM;
#endif

#endif

