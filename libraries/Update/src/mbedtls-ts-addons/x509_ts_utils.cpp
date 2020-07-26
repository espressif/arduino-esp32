
#include "mbedtls/x509.h"
#include "mbedtls/x509_crt.h"
#include "x509_ts_utils.h"
#include <assert.h>
#include <ctype.h>

#define mbedtls_free      free
#define mbedtls_calloc    calloc
#define mbedtls_printf    _TS_DEBUG_PRINTF

#ifdef _TS_DEBUG
char * _oid2str(mbedtls_x509_buf *oid) {
  static char s[256];
  s[0] = 0;
  mbedtls_oid_get_numeric_string(s, sizeof(s), oid);
  return s;
};

char * _oidbuff2str(unsigned char *buf, size_t len) {
  static mbedtls_x509_buf oid;
  oid.p = buf;
  oid.len = len;
  return _oid2str(&oid);
};

char * _bitstr(mbedtls_asn1_bitstring *bs) {
  static char s[256];
  int i = 0;
  for (; i < 8 * bs->len - bs->unused_bits && i < sizeof(s) - 1; i++) {
    s[i] = bs->p[i >> 4] & (1 << (i & 7)) ? '1' : '0';
  }
  s[i] = 0;
  return s;
};
#endif


int mbedtls_x509_get_names( unsigned char **p, const unsigned char *end,
                            mbedtls_x509_name *cur )
{
  int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
  size_t set_len = 0;

  if ( ( mbedtls_asn1_get_tag(p, end, &set_len,
                              MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE) ) != 0) {
    return ret;
  };
  unsigned char * ep = *p + set_len;

  ret = mbedtls_x509_get_name(p, *p + set_len, cur);
  if (ret)
    return ret;
  assert(ep == *p);

  return 0;
};
#define CHECK(code) { if( ( ret = ( code ) ) != 0 ) return( ret ); }
#define SKIPLEADINGZERORS(p,len) while ( len > 0 && **p == 0 ) {++( *p );--len;}

int mbedtls_asn1_get_uint64( unsigned char **p,
                             const unsigned char *end,
                             uint64_t *val )
{
  int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
  size_t len;

  if ( ( ret = mbedtls_asn1_get_tag( p, end, &len, MBEDTLS_ASN1_INTEGER ) ) != 0 ) {
    return ( ret );
  };
  unsigned char * ep = *p + len;

  /*
     len==0 is malformed (0 must be represented as 020100 for INTEGER,
     or 0A0100 for ENUMERATED tags
  */
  if ( len == 0 )
    return ( MBEDTLS_ERR_ASN1_INVALID_LENGTH );

  /* This is a cryptography library. Reject negative integers. */
  if ( ( **p & 0x80 ) != 0 )
    return ( MBEDTLS_ERR_X509_INVALID_FORMAT );

  SKIPLEADINGZERORS(p, len);

  if ( len > sizeof( uint64_t ) )
    return ( MBEDTLS_ERR_ASN1_INVALID_LENGTH );

  *val = 0;
  while ( len-- > 0 )
  {
    *val = ( *val << 8 ) | **p;
    (*p)++;
  }

  assert(ep == *p);
  return ( 0 );
}

// Many specs allow /require at least 160 bits these days.
//
int mbedtls_asn1_get_serial_mpi( unsigned char **p,
                                 const unsigned char *end,
                                 mbedtls_mpi *val )
{
  int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
  size_t len;
  mbedtls_mpi_init(val);

  if ( ( ret = mbedtls_asn1_get_tag( p, end, &len, MBEDTLS_ASN1_INTEGER ) ) != 0 )
    return ( ret );
  unsigned char * ep = *p + len;

  /*
     len==0 is malformed (0 must be represented as 020100 for INTEGER,
     or 0A0100 for ENUMERATED tags
  */
  if ( len == 0 )
    return ( MBEDTLS_ERR_ASN1_INVALID_LENGTH );

  /* Serials cannot be negative.. */
  if ( ( **p & 0x80 ) != 0 )
    return ( MBEDTLS_ERR_ASN1_INVALID_DATA );
  val->s = 1;

  SKIPLEADINGZERORS(p, len);

  while ( len-- > 0 )
  {
    if ((ret = mbedtls_mpi_lset(val, **p)) != 0)
      return ret;
    if ((ret = mbedtls_mpi_shift_l(val, 8)) != 0)
      return ret;
    (*p)++;
  }

  assert(ep == *p);

  return ( 0 );
}

