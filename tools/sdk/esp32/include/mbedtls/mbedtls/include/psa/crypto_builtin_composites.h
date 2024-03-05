/*
 *  Context structure declaration of the Mbed TLS software-based PSA drivers
 *  called through the PSA Crypto driver dispatch layer.
 *  This file contains the context structures of those algorithms which need to
 *  rely on other algorithms, i.e. are 'composite' algorithms.
 *
 * \note This file may not be included directly. Applications must
 * include psa/crypto.h.
 *
 * \note This header and its content is not part of the Mbed TLS API and
 * applications must not depend on it. Its main purpose is to define the
 * multi-part state objects of the Mbed TLS software-based PSA drivers. The
 * definition of these objects are then used by crypto_struct.h to define the
 * implementation-defined types of PSA multi-part state objects.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_BUILTIN_COMPOSITES_H
#define PSA_CRYPTO_BUILTIN_COMPOSITES_H

#include <psa/crypto_driver_common.h>

/*
 * MAC multi-part operation definitions.
 */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CMAC) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_HMAC)
#define MBEDTLS_PSA_BUILTIN_MAC
#endif

#if defined(MBEDTLS_PSA_BUILTIN_ALG_GCM) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_CCM) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_CHACHA20_POLY1305)
#define MBEDTLS_PSA_BUILTIN_AEAD  1
#endif

#if defined(MBEDTLS_PSA_BUILTIN_ALG_HMAC) || defined(PSA_CRYPTO_DRIVER_TEST)
typedef struct {
    /** The HMAC algorithm in use */
    psa_algorithm_t alg;
    /** The hash context. */
    struct psa_hash_operation_s hash_ctx;
    /** The HMAC part of the context. */
    uint8_t opad[PSA_HMAC_MAX_HASH_BLOCK_SIZE];
} mbedtls_psa_hmac_operation_t;

#define MBEDTLS_PSA_HMAC_OPERATION_INIT { 0, PSA_HASH_OPERATION_INIT, { 0 } }
#endif /* MBEDTLS_PSA_BUILTIN_ALG_HMAC */

#include "mbedtls/cmac.h"

typedef struct {
    psa_algorithm_t alg;
    union {
        unsigned dummy; /* Make the union non-empty even with no supported algorithms. */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_HMAC) || defined(PSA_CRYPTO_DRIVER_TEST)
        mbedtls_psa_hmac_operation_t hmac;
#endif /* MBEDTLS_PSA_BUILTIN_ALG_HMAC */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CMAC) || defined(PSA_CRYPTO_DRIVER_TEST)
        mbedtls_cipher_context_t cmac;
#endif /* MBEDTLS_PSA_BUILTIN_ALG_CMAC */
    } ctx;
} mbedtls_psa_mac_operation_t;

#define MBEDTLS_PSA_MAC_OPERATION_INIT { 0, { 0 } }

#endif /* PSA_CRYPTO_BUILTIN_COMPOSITES_H */
