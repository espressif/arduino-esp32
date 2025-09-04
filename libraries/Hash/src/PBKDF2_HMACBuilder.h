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

#ifndef PBKDF2_HMACBuilder_h
#define PBKDF2_HMACBuilder_h

#include <WString.h>
#include <Stream.h>
#include "HashBuilder.h"

class PBKDF2_HMACBuilder : public HashBuilder {
private:
  HashBuilder *hashBuilder;
  size_t hashSize;
  uint32_t iterations;

  // Password and salt storage
  uint8_t *password;
  size_t passwordLen;
  uint8_t *salt;
  size_t saltLen;

  // Output storage
  uint8_t *derivedKey;
  size_t derivedKeyLen;
  bool calculated;

  void hmac(const uint8_t *key, size_t keyLen, const uint8_t *data, size_t dataLen, uint8_t *output);
  void pbkdf2_hmac(const uint8_t *password, size_t passwordLen, const uint8_t *salt, size_t saltLen, uint32_t iterations, uint8_t *output, size_t outputLen);
  void clearData();

public:
  using HashBuilder::add;

  // Constructor takes a hash builder instance
  PBKDF2_HMACBuilder(HashBuilder *hash, String password = "", String salt = "", uint32_t iterations = 10000);
  ~PBKDF2_HMACBuilder();

  // Standard HashBuilder interface
  void begin() override;
  void add(const uint8_t *data, size_t len) override;
  bool addStream(Stream &stream, const size_t maxLen) override;
  void calculate() override;
  void getBytes(uint8_t *output) override;
  void getChars(char *output) override;
  String toString() override;
  size_t getHashSize() const override {
    return derivedKeyLen;
  }

  // PBKDF2 specific methods
  void setPassword(const uint8_t *password, size_t len);
  void setPassword(const char *password);
  void setPassword(String password);
  void setSalt(const uint8_t *salt, size_t len);
  void setSalt(const char *salt);
  void setSalt(String salt);
  void setIterations(uint32_t iterations);
  void setHashAlgorithm(HashBuilder *hash);
};

#endif
