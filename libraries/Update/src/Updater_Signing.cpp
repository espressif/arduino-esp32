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
#include "mbedtls/asn1.h"
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
    default:          log_e("Invalid hash type"); return false;
  }

  // Get hash bytes from the builder
  uint8_t hashBytes[64];  // Max hash size (SHA-512)
  hash->getBytes(hashBytes);
  size_t hash_size = hash->getHashSize();

  // Use RSA-PSS verification to match bin_signing.py which signs with PSS padding
  // and salt_length=PSS.MAX_LENGTH (which equals key_len - hash_size - 2)
  mbedtls_rsa_context *rsa_ctx = mbedtls_pk_rsa(*(mbedtls_pk_context *)_ctx);
  if (!rsa_ctx) {
    log_e("Failed to get RSA context");
    return false;
  }

  int key_len = (int)mbedtls_rsa_get_len(rsa_ctx);
  // PSS.MAX_LENGTH salt = key_len - hash_size - 2
  int expected_salt_len = key_len - (int)hash_size - 2;
  if (expected_salt_len < 0) {
    log_e("RSA key too small for hash algorithm");
    return false;
  }

  mbedtls_pk_rsassa_pss_options pss_opts;
  pss_opts.mgf1_hash_id = md_type;
  pss_opts.expected_salt_len = expected_salt_len;

  // RSA signatures are always exactly key_len bytes. The buffer may be
  // zero-padded to a larger size (e.g. 512), so use key_len as the actual
  // signature length to avoid MBEDTLS_ERR_PK_SIG_LEN_MISMATCH.
  int ret = mbedtls_pk_verify_ext(
    MBEDTLS_PK_RSASSA_PSS, &pss_opts, (mbedtls_pk_context *)_ctx, md_type, hashBytes, hash_size, (const unsigned char *)signature, key_len
  );

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
    default:          log_e("Invalid hash type"); return false;
  }

  // Get hash bytes from the builder
  uint8_t hashBytes[64];  // Max hash size (SHA-512)
  hash->getBytes(hashBytes);

  // ECDSA signatures are DER-encoded (SEQUENCE { INTEGER r, INTEGER s }).
  // The buffer may be zero-padded to a larger size (e.g. 512), so determine
  // the actual DER-encoded signature length from the ASN.1 header to avoid
  // MBEDTLS_ERR_PK_SIG_LEN_MISMATCH.
  const uint8_t *sig = (const uint8_t *)signature;
  size_t actualSigLen = signatureLen;
  unsigned char *p = (unsigned char *)sig;
  const unsigned char *end = p + signatureLen;
  size_t seq_len = 0;
  if (mbedtls_asn1_get_tag(&p, end, &seq_len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE) == 0) {
    // actualSigLen = header bytes (p advanced past them) + content length
    size_t parsed = (size_t)(p - sig) + seq_len;
    if (parsed <= signatureLen) {
      actualSigLen = parsed;
    }
  }

  int ret = mbedtls_pk_verify((mbedtls_pk_context *)_ctx, md_type, hashBytes, hash->getHashSize(), sig, actualSigLen);

  if (ret == 0) {
    log_i("ECDSA signature verified successfully");
    return true;
  } else {
    log_e("ECDSA signature verification failed: -0x%04X", -ret);
    return false;
  }
}

#endif  // UPDATE_SIGN
