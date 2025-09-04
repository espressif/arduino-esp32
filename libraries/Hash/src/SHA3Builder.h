// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
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

#ifndef SHA3Builder_h
#define SHA3Builder_h

#include <WString.h>
#include <Stream.h>

#include "HashBuilder.h"

// SHA3 constants
#define SHA3_224_HASH_SIZE 28
#define SHA3_256_HASH_SIZE 32
#define SHA3_384_HASH_SIZE 48
#define SHA3_512_HASH_SIZE 64

#define SHA3_224_RATE 144
#define SHA3_256_RATE 136
#define SHA3_384_RATE 104
#define SHA3_512_RATE 72

#define SHA3_STATE_SIZE 200  // 1600 bits = 200 bytes

class SHA3Builder : public HashBuilder {
protected:
  uint64_t state[25];   // SHA3 state (1600 bits)
  uint8_t buffer[200];  // Input buffer
  size_t rate;          // Rate (block size)
  size_t hash_size;     // Output hash size
  size_t buffer_size;   // Current buffer size
  bool finalized;       // Whether hash has been finalized
  uint8_t hash[64];     // Hash result

  void keccak_f(uint64_t state[25]);
  void process_block(const uint8_t *data);
  void pad();

public:
  using HashBuilder::add;

  SHA3Builder(size_t hash_size = SHA3_256_HASH_SIZE);
  virtual ~SHA3Builder() {}

  void begin() override;
  void add(const uint8_t *data, size_t len) override;
  bool addStream(Stream &stream, const size_t maxLen) override;
  void calculate() override;
  void getBytes(uint8_t *output) override;
  void getChars(char *output) override;
  String toString() override;

  size_t getHashSize() const override {
    return hash_size;
  }
};

class SHA3_224Builder : public SHA3Builder {
public:
  SHA3_224Builder() : SHA3Builder(SHA3_224_HASH_SIZE) {}
};

class SHA3_256Builder : public SHA3Builder {
public:
  SHA3_256Builder() : SHA3Builder(SHA3_256_HASH_SIZE) {}
};

class SHA3_384Builder : public SHA3Builder {
public:
  SHA3_384Builder() : SHA3Builder(SHA3_384_HASH_SIZE) {}
};

class SHA3_512Builder : public SHA3Builder {
public:
  SHA3_512Builder() : SHA3Builder(SHA3_512_HASH_SIZE) {}
};

#endif
