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

#ifndef SHA2Builder_h
#define SHA2Builder_h

#include <WString.h>
#include <Stream.h>

#include "HashBuilder.h"

// SHA2 constants
#define SHA2_224_HASH_SIZE 28
#define SHA2_256_HASH_SIZE 32
#define SHA2_384_HASH_SIZE 48
#define SHA2_512_HASH_SIZE 64

#define SHA2_224_BLOCK_SIZE 64
#define SHA2_256_BLOCK_SIZE 64
#define SHA2_384_BLOCK_SIZE 128
#define SHA2_512_BLOCK_SIZE 128

// SHA2 state sizes (in 32-bit words for SHA-224/256, 64-bit words for SHA-384/512)
#define SHA2_224_STATE_SIZE 8
#define SHA2_256_STATE_SIZE 8
#define SHA2_384_STATE_SIZE 8
#define SHA2_512_STATE_SIZE 8

class SHA2Builder : public HashBuilder {
protected:
  uint32_t state_32[8];   // SHA-224/256 state (256 bits)
  uint64_t state_64[8];   // SHA-384/512 state (512 bits)
  uint8_t buffer[128];    // Input buffer (max block size)
  size_t block_size;      // Block size
  size_t hash_size;       // Output hash size
  size_t buffer_size;     // Current buffer size
  bool finalized;         // Whether hash has been finalized
  bool is_sha512;         // Whether using SHA-512 family
  uint8_t hash[64];       // Hash result
  uint64_t total_length;  // Total length of input data

  void process_block_sha256(const uint8_t *data);
  void process_block_sha512(const uint8_t *data);
  void pad();

public:
  using HashBuilder::add;

  SHA2Builder(size_t hash_size = SHA2_256_HASH_SIZE);
  virtual ~SHA2Builder() {}

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

class SHA224Builder : public SHA2Builder {
public:
  SHA224Builder() : SHA2Builder(SHA2_224_HASH_SIZE) {}
};

class SHA256Builder : public SHA2Builder {
public:
  SHA256Builder() : SHA2Builder(SHA2_256_HASH_SIZE) {}
};

class SHA384Builder : public SHA2Builder {
public:
  SHA384Builder() : SHA2Builder(SHA2_384_HASH_SIZE) {}
};

class SHA512Builder : public SHA2Builder {
public:
  SHA512Builder() : SHA2Builder(SHA2_512_HASH_SIZE) {}
};

#endif
