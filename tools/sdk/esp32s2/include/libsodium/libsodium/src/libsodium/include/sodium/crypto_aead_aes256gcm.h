#ifndef crypto_aead_aes256gcm_H
#define crypto_aead_aes256gcm_H

#include <stddef.h>
#include "export.h"

#ifdef __cplusplus
# ifdef __GNUC__
#  pragma GCC diagnostic ignored "-Wlong-long"
# endif
extern "C" {
#endif

SODIUM_EXPORT
int crypto_aead_aes256gcm_is_available(void);

#define crypto_aead_aes256gcm_KEYBYTES  32U
SODIUM_EXPORT
size_t crypto_aead_aes256gcm_keybytes(void);

#define crypto_aead_aes256gcm_NSECBYTES 0U
SODIUM_EXPORT
size_t crypto_aead_aes256gcm_nsecbytes(void);

#define crypto_aead_aes256gcm_NPUBBYTES 12U
SODIUM_EXPORT
size_t crypto_aead_aes256gcm_npubbytes(void);

#define crypto_aead_aes256gcm_ABYTES    16U
SODIUM_EXPORT
size_t crypto_aead_aes256gcm_abytes(void);

typedef CRYPTO_ALIGN(16) unsigned char crypto_aead_aes256gcm_state[512];

SODIUM_EXPORT
size_t crypto_aead_aes256gcm_statebytes(void);

SODIUM_EXPORT
int crypto_aead_aes256gcm_encrypt(unsigned char *c,
                                  unsigned long long *clen_p,
                                  const unsigned char *m,
                                  unsigned long long mlen,
                                  const unsigned char *ad,
                                  unsigned long long adlen,
                                  const unsigned char *nsec,
                                  const unsigned char *npub,
                                  const unsigned char *k);

SODIUM_EXPORT
int crypto_aead_aes256gcm_decrypt(unsigned char *m,
                                  unsigned long long *mlen_p,
                                  unsigned char *nsec,
                                  const unsigned char *c,
                                  unsigned long long clen,
                                  const unsigned char *ad,
                                  unsigned long long adlen,
                                  const unsigned char *npub,
                                  const unsigned char *k)
            __attribute__ ((warn_unused_result));

SODIUM_EXPORT
int crypto_aead_aes256gcm_encrypt_detached(unsigned char *c,
                                           unsigned char *mac,
                                           unsigned long long *maclen_p,
                                           const unsigned char *m,
                                           unsigned long long mlen,
                                           const unsigned char *ad,
                                           unsigned long long adlen,
                                           const unsigned char *nsec,
                                           const unsigned char *npub,
                                           const unsigned char *k);

SODIUM_EXPORT
int crypto_aead_aes256gcm_decrypt_detached(unsigned char *m,
                                           unsigned char *nsec,
                                           const unsigned char *c,
                                           unsigned long long clen,
                                           const unsigned char *mac,
                                           const unsigned char *ad,
                                           unsigned long long adlen,
                                           const unsigned char *npub,
                                           const unsigned char *k)
        __attribute__ ((warn_unused_result));

/* -- Precomputation interface -- */

SODIUM_EXPORT
int crypto_aead_aes256gcm_beforenm(crypto_aead_aes256gcm_state *ctx_,
                                   const unsigned char *k);

SODIUM_EXPORT
int crypto_aead_aes256gcm_encrypt_afternm(unsigned char *c,
                                          unsigned long long *clen_p,
                                          const unsigned char *m,
                                          unsigned long long mlen,
                                          const unsigned char *ad,
                                          unsigned long long adlen,
                                          const unsigned char *nsec,
                                          const unsigned char *npub,
                                          const crypto_aead_aes256gcm_state *ctx_);

SODIUM_EXPORT
int crypto_aead_aes256gcm_decrypt_afternm(unsigned char *m,
                                          unsigned long long *mlen_p,
                                          unsigned char *nsec,
                                          const unsigned char *c,
                                          unsigned long long clen,
                                          const unsigned char *ad,
                                          unsigned long long adlen,
                                          const unsigned char *npub,
                                          const crypto_aead_aes256gcm_state *ctx_)
            __attribute__ ((warn_unused_result));

SODIUM_EXPORT
int crypto_aead_aes256gcm_encrypt_detached_afternm(unsigned char *c,
                                                   unsigned char *mac,
                                                   unsigned long long *maclen_p,
                                                   const unsigned char *m,
                                                   unsigned long long mlen,
                                                   const unsigned char *ad,
                                                   unsigned long long adlen,
                                                   const unsigned char *nsec,
                                                   const unsigned char *npub,
                                                   const crypto_aead_aes256gcm_state *ctx_);

SODIUM_EXPORT
int crypto_aead_aes256gcm_decrypt_detached_afternm(unsigned char *m,
                                                   unsigned char *nsec,
                                                   const unsigned char *c,
                                                   unsigned long long clen,
                                                   const unsigned char *mac,
                                                   const unsigned char *ad,
                                                   unsigned long long adlen,
                                                   const unsigned char *npub,
                                                   const crypto_aead_aes256gcm_state *ctx_)
        __attribute__ ((warn_unused_result));

SODIUM_EXPORT
void crypto_aead_aes256gcm_keygen(unsigned char k[crypto_aead_aes256gcm_KEYBYTES]);

#ifdef __cplusplus
}
#endif

#endif
