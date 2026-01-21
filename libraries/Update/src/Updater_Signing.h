/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#ifdef UPDATE_SIGN

#include <Arduino.h>
#include <SHA2Builder.h>

#ifdef __cplusplus
extern "C" {
#endif

// Signature schemes
#define SIGN_NONE  0
#define SIGN_RSA   1
#define SIGN_ECDSA 2

// Hash algorithms for signature verification
#define HASH_SHA256 0
#define HASH_SHA384 1
#define HASH_SHA512 2

// Signature sizes (in bytes)
#define RSA_2048_SIGNATURE_SIZE   256
#define RSA_3072_SIGNATURE_SIZE   384
#define RSA_4096_SIGNATURE_SIZE   512
#define ECDSA_P256_SIGNATURE_SIZE 64
#define ECDSA_P384_SIGNATURE_SIZE 96

// Hash sizes (in bytes)
#define SHA256_SIZE 32
#define SHA384_SIZE 48
#define SHA512_SIZE 64

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

class UpdaterVerifyClass {
public:
  virtual bool verify(SHA2Builder *hash, const void *signature, size_t signatureLen) = 0;
  virtual int getHashType() const = 0;
  virtual ~UpdaterVerifyClass() {}
};

// Signature verifiers using mbedtls (required for public key cryptography)
class UpdaterRSAVerifier : public UpdaterVerifyClass {
public:
  UpdaterRSAVerifier(const uint8_t *pubkey, size_t pubkeyLen, int hashType = HASH_SHA256);
  ~UpdaterRSAVerifier();
  bool verify(SHA2Builder *hash, const void *signature, size_t signatureLen) override;
  int getHashType() const override {
    return _hashType;
  }

private:
  void *_ctx;
  int _hashType;
  bool _valid;
};

class UpdaterECDSAVerifier : public UpdaterVerifyClass {
public:
  UpdaterECDSAVerifier(const uint8_t *pubkey, size_t pubkeyLen, int hashType = HASH_SHA256);
  ~UpdaterECDSAVerifier();
  bool verify(SHA2Builder *hash, const void *signature, size_t signatureLen) override;
  int getHashType() const override {
    return _hashType;
  }

private:
  void *_ctx;
  int _hashType;
  bool _valid;
};

#endif  // __cplusplus

#endif  // UPDATE_SIGN
