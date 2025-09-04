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

#include <algorithm>

#include "esp32-hal-log.h"
#include "SHA3Builder.h"

// Keccak round constants
static const uint64_t keccak_round_constants[24] = {0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808AULL, 0x8000000080008000ULL,
                                                    0x000000000000808BULL, 0x0000000080000001ULL, 0x8000000080008081ULL, 0x8000000000008009ULL,
                                                    0x000000000000008AULL, 0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000AULL,
                                                    0x000000008000808BULL, 0x800000000000008BULL, 0x8000000000008089ULL, 0x8000000000008003ULL,
                                                    0x8000000000008002ULL, 0x8000000000000080ULL, 0x000000000000800AULL, 0x800000008000000AULL,
                                                    0x8000000080008081ULL, 0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL};

// Rho rotation constants
static const uint32_t rho[6] = {0x3f022425, 0x1c143a09, 0x2c3d3615, 0x27191713, 0x312b382e, 0x3e030832};

// Pi permutation constants
static const uint32_t pi[6] = {0x110b070a, 0x10050312, 0x04181508, 0x0d13170f, 0x0e14020c, 0x01060916};

// Macros for bit manipulation
#define ROTR64(x, y) (((x) << (64U - (y))) | ((x) >> (y)))

// Keccak-f permutation
void SHA3Builder::keccak_f(uint64_t state[25]) {
  uint64_t lane[5];
  uint64_t *s = state;
  int i;

  for (int round = 0; round < 24; round++) {
    uint64_t t;

    // Theta step
    for (i = 0; i < 5; i++) {
      lane[i] = s[i] ^ s[i + 5] ^ s[i + 10] ^ s[i + 15] ^ s[i + 20];
    }
    for (i = 0; i < 5; i++) {
      t = lane[(i + 4) % 5] ^ ROTR64(lane[(i + 1) % 5], 63);
      s[i] ^= t;
      s[i + 5] ^= t;
      s[i + 10] ^= t;
      s[i + 15] ^= t;
      s[i + 20] ^= t;
    }

    // Rho step
    for (i = 1; i < 25; i += 4) {
      uint32_t r = rho[(i - 1) >> 2];
      for (int j = i; j < i + 4; j++) {
        uint8_t r8 = (uint8_t)(r >> 24);
        r <<= 8;
        s[j] = ROTR64(s[j], r8);
      }
    }

    // Pi step
    t = s[1];
    for (i = 0; i < 24; i += 4) {
      uint32_t p = pi[i >> 2];
      for (unsigned j = 0; j < 4; j++) {
        uint64_t tmp = s[p & 0xff];
        s[p & 0xff] = t;
        t = tmp;
        p >>= 8;
      }
    }

    // Chi step
    for (i = 0; i <= 20; i += 5) {
      lane[0] = s[i];
      lane[1] = s[i + 1];
      lane[2] = s[i + 2];
      lane[3] = s[i + 3];
      lane[4] = s[i + 4];
      s[i + 0] ^= (~lane[1]) & lane[2];
      s[i + 1] ^= (~lane[2]) & lane[3];
      s[i + 2] ^= (~lane[3]) & lane[4];
      s[i + 3] ^= (~lane[4]) & lane[0];
      s[i + 4] ^= (~lane[0]) & lane[1];
    }

    // Iota step
    s[0] ^= keccak_round_constants[round];
  }
}

// Process a block of data
void SHA3Builder::process_block(const uint8_t *data) {
  // XOR the data into the state using byte-level operations
  for (size_t i = 0; i < rate; i++) {
    size_t state_idx = i >> 3;           // i / 8
    size_t bit_offset = (i & 0x7) << 3;  // (i % 8) * 8
    uint64_t byte_val = (uint64_t)data[i] << bit_offset;
    state[state_idx] ^= byte_val;
  }

  // Apply Keccak-f permutation
  keccak_f(state);
}

// Pad the input according to SHA3 specification
void SHA3Builder::pad() {
  // Clear the buffer first
  memset(buffer + buffer_size, 0, rate - buffer_size);

  // Add the domain separator (0x06) at the current position
  buffer[buffer_size] = 0x06;

  // Set the last byte to indicate the end (0x80)
  buffer[rate - 1] = 0x80;
}

