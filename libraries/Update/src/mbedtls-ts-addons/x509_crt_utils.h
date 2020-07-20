#include "mbedtls/config.h"
#include "mbedtls/error.h"
#include "mbedtls/x509.h"
#include "mbedtls/x509_crt.h"
#include <mbedtls/sha256.h>
#include <mbedtls/md.h>
#include <assert.h>

#ifndef _H_X509_CRT_UTILS
#define _H_X509_CRT_UTILS

int mbedtls_x509_crt_fprint(char * buff, size_t len, char * prefixOrNull, mbedtls_x509_crt * cert, mbedtls_md_type_t typeOrNull);
#endif

