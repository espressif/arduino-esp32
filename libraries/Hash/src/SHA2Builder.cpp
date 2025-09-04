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
#include <string.h>

#include "esp32-hal-log.h"
#include "SHA2Builder.h"

// SHA-256 constants
static const uint32_t sha256_k[64] = {0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
                                      0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
                                      0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
                                      0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
                                      0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
                                      0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
                                      0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

// SHA-512 constants
static const uint64_t sha512_k[80] = {0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL, 0x3956c25bf348b538ULL,
                                      0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL, 0xd807aa98a3030242ULL, 0x12835b0145706fbeULL,
                                      0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL, 0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL,
                                      0xc19bf174cf692694ULL, 0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
                                      0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL, 0x983e5152ee66dfabULL,
                                      0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL, 0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
                                      0x06ca6351e003826fULL, 0x142929670a0e6e70ULL, 0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL,
                                      0x53380d139d95b3dfULL, 0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
                                      0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL,
                                      0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL, 0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL,
                                      0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL,
                                      0x682e6ff3d6b2b8a3ULL, 0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
                                      0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL, 0xca273eceea26619cULL,
                                      0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL,
                                      0x113f9804bef90daeULL, 0x1b710b35131c471bULL, 0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL,
                                      0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL};

// Macros for bit manipulation
#define ROTR32(x, n)   (((x) >> (n)) | ((x) << (32 - (n))))
#define ROTR64(x, n)   (((x) >> (n)) | ((x) << (64 - (n))))
#define CH32(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define CH64(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define MAJ32(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define MAJ64(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0_32(x)      (ROTR32(x, 2) ^ ROTR32(x, 13) ^ ROTR32(x, 22))
#define EP0_64(x)      (ROTR64(x, 28) ^ ROTR64(x, 34) ^ ROTR64(x, 39))
#define EP1_32(x)      (ROTR32(x, 6) ^ ROTR32(x, 11) ^ ROTR32(x, 25))
#define EP1_64(x)      (ROTR64(x, 14) ^ ROTR64(x, 18) ^ ROTR64(x, 41))
#define SIG0_32(x)     (ROTR32(x, 7) ^ ROTR32(x, 18) ^ ((x) >> 3))
#define SIG0_64(x)     (ROTR64(x, 1) ^ ROTR64(x, 8) ^ ((x) >> 7))
#define SIG1_32(x)     (ROTR32(x, 17) ^ ROTR32(x, 19) ^ ((x) >> 10))
#define SIG1_64(x)     (ROTR64(x, 19) ^ ROTR64(x, 61) ^ ((x) >> 6))

// Byte order conversion
#define BYTESWAP32(x) ((((x) & 0xFF000000) >> 24) | (((x) & 0x00FF0000) >> 8) | (((x) & 0x0000FF00) << 8) | (((x) & 0x000000FF) << 24))
#define BYTESWAP64(x) (((uint64_t)BYTESWAP32((uint32_t)((x) >> 32))) | (((uint64_t)BYTESWAP32((uint32_t)(x))) << 32))

// Constructor
SHA2Builder::SHA2Builder(size_t hash_size) : hash_size(hash_size), buffer_size(0), finalized(false), total_length(0) {
  // Determine block size and algorithm family
  if (hash_size == SHA2_224_HASH_SIZE || hash_size == SHA2_256_HASH_SIZE) {
    block_size = SHA2_256_BLOCK_SIZE;
    is_sha512 = false;
  } else if (hash_size == SHA2_384_HASH_SIZE || hash_size == SHA2_512_HASH_SIZE) {
    block_size = SHA2_512_BLOCK_SIZE;
    is_sha512 = true;
  } else {
    log_e("Invalid hash size: %d", hash_size);
    block_size = 0;
    is_sha512 = false;
  }
}

// Initialize the hash computation
void SHA2Builder::begin() {
  // Clear the state and buffer
  memset(state_32, 0, sizeof(state_32));
  memset(state_64, 0, sizeof(state_64));
  memset(buffer, 0, sizeof(buffer));
  buffer_size = 0;
  finalized = false;
  total_length = 0;

  // Initialize state based on algorithm
  if (!is_sha512) {
    // SHA-224/256 initial values
    if (hash_size == SHA2_224_HASH_SIZE) {
      // SHA-224 initial values
      state_32[0] = 0xc1059ed8;
      state_32[1] = 0x367cd507;
      state_32[2] = 0x3070dd17;
      state_32[3] = 0xf70e5939;
      state_32[4] = 0xffc00b31;
      state_32[5] = 0x68581511;
      state_32[6] = 0x64f98fa7;
      state_32[7] = 0xbefa4fa4;
    } else {
      // SHA-256 initial values
      state_32[0] = 0x6a09e667;
      state_32[1] = 0xbb67ae85;
      state_32[2] = 0x3c6ef372;
      state_32[3] = 0xa54ff53a;
      state_32[4] = 0x510e527f;
      state_32[5] = 0x9b05688c;
      state_32[6] = 0x1f83d9ab;
      state_32[7] = 0x5be0cd19;
    }
  } else {
    // SHA-384/512 initial values
    if (hash_size == SHA2_384_HASH_SIZE) {
      // SHA-384 initial values
      state_64[0] = 0xcbbb9d5dc1059ed8ULL;
      state_64[1] = 0x629a292a367cd507ULL;
      state_64[2] = 0x9159015a3070dd17ULL;
      state_64[3] = 0x152fecd8f70e5939ULL;
      state_64[4] = 0x67332667ffc00b31ULL;
      state_64[5] = 0x8eb44a8768581511ULL;
      state_64[6] = 0xdb0c2e0d64f98fa7ULL;
      state_64[7] = 0x47b5481dbefa4fa4ULL;
    } else {
      // SHA-512 initial values
      state_64[0] = 0x6a09e667f3bcc908ULL;
      state_64[1] = 0xbb67ae8584caa73bULL;
      state_64[2] = 0x3c6ef372fe94f82bULL;
      state_64[3] = 0xa54ff53a5f1d36f1ULL;
      state_64[4] = 0x510e527fade682d1ULL;
      state_64[5] = 0x9b05688c2b3e6c1fULL;
      state_64[6] = 0x1f83d9abfb41bd6bULL;
      state_64[7] = 0x5be0cd19137e2179ULL;
    }
  }
}

// Process a block for SHA-256
void SHA2Builder::process_block_sha256(const uint8_t *data) {
  uint32_t w[64];
  uint32_t a, b, c, d, e, f, g, h;
  uint32_t t1, t2;

  // Prepare message schedule
  for (int i = 0; i < 16; i++) {
    w[i] = BYTESWAP32(((uint32_t *)data)[i]);
  }
  for (int i = 16; i < 64; i++) {
    w[i] = SIG1_32(w[i - 2]) + w[i - 7] + SIG0_32(w[i - 15]) + w[i - 16];
  }

  // Initialize working variables
  a = state_32[0];
  b = state_32[1];
  c = state_32[2];
  d = state_32[3];
  e = state_32[4];
  f = state_32[5];
  g = state_32[6];
  h = state_32[7];

  // Main loop
  for (int i = 0; i < 64; i++) {
    t1 = h + EP1_32(e) + CH32(e, f, g) + sha256_k[i] + w[i];
    t2 = EP0_32(a) + MAJ32(a, b, c);
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  // Add the compressed chunk to the current hash value
  state_32[0] += a;
  state_32[1] += b;
  state_32[2] += c;
  state_32[3] += d;
  state_32[4] += e;
  state_32[5] += f;
  state_32[6] += g;
  state_32[7] += h;
}

// Process a block for SHA-512
void SHA2Builder::process_block_sha512(const uint8_t *data) {
  uint64_t w[80];
  uint64_t a, b, c, d, e, f, g, h;
  uint64_t t1, t2;

  // Prepare message schedule
  for (int i = 0; i < 16; i++) {
    w[i] = BYTESWAP64(((uint64_t *)data)[i]);
  }
  for (int i = 16; i < 80; i++) {
    w[i] = SIG1_64(w[i - 2]) + w[i - 7] + SIG0_64(w[i - 15]) + w[i - 16];
  }

  // Initialize working variables
  a = state_64[0];
  b = state_64[1];
  c = state_64[2];
  d = state_64[3];
  e = state_64[4];
  f = state_64[5];
  g = state_64[6];
  h = state_64[7];

  // Main loop
  for (int i = 0; i < 80; i++) {
    t1 = h + EP1_64(e) + CH64(e, f, g) + sha512_k[i] + w[i];
    t2 = EP0_64(a) + MAJ64(a, b, c);
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  // Add the compressed chunk to the current hash value
  state_64[0] += a;
  state_64[1] += b;
  state_64[2] += c;
  state_64[3] += d;
  state_64[4] += e;
  state_64[5] += f;
  state_64[6] += g;
  state_64[7] += h;
}

// Add data to the hash computation
void SHA2Builder::add(const uint8_t *data, size_t len) {
  if (finalized || len == 0) {
    return;
  }

  total_length += len;
  size_t offset = 0;

  // Process any buffered data first
  if (buffer_size > 0) {
    size_t to_copy = std::min(len, block_size - buffer_size);
    memcpy(buffer + buffer_size, data, to_copy);
    buffer_size += to_copy;
    offset += to_copy;

    if (buffer_size == block_size) {
      if (is_sha512) {
        process_block_sha512(buffer);
      } else {
        process_block_sha256(buffer);
      }
      buffer_size = 0;
    }
  }

  // Process full blocks
  while (offset + block_size <= len) {
    if (is_sha512) {
      process_block_sha512(data + offset);
    } else {
      process_block_sha256(data + offset);
    }
    offset += block_size;
  }

  // Buffer remaining data
  if (offset < len) {
    memcpy(buffer, data + offset, len - offset);
    buffer_size = len - offset;
  }
}

// Add data from a stream
bool SHA2Builder::addStream(Stream &stream, const size_t maxLen) {
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

    // Update SHA2 with buffer payload
    add(buf, numBytesRead);

    // Update available number of bytes
    maxLengthLeft -= numBytesRead;
    bytesAvailable = stream.available();
  }
  free(buf);
  return true;
}

// Pad the input according to SHA2 specification
void SHA2Builder::pad() {
  // Calculate the number of bytes we have
  uint64_t bit_length = total_length * 8;

  // Add the bit '1' to the message
  buffer[buffer_size++] = 0x80;

  // Pad with zeros until we have enough space for the length
  while (buffer_size + 8 > block_size) {
    if (buffer_size < block_size) {
      buffer[buffer_size++] = 0x00;
    } else {
      // Process the block
      if (is_sha512) {
        process_block_sha512(buffer);
      } else {
        process_block_sha256(buffer);
      }
      buffer_size = 0;
    }
  }

  // Pad with zeros to make room for the length
  while (buffer_size + 8 < block_size) {
    buffer[buffer_size++] = 0x00;
  }

  // Add the length in bits
  if (is_sha512) {
    // For SHA-512, length is 128 bits (16 bytes)
    // We only use the lower 64 bits for now
    for (int i = 0; i < 8; i++) {
      buffer[block_size - 8 + i] = (uint8_t)(bit_length >> (56 - i * 8));
    }
    // Set the upper 64 bits to 0 (for SHA-384/512, length is limited to 2^128-1)
    for (int i = 0; i < 8; i++) {
      buffer[block_size - 16 + i] = 0x00;
    }
  } else {
    // For SHA-256, length is 64 bits (8 bytes)
    for (int i = 0; i < 8; i++) {
      buffer[block_size - 8 + i] = (uint8_t)(bit_length >> (56 - i * 8));
    }
  }
}

// Finalize the hash computation
void SHA2Builder::calculate() {
  if (finalized) {
    return;
  }

  // Pad the input
  pad();

  // Process the final block
  if (is_sha512) {
    process_block_sha512(buffer);
  } else {
    process_block_sha256(buffer);
  }

  // Extract bytes from the state
  if (is_sha512) {
    for (size_t i = 0; i < hash_size; i++) {
      hash[i] = (uint8_t)(state_64[i >> 3] >> (56 - ((i & 0x7) << 3)));
    }
  } else {
    for (size_t i = 0; i < hash_size; i++) {
      hash[i] = (uint8_t)(state_32[i >> 2] >> (24 - ((i & 0x3) << 3)));
    }
  }

  finalized = true;
}

// Get the hash as bytes
void SHA2Builder::getBytes(uint8_t *output) {
  memcpy(output, hash, hash_size);
}

// Get the hash as hex string
void SHA2Builder::getChars(char *output) {
  if (!finalized || output == nullptr) {
    log_e("Error: SHA2 not calculated or no output buffer provided.");
    return;
  }

  bytes2hex(output, hash_size * 2 + 1, hash, hash_size);
}

// Get the hash as String
String SHA2Builder::toString() {
  char out[(hash_size * 2) + 1];
  getChars(out);
  return String(out);
}