int mbedtls_asn1_get_serial_bitstring( unsigned char **p,
                                       const unsigned char *end,
                                       mbedtls_asn1_bitstring *val )
{
  int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
  size_t len;

  if ( ( ret = mbedtls_asn1_get_tag( p, end, &len, MBEDTLS_ASN1_INTEGER ) ) != 0 )
    return ( ret );
  unsigned char * ep = *p + len;

  /*
     len==0 is malformed (0 must be represented as 020100 for INTEGER,
     or 0A0100 for ENUMERATED tags
  */
  if ( len == 0 )
    return ( MBEDTLS_ERR_ASN1_INVALID_LENGTH );

  /* Serials cannot be negative.. */
  if ( ( **p & 0x80 ) != 0 )
    return ( MBEDTLS_ERR_ASN1_INVALID_DATA );

  SKIPLEADINGZERORS(p, len);

  val->unused_bits = 0;
  val->len = len;
  val->p = *p;

  *p += len;

  assert(ep == *p);
  return ( 0 );
}

int x509_parse_int( unsigned char **p, size_t n, int *res )
{
  *res = 0;

  for ( ; n > 0; --n )
  {
    if ( ( **p < '0') || ( **p > '9' ) )
      return ( MBEDTLS_ERR_X509_INVALID_DATE );

    *res *= 10;
    *res += ( *(*p)++ - '0' );
  }

  return ( 0 );
}
int x509_parse_time( unsigned char **p, size_t len, size_t yearlen,
                     mbedtls_x509_time *tm )
{
  int x509_parse_int( unsigned char **p, size_t n, int *res );
  int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
  /*
     Minimum length is 10 or 12 depending on yearlen
  */
  if ( len < yearlen + 8 )
    return ( MBEDTLS_ERR_X509_INVALID_DATE );

  len -= yearlen + 8;

  /*
     Parse year, month, day, hour, minute
  */
  CHECK( x509_parse_int( p, yearlen, &tm->year ) );
  if ( 2 == yearlen )
  {
    if ( tm->year < 50 )
      tm->year += 100;

    tm->year += 1900;
  }

  CHECK( x509_parse_int( p, 2, &tm->mon ) );
  CHECK( x509_parse_int( p, 2, &tm->day ) );
  CHECK( x509_parse_int( p, 2, &tm->hour ) );
  CHECK( x509_parse_int( p, 2, &tm->min ) );

  /*
     Parse seconds if present
  */
  if ( len >= 2 )
  {
    CHECK( x509_parse_int( p, 2, &tm->sec ) );
    len -= 2;

    /* Parse any fractional seconds
    */
    if (len > 1 && (*p)[0] == '.') {
      float millis = 0, d = 0.1;
      (*p)++; len--;

      while (len && isdigit((*p)[0])) {
        int k = ((*p)[0]) - '0';
        millis += k * d;
        d /= 10.;
        (*p)++; len--;
      }
    }
  };

  /*
     Parse trailing 'Z' if present
  */
  if ( len > 0 && 'Z' == **p )
  {
    (*p)++;
    len--;

  }
  // sometime we get some timezone cruft.
  // skipp all that.
  if (len) {
    (*p) += len;
    len  = 0;
  };

  if ( 0 != len ) {
    _TS_DEBUG_PRINTF("At the end with %d left %c %c %c", len, (*p)[0], (*p)[1], (*p)[2]);
    return ( MBEDTLS_ERR_X509_INVALID_DATE );
  };
  // CHECK( x509_date_is_valid( tm ) );

  return ( 0 );
}

