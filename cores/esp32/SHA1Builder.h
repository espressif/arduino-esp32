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

#ifndef SHA1Builder_h
#define SHA1Builder_h

#include <WString.h>
#include <Stream.h>

#include "HashBuilder.h"

#define SHA1_HASH_SIZE 20

class SHA1Builder : public HashBuilder {
private:
  uint32_t total[2];            /* number of bytes processed  */
  uint32_t state[5];            /* intermediate digest state  */
  unsigned char buffer[64];     /* data block being processed */
  uint8_t hash[SHA1_HASH_SIZE]; /* SHA-1 result               */

  void process(const uint8_t* data);

public:
  void begin() override;

  using HashBuilder::add;
  void add(const uint8_t* data, size_t len) override;

  using HashBuilder::addHexString;
  void addHexString(const char* data) override;

  bool addStream(Stream& stream, const size_t maxLen) override;
  void calculate() override;
  void getBytes(uint8_t* output) override;
  void getChars(char* output) override;
  String toString() override;
};

#endif
