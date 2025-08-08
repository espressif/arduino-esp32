// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef HashBuilder_h
#define HashBuilder_h

#include <WString.h>
#include <Stream.h>

#include "HEXBuilder.h"

// Base class for hash builders

class HashBuilder : public HEXBuilder {
public:
  virtual ~HashBuilder() {}
  virtual void begin() = 0;

  virtual void add(const uint8_t *data, size_t len) = 0;
  void add(const char *data);
  void add(String data);

  void addHexString(const char *data);
  void addHexString(String data);

  virtual bool addStream(Stream &stream, const size_t maxLen) = 0;
  virtual void calculate() = 0;
  virtual void getBytes(uint8_t *output) = 0;
  virtual void getChars(char *output) = 0;
  virtual String toString() = 0;
  virtual size_t getHashSize() const = 0;
};

#endif
