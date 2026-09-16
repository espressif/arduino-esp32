/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Update.h"
#include "Arduino.h"
#include "HEXBuilder.h"
#include "mbedtls/build_info.h"
#if MBEDTLS_VERSION_MAJOR >= 4
#include "psa/crypto.h"
#else
#include "mbedtls/sha512.h"
#endif /* MBEDTLS_VERSION_MAJOR >= 4 */

// Optional SHA-512 streaming hash. Linked only when a sketch calls setSHA512()
// (referenced via a function-pointer table assigned in that setter), so
// --gc-sections can drop mbedtls/PSA SHA-512 from Update sketches that never
// enable it.

struct UpdateSHA512Context {
#if MBEDTLS_VERSION_MAJOR >= 4
  psa_hash_operation_t op;
#else
  mbedtls_sha512_context ctx;
#endif
  uint8_t expected[64];
};

static bool update_sha512_begin(UpdateSHA512Context *ctx) {
#if MBEDTLS_VERSION_MAJOR >= 4
  psa_status_t psa_ret = psa_crypto_init();
  if (psa_ret != PSA_SUCCESS) {
    log_e("PSA crypto init failed: %d", (int)psa_ret);
    return false;
  }
  ctx->op = psa_hash_operation_init();
  psa_ret = psa_hash_setup(&ctx->op, PSA_ALG_SHA_512);
  if (psa_ret != PSA_SUCCESS) {
    log_e("PSA hash setup failed: %d", (int)psa_ret);
    psa_hash_abort(&ctx->op);
    ctx->op = psa_hash_operation_init();
    return false;
  }
  return true;
#else
  mbedtls_sha512_init(&ctx->ctx);
  if (mbedtls_sha512_starts(&ctx->ctx, 0) != 0) {
    mbedtls_sha512_free(&ctx->ctx);
    return false;
  }
  return true;
#endif
}

static bool update_sha512_update(UpdateSHA512Context *ctx, const uint8_t *data, size_t len) {
  if (!ctx || !data || len == 0) {
    return true;
  }
#if MBEDTLS_VERSION_MAJOR >= 4
  return psa_hash_update(&ctx->op, data, len) == PSA_SUCCESS;
#else
  return mbedtls_sha512_update(&ctx->ctx, data, len) == 0;
#endif
}

static bool update_sha512_finish(UpdateSHA512Context *ctx, uint8_t out[64]) {
  if (!ctx || !out) {
    return false;
  }
#if MBEDTLS_VERSION_MAJOR >= 4
  size_t hash_len = 0;
  psa_status_t psa_ret = psa_hash_finish(&ctx->op, out, 64, &hash_len);
  if (psa_ret != PSA_SUCCESS || hash_len != 64) {
    psa_hash_abort(&ctx->op);
  }
  ctx->op = psa_hash_operation_init();
  return psa_ret == PSA_SUCCESS && hash_len == 64;
#else
  int ret = mbedtls_sha512_finish(&ctx->ctx, out);
  mbedtls_sha512_free(&ctx->ctx);
  return ret == 0;
#endif
}

static void update_sha512_abort(UpdateSHA512Context *ctx) {
  if (!ctx) {
    return;
  }
#if MBEDTLS_VERSION_MAJOR >= 4
  psa_hash_abort(&ctx->op);
  ctx->op = psa_hash_operation_init();
#else
  mbedtls_sha512_free(&ctx->ctx);
#endif
}

static void updateSHA512FreeContext(void *&ctx) {
  if (ctx) {
    update_sha512_abort(static_cast<UpdateSHA512Context *>(ctx));
    delete static_cast<UpdateSHA512Context *>(ctx);
    ctx = nullptr;
  }
}

static bool updateSHA512Update(void *&ctx, const uint8_t *data, size_t len) {
  if (!ctx) {
    return true;
  }
  if (!update_sha512_update(static_cast<UpdateSHA512Context *>(ctx), data, len)) {
    log_e("SHA-512 update failed");
    updateSHA512FreeContext(ctx);
    return false;
  }
  return true;
}

static bool updateSHA512Finish(void *&ctx, uint8_t *result, bool &valid) {
  if (!ctx) {
    return false;
  }
  UpdateSHA512Context *sha512_ctx = static_cast<UpdateSHA512Context *>(ctx);
  uint8_t digest[64];
  bool ok = update_sha512_finish(sha512_ctx, digest);
  if (ok) {
    ok = memcmp(sha512_ctx->expected, digest, sizeof(digest)) == 0;
  }
  // Streaming state is consumed by finish; drop the heap object either way.
  delete sha512_ctx;
  ctx = nullptr;
  if (!ok) {
    valid = false;
    return false;
  }
  memcpy(result, digest, 64);
  return true;
}

bool UpdateClass::setSHA512(
  const char *expected_sha512
#ifndef UPDATE_NOCRYPT
  ,
  bool calc_post_decryption
#endif /* UPDATE_NOCRYPT */
) {
  if (!expected_sha512 || strlen(expected_sha512) != 128 || !HEXBuilder::isHexString(expected_sha512, 128)) {
    return false;
  }
  // Digest must cover the entire payload; reject once writing has started.
  if (_progress > 0 || _bufferLen > 0) {
    log_e("setSHA512 must be called before writing data");
    return false;
  }
  if (!isRunning()) {
    log_e("setSHA512 requires an active update (call begin first)");
    return false;
  }

  UpdateSHA512Context *ctx = new (std::nothrow) UpdateSHA512Context{};
  if (!ctx) {
    log_e("Failed to allocate SHA-512 context");
    return false;
  }
  if (!update_sha512_begin(ctx)) {
    delete ctx;
    return false;
  }

  static const SHAOps ops = {
    updateSHA512FreeContext,
    updateSHA512Update,
    updateSHA512Finish,
  };

  HEXBuilder::hex2bytes(ctx->expected, sizeof(ctx->expected), expected_sha512);
  if (_sha512Ops) {
    _sha512Ops->freeContext(_sha512_ctx);
  }
  _sha512_valid = false;
  memset(_sha512_result, 0, sizeof(_sha512_result));
  _sha512_ctx = ctx;
  _sha512Ops = &ops;
#ifndef UPDATE_NOCRYPT
  _target_sha512_decrypted = calc_post_decryption;
#endif /* UPDATE_NOCRYPT */
  return true;
}
