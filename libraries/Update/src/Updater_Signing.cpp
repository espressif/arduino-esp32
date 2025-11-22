/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef UPDATE_SIGN

#include "Updater_Signing.h"
#include "mbedtls/pk.h"
#include "mbedtls/rsa.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/ecp.h"
#include "mbedtls/md.h"
#include "esp32-hal-log.h"

// ==================== UpdaterRSAVerifier (using mbedtls) ====================

UpdaterRSAVerifier::UpdaterRSAVerifier(const uint8_t *pubkey, size_t pubkeyLen, int hashType) : _hashType(hashType), _valid(false) {
  _ctx = new mbedtls_pk_context;
  mbedtls_pk_init((mbedtls_pk_context *)_ctx);

  // Try to parse the public key
  int ret = mbedtls_pk_parse_public_key((mbedtls_pk_context *)_ctx, pubkey, pubkeyLen);
  if (ret != 0) {
    log_e("Failed to parse RSA public key: -0x%04X", -ret);
    return;
  }

  // Verify it's an RSA key
  if (mbedtls_pk_get_type((mbedtls_pk_context *)_ctx) != MBEDTLS_PK_RSA) {
    log_e("Public key is not RSA");
    return;
  }

  _valid = true;
  log_i("RSA public key loaded successfully");
}

UpdaterRSAVerifier::~UpdaterRSAVerifier() {
  if (_ctx) {
    mbedtls_pk_free((mbedtls_pk_context *)_ctx);
    delete (mbedtls_pk_context *)_ctx;
    _ctx = nullptr;
  }
}

bool UpdaterRSAVerifier::verify(SHA2Builder *hash, const void *signature, size_t signatureLen) {
  if (!_valid || !hash) {
    log_e("Invalid RSA verifier or hash");
    return false;
  }

  mbedtls_md_type_t md_type;
  switch (_hashType) {
    case HASH_SHA256: md_type = MBEDTLS_MD_SHA256; break;
    case HASH_SHA384: md_type = MBEDTLS_MD_SHA384; break;
    case HASH_SHA512: md_type = MBEDTLS_MD_SHA512; break;
    default:
      log_e("Invalid hash type");
      return false;
  }

  // Get hash bytes from the builder
  uint8_t hashBytes[64];  // Max hash size (SHA-512)
  hash->getBytes(hashBytes);

  int ret = mbedtls_pk_verify((mbedtls_pk_context *)_ctx, md_type, hashBytes, hash->getHashSize(), (const unsigned char *)signature, signatureLen);

  if (ret == 0) {
    log_i("RSA signature verified successfully");
    return true;
  } else {
    log_e("RSA signature verification failed: -0x%04X", -ret);
    return false;
  }
}

// ==================== UpdaterECDSAVerifier (using mbedtls) ====================

UpdaterECDSAVerifier::UpdaterECDSAVerifier(const uint8_t *pubkey, size_t pubkeyLen, int hashType) : _hashType(hashType), _valid(false) {
  _ctx = new mbedtls_pk_context;
  mbedtls_pk_init((mbedtls_pk_context *)_ctx);

  // Try to parse the public key
  int ret = mbedtls_pk_parse_public_key((mbedtls_pk_context *)_ctx, pubkey, pubkeyLen);
  if (ret != 0) {
    log_e("Failed to parse ECDSA public key: -0x%04X", -ret);
    return;
  }

  // Verify it's an ECDSA key
  mbedtls_pk_type_t type = mbedtls_pk_get_type((mbedtls_pk_context *)_ctx);
  if (type != MBEDTLS_PK_ECKEY && type != MBEDTLS_PK_ECDSA) {
    log_e("Public key is not ECDSA");
    return;
  }

  _valid = true;
  log_i("ECDSA public key loaded successfully");
}

UpdaterECDSAVerifier::~UpdaterECDSAVerifier() {
  if (_ctx) {
    mbedtls_pk_free((mbedtls_pk_context *)_ctx);
    delete (mbedtls_pk_context *)_ctx;
    _ctx = nullptr;
  }
}

bool UpdaterECDSAVerifier::verify(SHA2Builder *hash, const void *signature, size_t signatureLen) {
  if (!_valid || !hash) {
    log_e("Invalid ECDSA verifier or hash");
    return false;
  }

  mbedtls_md_type_t md_type;
  switch (_hashType) {
    case HASH_SHA256: md_type = MBEDTLS_MD_SHA256; break;
    case HASH_SHA384: md_type = MBEDTLS_MD_SHA384; break;
    case HASH_SHA512: md_type = MBEDTLS_MD_SHA512; break;
    default:
      log_e("Invalid hash type");
      return false;
  }

  // Get hash bytes from the builder
  uint8_t hashBytes[64];  // Max hash size (SHA-512)
  hash->getBytes(hashBytes);

  int ret = mbedtls_pk_verify((mbedtls_pk_context *)_ctx, md_type, hashBytes, hash->getHashSize(), (const unsigned char *)signature, signatureLen);

  if (ret == 0) {
    log_i("ECDSA signature verified successfully");
    return true;
  } else {
    log_e("ECDSA signature verification failed: -0x%04X", -ret);
    return false;
  }
}

#endif  // UPDATE_SIGN
