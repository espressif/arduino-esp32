#include "mbedtls/config.h"
#include "mbedtls/error.h"
#include "mbedtls/x509.h"
#include "mbedtls/x509_crt.h"
#include <mbedtls/sha256.h>
#include <mbedtls/md.h>
#include <assert.h>

#ifndef _H_X509_CRT_UTILS
#define _H_X509_CRT_UTILS

/**
 * \brief           This function returns a buffer with the fingerprint of
 *                  a certificate (i.e. taken of the raw DER encoded blob)
 *                  and outputs this in the same style as OpenSSL.
 *
 * \param buf      Buffer to write to
 * \param size     Maximum size of buffer
 * \param prefix   A line prefix
 * \param crt      The X509 certificate to fingerprint.
 * \param mdtype   The digest to use; if set to MBEDTLS_MD_NONE the default
 *                 SHA256 will be used.
 *
 * \return         The length of the string written (not including the
 *                 terminated nul byte), or a negative error code.
 */
int mbedtls_x509_crt_fprint(char * buf, size_t size, const char * prefix, const mbedtls_x509_crt * crt, mbedtls_md_type_t mdtype);
#endif

