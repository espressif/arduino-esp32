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

#include <Arduino.h>
#include "HMACBuilder.h"

HMACBuilder::HMACBuilder(HashBuilder *hash, size_t blockSize)
  : hashBuilder(nullptr), hashSize(0), blockSize(0), keySet(false), innerStarted(false), calculated(false) {
  memset(ipad, 0, sizeof(ipad));
  memset(opad, 0, sizeof(opad));
  memset(this->hash, 0, sizeof(this->hash));
  setHashAlgorithm(hash, blockSize);
}

HMACBuilder::~HMACBuilder() {
  forced_memzero(ipad, sizeof(ipad));
  forced_memzero(opad, sizeof(opad));
  forced_memzero(hash, sizeof(hash));
}

void HMACBuilder::setHashAlgorithm(HashBuilder *hash, size_t newBlockSize) {
  hashBuilder = hash;
  hashSize = 0;
  blockSize = 0;

  if (hash) {
    hashSize = hash->getHashSize();
    blockSize = newBlockSize ? newBlockSize : hash->getBlockSize();
  }

  if (hash && !blockSize) {
    log_e("HMACBuilder: hash block size is unknown. Pass it explicitly or use a HashBuilder that implements getBlockSize().");
  }
  if (blockSize > HMAC_MAX_BLOCK_SIZE) {
    log_e("HMACBuilder: block size %u exceeds HMAC_MAX_BLOCK_SIZE %u", (unsigned)blockSize, (unsigned)HMAC_MAX_BLOCK_SIZE);
    blockSize = 0;
  }
  if (hashSize > sizeof(this->hash)) {
    log_e("HMACBuilder: hash size %u exceeds buffer", (unsigned)hashSize);
    hashSize = 0;
  }
  keySet = false;
  innerStarted = false;
  calculated = false;
}

void HMACBuilder::setKey(const uint8_t *key, size_t len) {
  if (!hashBuilder || !blockSize || !hashSize || key == nullptr) {
    log_e("HMACBuilder: hash algorithm or key not set");
    keySet = false;
    innerStarted = false;
    calculated = false;
    forced_memzero(ipad, sizeof(ipad));
    forced_memzero(opad, sizeof(opad));
    forced_memzero(hash, sizeof(hash));
    return;
  }

  uint8_t keyPad[HMAC_MAX_BLOCK_SIZE];
  memset(keyPad, 0, blockSize);

  if (len > blockSize) {
    hashBuilder->begin();
    hashBuilder->add(key, len);
    hashBuilder->calculate();
    hashBuilder->getBytes(keyPad);
  } else {
    memcpy(keyPad, key, len);
  }

  for (size_t i = 0; i < blockSize; i++) {
    ipad[i] = keyPad[i] ^ 0x36;
    opad[i] = keyPad[i] ^ 0x5c;
  }
  forced_memzero(keyPad, sizeof(keyPad));

  keySet = true;
  innerStarted = false;
  calculated = false;
}

void HMACBuilder::setKey(const char *key) {
  if (key == nullptr) {
    setKey((const uint8_t *)nullptr, 0);
    return;
  }
  setKey((const uint8_t *)key, strlen(key));
}

void HMACBuilder::setKey(const String &key) {
  setKey((const uint8_t *)key.c_str(), key.length());
}

void HMACBuilder::begin() {
  calculated = false;
  innerStarted = false;
  memset(hash, 0, sizeof(hash));

  if (!hashBuilder || !keySet) {
    log_e("HMACBuilder: hash algorithm or key not set");
    return;
  }

  hashBuilder->begin();
  hashBuilder->add(ipad, blockSize);
  innerStarted = true;
}

void HMACBuilder::add(const uint8_t *data, size_t len) {
  if (!innerStarted || data == nullptr || len == 0) {
    return;
  }
  hashBuilder->add(data, len);
}

bool HMACBuilder::addStream(Stream &stream, const size_t maxLen) {
  const size_t buf_size = 512;
  size_t maxLengthLeft = maxLen;
  uint8_t *buf = (uint8_t *)malloc(buf_size);

  if (!buf) {
    return false;
  }

  int bytesAvailable = stream.available();
  while ((bytesAvailable > 0) && (maxLengthLeft > 0)) {
    size_t readBytes = (size_t)bytesAvailable;
    if (readBytes > maxLengthLeft) {
      readBytes = maxLengthLeft;
    }
    if (readBytes > buf_size) {
      readBytes = buf_size;
    }

    size_t numBytesRead = stream.readBytes(buf, readBytes);
    if (numBytesRead < 1) {
      free(buf);
      return false;
    }

    add(buf, numBytesRead);

    maxLengthLeft -= numBytesRead;
    bytesAvailable = stream.available();
  }
  free(buf);
  return true;
}

void HMACBuilder::calculate() {
  if (!innerStarted) {
    log_e("HMACBuilder: calculate() called before begin()");
    return;
  }

  uint8_t innerHash[64];
  hashBuilder->calculate();
  hashBuilder->getBytes(innerHash);

  hashBuilder->begin();
  hashBuilder->add(opad, blockSize);
  hashBuilder->add(innerHash, hashSize);
  hashBuilder->calculate();
  hashBuilder->getBytes(hash);
  forced_memzero(innerHash, sizeof(innerHash));

  calculated = true;
  innerStarted = false;
}

void HMACBuilder::getBytes(uint8_t *output) {
  if (!calculated || output == nullptr) {
    log_e("Error: HMAC not calculated or no output buffer provided.");
    return;
  }
  memcpy(output, hash, hashSize);
}

void HMACBuilder::getChars(char *output) {
  if (!calculated || output == nullptr) {
    log_e("Error: HMAC not calculated or no output buffer provided.");
    return;
  }
  bytes2hex(output, hashSize * 2 + 1, hash, hashSize);
}

String HMACBuilder::toString() {
  if (!calculated) {
    log_e("Error: HMAC not calculated.");
    return "";
  }
  char out[(hashSize * 2) + 1];
  getChars(out);
  return String(out);
}
