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

#include <Arduino.h>
#include "PBKDF2_HMACBuilder.h"
#include "HMACBuilder.h"

PBKDF2_HMACBuilder::PBKDF2_HMACBuilder(HashBuilder *hash, String password, String salt, uint32_t iterations) {
  this->hashBuilder = hash;
  if (hash) {
    this->hashSize = hash->getHashSize();
  } else {
    this->hashSize = 0;
    log_w("PBKDF2_HMACBuilder: No hash algorithm provided. Use setHashAlgorithm() before calculate().");
  }
  this->iterations = iterations;

  // Initialize pointers
  this->password = nullptr;
  this->salt = nullptr;
  this->passwordLen = 0;
  this->saltLen = 0;
  this->derivedKey = nullptr;
  this->derivedKeyLen = 0;
  this->calculated = false;

  if (password.length() > 0) {
    setPassword(password);
  }

  if (salt.length() > 0) {
    setSalt(salt);
  }
}

PBKDF2_HMACBuilder::~PBKDF2_HMACBuilder() {
  clearData();
}

void PBKDF2_HMACBuilder::clearData() {
  if (derivedKey != nullptr) {
    forced_memzero(derivedKey, derivedKeyLen);
    delete[] derivedKey;
    derivedKey = nullptr;
  }
  derivedKeyLen = 0;
  calculated = false;
}

// HashBuilder interface methods
void PBKDF2_HMACBuilder::begin() {
  clearData();
}

void PBKDF2_HMACBuilder::add(const uint8_t *data, size_t len) {
  log_w("PBKDF2_HMACBuilder::add sets only the password. Use setPassword() and setSalt() instead.");
  setPassword(data, len);
}

bool PBKDF2_HMACBuilder::addStream(Stream &stream, const size_t maxLen) {
  log_e("PBKDF2_HMACBuilder does not support addStream. Use setPassword() and setSalt() instead.");
  (void)stream;
  (void)maxLen;
  return false;
}

void PBKDF2_HMACBuilder::calculate() {
  if (password == nullptr || salt == nullptr) {
    log_e("Error: Password or salt not set.");
    return;
  }

  // Set default output size to hash size if not specified
  if (derivedKeyLen == 0) {
    derivedKeyLen = hashSize;
  }

  // Allocate output buffer
  if (derivedKey != nullptr) {
    forced_memzero(derivedKey, derivedKeyLen);
    delete[] derivedKey;
  }
  derivedKey = new uint8_t[derivedKeyLen];

  // Perform PBKDF2-HMAC
  pbkdf2_hmac(password, passwordLen, salt, saltLen, iterations, derivedKey, derivedKeyLen);
  calculated = true;
}

void PBKDF2_HMACBuilder::getBytes(uint8_t *output) {
  if (!calculated || derivedKey == nullptr) {
    log_e("Error: PBKDF2-HMAC not calculated or no output buffer provided.");
    return;
  }
  memcpy(output, derivedKey, derivedKeyLen);
}

void PBKDF2_HMACBuilder::getChars(char *output) {
  if (!calculated || derivedKey == nullptr) {
    log_e("Error: PBKDF2-HMAC not calculated or no output buffer provided.");
    return;
  }

  bytes2hex(output, derivedKeyLen * 2 + 1, derivedKey, derivedKeyLen);
}

String PBKDF2_HMACBuilder::toString() {
  if (!calculated || derivedKey == nullptr) {
    log_e("Error: PBKDF2-HMAC not calculated or no output buffer provided.");
    return "";
  }

  char out[(derivedKeyLen * 2) + 1];
  getChars(out);
  return String(out);
}

// PBKDF2 specific methods
void PBKDF2_HMACBuilder::setPassword(const uint8_t *password, size_t len) {
  if (this->password != nullptr) {
    forced_memzero(this->password, len);
    delete[] this->password;
  }
  this->password = new uint8_t[len];
  memcpy(this->password, password, len);
  this->passwordLen = len;
  calculated = false;
}

void PBKDF2_HMACBuilder::setPassword(const char *password) {
  setPassword((const uint8_t *)password, strlen(password));
}

void PBKDF2_HMACBuilder::setPassword(String password) {
  setPassword((const uint8_t *)password.c_str(), password.length());
}

void PBKDF2_HMACBuilder::setSalt(const uint8_t *salt, size_t len) {
  if (this->salt != nullptr) {
    forced_memzero(this->salt, len);
    delete[] this->salt;
  }
  this->salt = new uint8_t[len];
  memcpy(this->salt, salt, len);
  this->saltLen = len;
  calculated = false;
}

void PBKDF2_HMACBuilder::setSalt(const char *salt) {
  setSalt((const uint8_t *)salt, strlen(salt));
}

void PBKDF2_HMACBuilder::setSalt(String salt) {
  setSalt((const uint8_t *)salt.c_str(), salt.length());
}

void PBKDF2_HMACBuilder::setIterations(uint32_t iterations) {
  this->iterations = iterations;
}

void PBKDF2_HMACBuilder::setHashAlgorithm(HashBuilder *hash) {
  hashBuilder = hash;
  if (hash) {
    hashSize = hash->getHashSize();
  } else {
    hashSize = 0;
    log_e("PBKDF2_HMACBuilder: Hash algorithm set to null.");
  }
}

void PBKDF2_HMACBuilder::pbkdf2_hmac(
  const uint8_t *password, size_t passwordLen, const uint8_t *salt, size_t saltLen, uint32_t iterations, uint8_t *output, size_t outputLen
) {
  HMACBuilder hmac(hashBuilder);
  hmac.setKey(password, passwordLen);
  if (!hmac.getBlockSize() || !hmac.getHashSize()) {
    log_e("PBKDF2_HMACBuilder: HMAC is not usable (hash algorithm or block size missing).");
    return;
  }

  uint8_t u1[64];  // Large enough for any hash
  uint8_t u2[64];
  uint8_t block[64];
  uint8_t blockIndex[4];

  size_t blocks = (outputLen + hashSize - 1) / hashSize;

  for (size_t i = 1; i <= blocks; i++) {
    // Prepare salt || INT(i)
    blockIndex[0] = (i >> 24) & 0xFF;
    blockIndex[1] = (i >> 16) & 0xFF;
    blockIndex[2] = (i >> 8) & 0xFF;
    blockIndex[3] = i & 0xFF;

    // U1 = HMAC(password, salt || INT(i))
    hmac.begin();
    hmac.add(salt, saltLen);
    hmac.add(blockIndex, sizeof(blockIndex));
    hmac.calculate();
    hmac.getBytes(u1);
    memcpy(block, u1, hashSize);

    // U2 = HMAC(password, U1)
    for (uint32_t j = 1; j < iterations; j++) {
      hmac.begin();
      hmac.add(u1, hashSize);
      hmac.calculate();
      hmac.getBytes(u2);
      memcpy(u1, u2, hashSize);

      // XOR with previous result
      for (size_t k = 0; k < hashSize; k++) {
        block[k] ^= u1[k];
      }
    }

    // Copy block to output
    size_t copyLen = (i == blocks) ? (outputLen - (i - 1) * hashSize) : hashSize;
    memcpy(output + (i - 1) * hashSize, block, copyLen);
  }

  forced_memzero(u1, sizeof(u1));
  forced_memzero(u2, sizeof(u2));
  forced_memzero(block, sizeof(block));
}
