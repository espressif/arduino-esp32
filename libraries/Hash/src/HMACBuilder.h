// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OF CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef HMACBuilder_h
#define HMACBuilder_h

#include <WString.h>
#include <Stream.h>
#include "HashBuilder.h"

// Large enough for SHA-1/SHA-2 (64 or 128) and SHA-3-224's rate (144).
#ifndef HMAC_MAX_BLOCK_SIZE
#define HMAC_MAX_BLOCK_SIZE 144
#endif

class HMACBuilder : public HashBuilder {
private:
  HashBuilder *hashBuilder;
  size_t hashSize;
  size_t blockSize;
  uint8_t ipad[HMAC_MAX_BLOCK_SIZE];
  uint8_t opad[HMAC_MAX_BLOCK_SIZE];
  uint8_t hash[64];
  bool keySet;
  bool innerStarted;
  bool calculated;

public:
  using HashBuilder::add;

  // `hash` is not owned. `blockSize` of 0 uses hash->getBlockSize().
  // Construction fails (block size 0) if the hash does not report one.
  HMACBuilder(HashBuilder *hash = nullptr, size_t blockSize = 0);
  ~HMACBuilder();

  void setHashAlgorithm(HashBuilder *hash, size_t blockSize = 0);
  void setKey(const uint8_t *key, size_t len);
  void setKey(const char *key);
  void setKey(const String &key);

  void begin() override;
  void add(const uint8_t *data, size_t len) override;
  bool addStream(Stream &stream, const size_t maxLen) override;
  void calculate() override;
  void getBytes(uint8_t *output) override;
  void getChars(char *output) override;
  String toString() override;

  size_t getHashSize() const override {
    return hashSize;
  }
  size_t getBlockSize() const override {
    return blockSize;
  }
};

#endif
