/*
  EEPROM.cpp -ported by Paolo Becchi to Esp32  
  Op
  from esp8266 EEPROM emulation

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

#include "Arduino.h"
#include "EEPROM.h"

#include <esp_log.h>

static const char* TAG = "eeprom";

EEPROMClass::EEPROMClass(uint32_t sector)
: _sector(sector)
, _data(0)
, _size(0)
, _dirty(false)
{
}

EEPROMClass::EEPROMClass(void)
	: _sector(0)// (((uint32_t)&_SPIFFS_end - 0x40200000) / SPI_FLASH_SEC_SIZE))
, _data(0)
, _size(0)
, _dirty(false)
{
}

bool EEPROMClass::begin(size_t size) {
  if (size <= 0) {
    return false;
  }
  if (size > SPI_FLASH_SEC_SIZE) {
    size = SPI_FLASH_SEC_SIZE;
  }
  _mypart = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_ANY, EEPROM_FLASH_PARTITION_NAME);
  if (_mypart == NULL) {
    return false;
  }
  size = (size + 3) & (~3);

  if (_data) {
    delete[] _data;
  }

  _data = new uint8_t[size];
  _size = size;	
  bool ret = false;
  if (esp_partition_read (_mypart,0, (void *) _data,_size)==ESP_OK) {
    ret=true;
  }
  
  return ret;
}

void EEPROMClass::end() {
  if (!_size) {
    return;
  }

  commit();
  if (_data) {
    delete[] _data;
  }
  _data = 0;
  _size = 0;
}

uint8_t EEPROMClass::read(int address) {
  if (address < 0 || (size_t)address >= _size) {
    return 0;
  }
  if (!_data) {
    return 0;
  }

  return _data[address];
}

void EEPROMClass::write(int address, uint8_t value) {
  if (address < 0 || (size_t)address >= _size)
    return;
  if(!_data)
    return;

  // Optimise _dirty. Only flagged if data written is different.
  uint8_t* pData = &_data[address];
  if (*pData != value)
  {
    *pData = value;
    _dirty = true;
  }
}

bool EEPROMClass::commit() {
  bool ret = false;
  if (!_size)
    return false;
  if (!_dirty)
    return true;
  if (!_data)
    return false;


  if (esp_partition_erase_range(_mypart, 0, SPI_FLASH_SEC_SIZE) != ESP_OK)
  {	  
     log_e( "partition erase err.");
  }
  else
  {
     if (esp_partition_write(_mypart, 0, (void *)_data, _size) == ESP_ERR_INVALID_SIZE)
     {
	log_e( "error in Write");
     }
     else
     {
	_dirty = false;
	ret = true;
     }
  }

  return ret;
}

uint8_t * EEPROMClass::getDataPtr() {
  _dirty = true;
  return &_data[0];
}

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_EEPROM)
EEPROMClass EEPROM;
#endif
