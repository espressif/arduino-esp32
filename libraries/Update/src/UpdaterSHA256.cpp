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
#include "mbedtls/sha256.h"
#endif /* MBEDTLS_VERSION_MAJOR >= 4 */

// Optional SHA-256 streaming hash. Linked only when a sketch calls setSHA256()
// (referenced via a function-pointer table assigned in that setter), so
// --gc-sections can drop mbedtls/PSA SHA-256 from Update sketches that never
// enable it.

struct UpdateSHA256Context {
#if MBEDTLS_VERSION_MAJOR >= 4
  psa_hash_operation_t op;
#else
  mbedtls_sha256_context ctx;
#endif
  uint8_t expected[32];
};

static bool update_sha256_begin(UpdateSHA256Context *ctx) {
#if MBEDTLS_VERSION_MAJOR >= 4
  psa_status_t psa_ret = psa_crypto_init();
  if (psa_ret != PSA_SUCCESS) {
    log_e("PSA crypto init failed: %d", (int)psa_ret);
    return false;
  }
  ctx->op = psa_hash_operation_init();
  psa_ret = psa_hash_setup(&ctx->op, PSA_ALG_SHA_256);
  if (psa_ret != PSA_SUCCESS) {
    log_e("PSA hash setup failed: %d", (int)psa_ret);
    psa_hash_abort(&ctx->op);
    ctx->op = psa_hash_operation_init();
    return false;
  }
  return true;
#else
  mbedtls_sha256_init(&ctx->ctx);
  if (mbedtls_sha256_starts(&ctx->ctx, 0) != 0) {
    mbedtls_sha256_free(&ctx->ctx);
    return false;
  }
  return true;
#endif
}

static bool update_sha256_update(UpdateSHA256Context *ctx, const uint8_t *data, size_t len) {
  if (!ctx || !data || len == 0) {
    return true;
  }
#if MBEDTLS_VERSION_MAJOR >= 4
  return psa_hash_update(&ctx->op, data, len) == PSA_SUCCESS;
#else
  return mbedtls_sha256_update(&ctx->ctx, data, len) == 0;
#endif
}

static bool update_sha256_finish(UpdateSHA256Context *ctx, uint8_t out[32]) {
  if (!ctx || !out) {
    return false;
  }
#if MBEDTLS_VERSION_MAJOR >= 4
  size_t hash_len = 0;
  psa_status_t psa_ret = psa_hash_finish(&ctx->op, out, 32, &hash_len);
  if (psa_ret != PSA_SUCCESS || hash_len != 32) {
    psa_hash_abort(&ctx->op);
  }
  ctx->op = psa_hash_operation_init();
  return psa_ret == PSA_SUCCESS && hash_len == 32;
#else
  int ret = mbedtls_sha256_finish(&ctx->ctx, out);
  mbedtls_sha256_free(&ctx->ctx);
  return ret == 0;
#endif
}

static void update_sha256_abort(UpdateSHA256Context *ctx) {
  if (!ctx) {
    return;
  }
#if MBEDTLS_VERSION_MAJOR >= 4
  psa_hash_abort(&ctx->op);
  ctx->op = psa_hash_operation_init();
#else
  mbedtls_sha256_free(&ctx->ctx);
#endif
}

static void updateSHA256FreeContext(void *&ctx) {
  if (ctx) {
    update_sha256_abort(static_cast<UpdateSHA256Context *>(ctx));
    delete static_cast<UpdateSHA256Context *>(ctx);
    ctx = nullptr;
  }
}

static bool updateSHA256Update(void *&ctx, const uint8_t *data, size_t len) {
  if (!ctx) {
    return true;
  }
  if (!update_sha256_update(static_cast<UpdateSHA256Context *>(ctx), data, len)) {
    log_e("SHA-256 update failed");
    updateSHA256FreeContext(ctx);
    return false;
  }
  return true;
}

static bool updateSHA256Finish(void *&ctx, uint8_t *result, bool &valid) {
  if (!ctx) {
    return false;
  }
  UpdateSHA256Context *sha256_ctx = static_cast<UpdateSHA256Context *>(ctx);
  uint8_t digest[32];
  bool ok = update_sha256_finish(sha256_ctx, digest);
  if (ok) {
    ok = memcmp(sha256_ctx->expected, digest, sizeof(digest)) == 0;
  }
  // Streaming state is consumed by finish; drop the heap object either way.
  delete sha256_ctx;
  ctx = nullptr;
  if (!ok) {
    valid = false;
    return false;
  }
  memcpy(result, digest, 32);
  return true;
}

bool UpdateClass::setSHA256(
  const char *expected_sha256
#ifndef UPDATE_NOCRYPT
  ,
  bool calc_post_decryption
#endif /* UPDATE_NOCRYPT */
) {
  if (!expected_sha256 || strlen(expected_sha256) != 64 || !HEXBuilder::isHexString(expected_sha256, 64)) {
    return false;
  }
  // Digest must cover the entire payload; reject once writing has started.
  if (_progress > 0 || _bufferLen > 0) {
    log_e("setSHA256 must be called before writing data");
    return false;
  }
  if (!isRunning()) {
    log_e("setSHA256 requires an active update (call begin first)");
    return false;
  }

  UpdateSHA256Context *ctx = new (std::nothrow) UpdateSHA256Context{};
  if (!ctx) {
    log_e("Failed to allocate SHA-256 context");
    return false;
  }
  if (!update_sha256_begin(ctx)) {
    delete ctx;
    return false;
  }

  static const SHAOps ops = {
    updateSHA256FreeContext,
    updateSHA256Update,
    updateSHA256Finish,
  };

  HEXBuilder::hex2bytes(ctx->expected, sizeof(ctx->expected), expected_sha256);
  if (_sha256Ops) {
    _sha256Ops->freeContext(_sha256_ctx);
  }
  _sha256_valid = false;
  memset(_sha256_result, 0, sizeof(_sha256_result));
  _sha256_ctx = ctx;
  _sha256Ops = &ops;
#ifndef UPDATE_NOCRYPT
  _target_sha256_decrypted = calc_post_decryption;
#endif /* UPDATE_NOCRYPT */
  return true;
}
