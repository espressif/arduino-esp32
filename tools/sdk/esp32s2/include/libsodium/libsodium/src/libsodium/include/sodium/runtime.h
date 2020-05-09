
#ifndef sodium_runtime_H
#define sodium_runtime_H

#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

SODIUM_EXPORT
int sodium_runtime_has_neon(void);

SODIUM_EXPORT
int sodium_runtime_has_sse2(void);

SODIUM_EXPORT
int sodium_runtime_has_sse3(void);

SODIUM_EXPORT
int sodium_runtime_has_ssse3(void);

SODIUM_EXPORT
int sodium_runtime_has_sse41(void);

SODIUM_EXPORT
int sodium_runtime_has_avx(void);

SODIUM_EXPORT
int sodium_runtime_has_avx2(void);

SODIUM_EXPORT
int sodium_runtime_has_pclmul(void);

SODIUM_EXPORT
int sodium_runtime_has_aesni(void);

/* ------------------------------------------------------------------------- */

int _sodium_runtime_get_cpu_features(void);

#ifdef __cplusplus
}
#endif

#endif
