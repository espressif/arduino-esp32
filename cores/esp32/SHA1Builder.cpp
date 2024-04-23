/*
 *  FIPS-180-1 compliant SHA-1 implementation
 *
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is part of mbed TLS (https://tls.mbed.org)
 *  Modified for esp32 by Lucas Saavedra Vaz on 11 Jan 2024
 */

#include <Arduino.h>
#include <SHA1Builder.h>

// 32-bit integer manipulation macros (big endian)

#ifndef GET_UINT32_BE
#define GET_UINT32_BE(n, b, i) \
  { (n) = ((uint32_t)(b)[(i)] << 24) | ((uint32_t)(b)[(i) + 1] << 16) | ((uint32_t)(b)[(i) + 2] << 8) | ((uint32_t)(b)[(i) + 3]); }
#endif

#ifndef PUT_UINT32_BE
#define PUT_UINT32_BE(n, b, i)           \
  {                                      \
    (b)[(i)] = (uint8_t)((n) >> 24);     \
    (b)[(i) + 1] = (uint8_t)((n) >> 16); \
    (b)[(i) + 2] = (uint8_t)((n) >> 8);  \
    (b)[(i) + 3] = (uint8_t)((n));       \
  }
#endif

// Constants

static const uint8_t sha1_padding[64] = {0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                         0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// Private methods

void SHA1Builder::process(const uint8_t *data) {
  uint32_t temp, W[16], A, B, C, D, E;

  GET_UINT32_BE(W[0], data, 0);
  GET_UINT32_BE(W[1], data, 4);
  GET_UINT32_BE(W[2], data, 8);
  GET_UINT32_BE(W[3], data, 12);
  GET_UINT32_BE(W[4], data, 16);
  GET_UINT32_BE(W[5], data, 20);
  GET_UINT32_BE(W[6], data, 24);
  GET_UINT32_BE(W[7], data, 28);
  GET_UINT32_BE(W[8], data, 32);
  GET_UINT32_BE(W[9], data, 36);
  GET_UINT32_BE(W[10], data, 40);
  GET_UINT32_BE(W[11], data, 44);
  GET_UINT32_BE(W[12], data, 48);
  GET_UINT32_BE(W[13], data, 52);
  GET_UINT32_BE(W[14], data, 56);
  GET_UINT32_BE(W[15], data, 60);

#define sha1_S(x, n) ((x << n) | ((x & 0xFFFFFFFF) >> (32 - n)))

#define sha1_R(t) (temp = W[(t - 3) & 0x0F] ^ W[(t - 8) & 0x0F] ^ W[(t - 14) & 0x0F] ^ W[t & 0x0F], (W[t & 0x0F] = sha1_S(temp, 1)))

#define sha1_P(a, b, c, d, e, x)                      \
  {                                                   \
    e += sha1_S(a, 5) + sha1_F(b, c, d) + sha1_K + x; \
    b = sha1_S(b, 30);                                \
  }

  A = state[0];
  B = state[1];
  C = state[2];
  D = state[3];
  E = state[4];

#define sha1_F(x, y, z) (z ^ (x & (y ^ z)))
#define sha1_K          0x5A827999

  sha1_P(A, B, C, D, E, W[0]);
  sha1_P(E, A, B, C, D, W[1]);
  sha1_P(D, E, A, B, C, W[2]);
  sha1_P(C, D, E, A, B, W[3]);
  sha1_P(B, C, D, E, A, W[4]);
  sha1_P(A, B, C, D, E, W[5]);
  sha1_P(E, A, B, C, D, W[6]);
  sha1_P(D, E, A, B, C, W[7]);
  sha1_P(C, D, E, A, B, W[8]);
  sha1_P(B, C, D, E, A, W[9]);
  sha1_P(A, B, C, D, E, W[10]);
  sha1_P(E, A, B, C, D, W[11]);
  sha1_P(D, E, A, B, C, W[12]);
  sha1_P(C, D, E, A, B, W[13]);
  sha1_P(B, C, D, E, A, W[14]);
  sha1_P(A, B, C, D, E, W[15]);
  sha1_P(E, A, B, C, D, sha1_R(16));
  sha1_P(D, E, A, B, C, sha1_R(17));
  sha1_P(C, D, E, A, B, sha1_R(18));
  sha1_P(B, C, D, E, A, sha1_R(19));

#undef sha1_K
#undef sha1_F

#define sha1_F(x, y, z) (x ^ y ^ z)
#define sha1_K          0x6ED9EBA1

  sha1_P(A, B, C, D, E, sha1_R(20));
  sha1_P(E, A, B, C, D, sha1_R(21));
  sha1_P(D, E, A, B, C, sha1_R(22));
  sha1_P(C, D, E, A, B, sha1_R(23));
  sha1_P(B, C, D, E, A, sha1_R(24));
  sha1_P(A, B, C, D, E, sha1_R(25));
  sha1_P(E, A, B, C, D, sha1_R(26));
  sha1_P(D, E, A, B, C, sha1_R(27));
  sha1_P(C, D, E, A, B, sha1_R(28));
  sha1_P(B, C, D, E, A, sha1_R(29));
  sha1_P(A, B, C, D, E, sha1_R(30));
  sha1_P(E, A, B, C, D, sha1_R(31));
  sha1_P(D, E, A, B, C, sha1_R(32));
  sha1_P(C, D, E, A, B, sha1_R(33));
  sha1_P(B, C, D, E, A, sha1_R(34));
  sha1_P(A, B, C, D, E, sha1_R(35));
  sha1_P(E, A, B, C, D, sha1_R(36));
  sha1_P(D, E, A, B, C, sha1_R(37));
  sha1_P(C, D, E, A, B, sha1_R(38));
  sha1_P(B, C, D, E, A, sha1_R(39));

#undef sha1_K
#undef sha1_F

#define sha1_F(x, y, z) ((x & y) | (z & (x | y)))
#define sha1_K          0x8F1BBCDC

  sha1_P(A, B, C, D, E, sha1_R(40));
  sha1_P(E, A, B, C, D, sha1_R(41));
  sha1_P(D, E, A, B, C, sha1_R(42));
  sha1_P(C, D, E, A, B, sha1_R(43));
  sha1_P(B, C, D, E, A, sha1_R(44));
  sha1_P(A, B, C, D, E, sha1_R(45));
  sha1_P(E, A, B, C, D, sha1_R(46));
  sha1_P(D, E, A, B, C, sha1_R(47));
  sha1_P(C, D, E, A, B, sha1_R(48));
  sha1_P(B, C, D, E, A, sha1_R(49));
  sha1_P(A, B, C, D, E, sha1_R(50));
  sha1_P(E, A, B, C, D, sha1_R(51));
  sha1_P(D, E, A, B, C, sha1_R(52));
  sha1_P(C, D, E, A, B, sha1_R(53));
  sha1_P(B, C, D, E, A, sha1_R(54));
  sha1_P(A, B, C, D, E, sha1_R(55));
  sha1_P(E, A, B, C, D, sha1_R(56));
  sha1_P(D, E, A, B, C, sha1_R(57));
  sha1_P(C, D, E, A, B, sha1_R(58));
  sha1_P(B, C, D, E, A, sha1_R(59));

#undef sha1_K
#undef sha1_F

#define sha1_F(x, y, z) (x ^ y ^ z)
#define sha1_K          0xCA62C1D6

  sha1_P(A, B, C, D, E, sha1_R(60));
  sha1_P(E, A, B, C, D, sha1_R(61));
  sha1_P(D, E, A, B, C, sha1_R(62));
  sha1_P(C, D, E, A, B, sha1_R(63));
  sha1_P(B, C, D, E, A, sha1_R(64));
  sha1_P(A, B, C, D, E, sha1_R(65));
  sha1_P(E, A, B, C, D, sha1_R(66));
  sha1_P(D, E, A, B, C, sha1_R(67));
  sha1_P(C, D, E, A, B, sha1_R(68));
  sha1_P(B, C, D, E, A, sha1_R(69));
  sha1_P(A, B, C, D, E, sha1_R(70));
  sha1_P(E, A, B, C, D, sha1_R(71));
  sha1_P(D, E, A, B, C, sha1_R(72));
  sha1_P(C, D, E, A, B, sha1_R(73));
  sha1_P(B, C, D, E, A, sha1_R(74));
  sha1_P(A, B, C, D, E, sha1_R(75));
  sha1_P(E, A, B, C, D, sha1_R(76));
  sha1_P(D, E, A, B, C, sha1_R(77));
  sha1_P(C, D, E, A, B, sha1_R(78));
  sha1_P(B, C, D, E, A, sha1_R(79));

#undef sha1_K
#undef sha1_F

  state[0] += A;
  state[1] += B;
  state[2] += C;
  state[3] += D;
  state[4] += E;
}

// Public methods

void SHA1Builder::begin(void) {
  total[0] = 0;
  total[1] = 0;

  state[0] = 0x67452301;
  state[1] = 0xEFCDAB89;
  state[2] = 0x98BADCFE;
  state[3] = 0x10325476;
  state[4] = 0xC3D2E1F0;

  memset(buffer, 0x00, sizeof(buffer));
  memset(hash, 0x00, sizeof(hash));
}

void SHA1Builder::add(const uint8_t *data, size_t len) {
  size_t fill;
  uint32_t left;

  if (len == 0) {
    return;
  }

  left = total[0] & 0x3F;
  fill = 64 - left;

  total[0] += (uint32_t)len;
  total[0] &= 0xFFFFFFFF;

  if (total[0] < (uint32_t)len) {
    total[1]++;
  }

  if (left && len >= fill) {
    memcpy((void *)(buffer + left), data, fill);
    process(buffer);
    data += fill;
    len -= fill;
    left = 0;
  }

  while (len >= 64) {
    process(data);
    data += 64;
    len -= 64;
  }

  if (len > 0) {
    memcpy((void *)(buffer + left), data, len);
  }
}

void SHA1Builder::addHexString(const char *data) {
  uint16_t len = strlen(data);
  uint8_t *tmp = (uint8_t *)malloc(len / 2);
  if (tmp == NULL) {
    return;
  }
  hex2bytes(tmp, len / 2, data);
  add(tmp, len / 2);
  free(tmp);
}

bool SHA1Builder::addStream(Stream &stream, const size_t maxLen) {
  const int buf_size = 512;
  int maxLengthLeft = maxLen;
  uint8_t *buf = (uint8_t *)malloc(buf_size);

  if (!buf) {
    return false;
  }

  int bytesAvailable = stream.available();
  while ((bytesAvailable > 0) && (maxLengthLeft > 0)) {

    // determine number of bytes to read
    int readBytes = bytesAvailable;
    if (readBytes > maxLengthLeft) {
      readBytes = maxLengthLeft;  // read only until max_len
    }
    if (readBytes > buf_size) {
      readBytes = buf_size;  // not read more the buffer can handle
    }

    // read data and check if we got something
    int numBytesRead = stream.readBytes(buf, readBytes);
    if (numBytesRead < 1) {
      free(buf);
      return false;
    }

    // Update SHA1 with buffer payload
    add(buf, numBytesRead);

    // update available number of bytes
    maxLengthLeft -= numBytesRead;
    bytesAvailable = stream.available();
  }
  free(buf);
  return true;
}

void SHA1Builder::calculate(void) {
  uint32_t last, padn;
  uint32_t high, low;
  uint8_t msglen[8];

  high = (total[0] >> 29) | (total[1] << 3);
  low = (total[0] << 3);

  PUT_UINT32_BE(high, msglen, 0);
  PUT_UINT32_BE(low, msglen, 4);

  last = total[0] & 0x3F;
  padn = (last < 56) ? (56 - last) : (120 - last);

  add((uint8_t *)sha1_padding, padn);
  add(msglen, 8);

  PUT_UINT32_BE(state[0], hash, 0);
  PUT_UINT32_BE(state[1], hash, 4);
  PUT_UINT32_BE(state[2], hash, 8);
  PUT_UINT32_BE(state[3], hash, 12);
  PUT_UINT32_BE(state[4], hash, 16);
}

void SHA1Builder::getBytes(uint8_t *output) {
  memcpy(output, hash, SHA1_HASH_SIZE);
}

void SHA1Builder::getChars(char *output) {
  bytes2hex(output, SHA1_HASH_SIZE * 2 + 1, hash, SHA1_HASH_SIZE);
}

String SHA1Builder::toString(void) {
  char out[(SHA1_HASH_SIZE * 2) + 1];
  getChars(out);
  return String(out);
}