int mbedtls_asn1_get_algorithm_itentifier(unsigned char **p, unsigned char * end, int * alg)
{
  size_t len;
  int ret;
  *alg = -1;

  if ( ( ret = mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_OID) ) != 0 ) {
    assert(0);
    return ( MBEDTLS_ERR_X509_INVALID_FORMAT );
  }
  unsigned char * ep = *p + len;

  mbedtls_x509_buf oid;
  oid.p = *p;
  oid.len = len;
  _TS_DEBUG_PRINTF("algoritm OID: %s", _oid2str(&oid));

  mbedtls_md_type_t md_alg;
  mbedtls_pk_type_t pk_alg;

  if ( ( ret = mbedtls_oid_get_md_alg(&oid, &md_alg)) == 0) {
    _TS_DEBUG_PRINTF("   MD algoritm internal num: %d %s", md_alg, mbedtls_md_get_name(mbedtls_md_info_from_type(md_alg)));
    *alg = (int) md_alg;
  } else if ( ( ret = mbedtls_oid_get_pk_alg(&oid, &pk_alg)) == 0) {
    _TS_DEBUG_PRINTF("   PK algoritm internal num: %d", pk_alg); //, mbedtls_pk_get_name(mbedtls_pk_info_from_type(pk_alg)));
    *alg = (int) pk_alg;
  } else {
    _TS_DEBUG_PRINTF("Unkn OID");
    *alg = -1;
  }
  *p += len;

  assert(ep == *p);
  return 0;
}

int mbedtls_asn1_get_algorithm_itentifiers(unsigned char **p, unsigned char * end,  int * alg)
{
  size_t len;
  int ret;
  *alg = -1;

  if ( ( ret = mbedtls_asn1_get_tag( p, end, &len,
                                     MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE ) ) != 0 ) {
    assert(0);
    return ( MBEDTLS_ERR_X509_INVALID_FORMAT );
  }
  unsigned char * ep = *p + len;

  while (*p < ep) {
    if ( mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_NULL) == 0 ) {
      break;
    };
    int out = -1;
    if (0 == mbedtls_asn1_get_algorithm_itentifier(p, end, &out) && out != -1)
      *alg = out;
  };

  assert(ep == *p);
  return 0;
}

int mbedtls_asn1_get_certificate_set(unsigned char **p, unsigned char * end, mbedtls_x509_crt ** chain) {
  size_t len = end - *p;

  // We allow for this function to be called multiple times (e.g. for a multi-party
  // nested signature in S/MIME).
  //
  if (*chain == NULL) {
    if ((*chain = (mbedtls_x509_crt *)mbedtls_calloc( 1, sizeof(mbedtls_x509_crt))) == NULL)
      return MBEDTLS_ERR_X509_ALLOC_FAILED;
    mbedtls_x509_crt_init(*chain);
  };

  while (mbedtls_x509_crt_parse_der(*chain, *p, len) == 0) { /* and end check !*/
    // We keep them in the order as given on the wire. This means that usually
    // the leaf is first; followed by the chain to the top. But not always - quite a
    // few german institutions do it the other way round.
    //
    // But given that some callers will need this set to be ordered - we do not try
    // to order them 'right' just yet.
    //
    mbedtls_x509_crt * crt;
    for (crt = *chain; crt->next; crt = crt->next);

    // Bit of a cheat - we do not get the actual legth eaten returned; so
    // get it from the raw length.
    //
    *p += crt->raw.len;
    len -= crt->raw.len;

  }; /* while there are still certs in the sequence */

#ifdef _TS_DEBUG
  int i = 0;
  for (mbedtls_x509_crt * crt = *chain; crt; crt = crt->next) i++;
  _TS_DEBUG_PRINTF("Extracted %d certs", i);
#endif

  assert(end == *p);
  return 0;
};
