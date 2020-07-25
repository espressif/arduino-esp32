#include "mbedtls/config.h"
#include "mbedtls/error.h"
#include "mbedtls/asn1.h"
#include "mbedtls/oid.h"
#include "mbedtls/x509.h"
#include "mbedtls/x509_crt.h"

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
#define _TS_DEBUG jipjip
#endif

#ifndef _X509_TS_UTILS
#define _X509_TS_UTILS

int mbedtls_asn1_get_serial_bitstring( unsigned char **p,
                                       const unsigned char *end,
                                       mbedtls_asn1_bitstring *val );
int mbedtls_x509_get_names( unsigned char **p, const unsigned char *end,
                            mbedtls_x509_name *cur );
int mbedtls_asn1_get_uint64( unsigned char **p,
                             const unsigned char *end,
                             uint64_t *val );
int mbedtls_asn1_get_serial_mpi( unsigned char **p,
                                        const unsigned char *end,
                                        mbedtls_mpi *val );
int mbedtls_asn1_get_algorithm_itentifier(unsigned char **p, unsigned char * end, int * alg);
int mbedtls_asn1_get_algorithm_itentifiers(unsigned char **p, unsigned char * end,  int * alg);
int mbedtls_asn1_get_certificate_set(unsigned char **p, unsigned char * end, mbedtls_x509_crt ** chain);

int x509_parse_int( unsigned char **p, size_t n, int *res );
int x509_parse_time( unsigned char **p, size_t len, size_t yearlen,
                     mbedtls_x509_time *tm );

#ifndef MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED
#define MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED -0x006E  /**< This is a bug in the library */
#endif

#ifdef _TS_DEBUG
#include <esp32-hal-log.h>
char * _oid2str(mbedtls_x509_buf *oid);
char * _oidbuff2str(unsigned char *buf, size_t len);
char * _bitstr(mbedtls_asn1_bitstring *bs);
#define _TS_DEBUG_PRINTF(...) log_d( __VA_ARGS__ )
#else
#define _TS_DEBUG_PRINTF(...) /* no debugging compiled in */
#endif

#endif
