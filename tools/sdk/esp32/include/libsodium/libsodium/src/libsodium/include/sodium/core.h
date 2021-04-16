
#ifndef sodium_core_H
#define sodium_core_H

#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

SODIUM_EXPORT
int sodium_init(void)
            __attribute__ ((warn_unused_result));

#ifdef __cplusplus
}
#endif

#endif