// Constructor
SHA3Builder::SHA3Builder(size_t hash_size) : hash_size(hash_size), buffer_size(0), finalized(false) {
  // Calculate rate based on hash size
  if (hash_size == SHA3_224_HASH_SIZE) {
    rate = SHA3_224_RATE;
  } else if (hash_size == SHA3_256_HASH_SIZE) {
    rate = SHA3_256_RATE;
  } else if (hash_size == SHA3_384_HASH_SIZE) {
    rate = SHA3_384_RATE;
  } else if (hash_size == SHA3_512_HASH_SIZE) {
    rate = SHA3_512_RATE;
  } else {
    log_e("Invalid hash size: %d", hash_size);
    rate = 0;  // Invalid hash size
  }
}

// Initialize the hash computation
void SHA3Builder::begin() {
  // Clear the state
  memset(state, 0, sizeof(state));
  memset(buffer, 0, sizeof(buffer));
  buffer_size = 0;
  finalized = false;
}

// Add data to the hash computation
void SHA3Builder::add(const uint8_t *data, size_t len) {
  if (finalized || len == 0) {
    return;
  }

  size_t offset = 0;

  // Process any buffered data first
  if (buffer_size > 0) {
    size_t to_copy = std::min(len, rate - buffer_size);
    memcpy(buffer + buffer_size, data, to_copy);
    buffer_size += to_copy;
    offset += to_copy;

    if (buffer_size == rate) {
      process_block(buffer);
      buffer_size = 0;
    }
  }

  // Process full blocks
  while (offset + rate <= len) {
    process_block(data + offset);
    offset += rate;
  }

  // Buffer remaining data
  if (offset < len) {
    memcpy(buffer, data + offset, len - offset);
    buffer_size = len - offset;
  }
}

// Add data from a stream
bool SHA3Builder::addStream(Stream &stream, const size_t maxLen) {
  const int buf_size = 512;
  int maxLengthLeft = maxLen;
  uint8_t *buf = (uint8_t *)malloc(buf_size);

  if (!buf) {
    return false;
  }

  int bytesAvailable = stream.available();
  while ((bytesAvailable > 0) && (maxLengthLeft > 0)) {
    // Determine number of bytes to read
    int readBytes = bytesAvailable;
    if (readBytes > maxLengthLeft) {
      readBytes = maxLengthLeft;
    }
    if (readBytes > buf_size) {
      readBytes = buf_size;
    }

    // Read data and check if we got something
    int numBytesRead = stream.readBytes(buf, readBytes);
    if (numBytesRead < 1) {
      free(buf);
      return false;
    }

    // Update SHA3 with buffer payload
    add(buf, numBytesRead);

    // Update available number of bytes
    maxLengthLeft -= numBytesRead;
    bytesAvailable = stream.available();
  }
  free(buf);
  return true;
}

// Finalize the hash computation
void SHA3Builder::calculate() {
  if (finalized) {
    return;
  }

  // Pad the input
  pad();

  // Process the final block
  process_block(buffer);

  // Extract bytes from the state
  for (size_t i = 0; i < hash_size; i++) {
    size_t state_idx = i >> 3;           // i / 8
    size_t bit_offset = (i & 0x7) << 3;  // (i % 8) * 8
    hash[i] = (uint8_t)(state[state_idx] >> bit_offset);
  }

  finalized = true;
}

// Get the hash as bytes
void SHA3Builder::getBytes(uint8_t *output) {
  memcpy(output, hash, hash_size);
}

// Get the hash as hex string
void SHA3Builder::getChars(char *output) {
  if (!finalized || output == nullptr) {
    log_e("Error: SHA3 not calculated or no output buffer provided.");
    return;
  }

  bytes2hex(output, hash_size * 2 + 1, hash, hash_size);
}

// Get the hash as String
String SHA3Builder::toString() {
  char out[(hash_size * 2) + 1];
  getChars(out);
  return String(out);
}
