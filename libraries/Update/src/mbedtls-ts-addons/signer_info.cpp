#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>

#include "x509_ts_utils.h"
#include "ts.h"


#ifdef _TS_DEBUG
#define CHKR(r) { if ( ( ret = (r) ) != 0 )  { assert(0); return ret; };  }
#else
#define CHKR(r) { if ( ( ret = (r) ) != 0 )  { return ret; };  }
#endif
/*
   SignerInfo ::= SEQUENCE
      version CMSVersion,
      sid SignerIdentifier,
      digestAlgorithm DigestAlgorithmIdentifier,
      signedAttrs [0] IMPLICIT SignedAttributes OPTIONAL,
      signatureAlgorithm SignatureAlgorithmIdentifier,
      signature SignatureValue,
      unsignedAttrs [1] IMPLICIT UnsignedAttributes OPTIONAL

    SignerIdentifier ::= CHOICE
      issuerAndSerialNumber IssuerAndSerialNumber,
      subjectKeyIdentifier [0] SubjectKeyIdentifier

    SignedAttributes ::= SET SIZE (1..MAX) OF Attribute

    UnsignedAttributes ::= SET SIZE (1..MAX) OF Attribute

    Attribute ::= SEQUENCE
      attrType OBJECT IDENTIFIER,
      attrValues SET OF AttributeValue

    IssuerAndSerialNumber ::= SEQUENCE
      issuer Name,
      serialNumber CertificateSerialNumber

    AttributeValue ::= ANY

    SignatureValue ::= OCTET STRING
*/
int mbedtls_ts_get_signer_info(unsigned char **p, unsigned char * end, mbedtls_asn1_ts_pki_signer_info * signer_info)
{
  unsigned char *sap = NULL, *eap = NULL;
  int sigVersion;
  size_t len;
  int ret = MBEDTLS_ERR_X509_INVALID_FORMAT;


  mbedtls_asn1_ts_pki_signer_info out;
  bzero(&out, sizeof(out));

  CHKR(mbedtls_asn1_get_tag( p, end, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE) );

  eap = *p + len;

  CHKR(mbedtls_asn1_get_int( p, end, &sigVersion));


  switch (sigVersion) {
    case 1:
      CHKR(mbedtls_asn1_get_tag( p, end, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE) );
      CHKR( mbedtls_asn1_get_tag( p, end, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE) );
      CHKR(mbedtls_x509_get_name( p, *p + len, &(out.sid_name)));

#ifdef _TS_DEBUG
      {
        char s[1024];
        mbedtls_x509_dn_gets(s, sizeof(s), &(out.sid_name));
        _TS_DEBUG_PRINTF("SID name %s", s);
      };
#endif

      CHKR(mbedtls_asn1_get_serial_bitstring( p, end, &(out.sid_serial)));
      break;
    case 3:
      // return unsuppoted
      assert(1); // todo !
      break;
    default:
      assert(1);
      return  ( MBEDTLS_ERR_TS_REPLY_INVALID_FORMAT);
      break;
  }

  _TS_DEBUG_PRINTF("DigestAlgorithmIdentifier:");

  CHKR(mbedtls_asn1_get_algorithm_itentifiers(p, *p + len, (int*) & (out.sig_digest_type)));

  out.signing_cert_hash_type = MBEDTLS_MD_SHA1; // Default for for V1 - ss rfc 5035, section 5.4
  sap = *p;
  if (mbedtls_asn1_get_tag( p, end, &len,
                            MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_CONTEXT_SPECIFIC) == 0) {

    // i.e. the block that is signed;
    out.signed_attribute_len = len + *p - sap;
    out.signed_attribute_raw = sap;

    unsigned char *ep = *p + len;
    _TS_DEBUG_PRINTF("Signed attributes:");
    while (*p < ep) {
      /* Attribute ::= SEQUENCE {
          attrType OBJECT IDENTIFIER,
          attrValues SET OF AttributeValue
          AttributeValue ::= ANY
        }
      */
      CHKR(mbedtls_asn1_get_tag( p, ep, &len,
                                 MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE));

      unsigned char *eap = *p + len;

      CHKR(mbedtls_asn1_get_tag(p, ep, &len, MBEDTLS_ASN1_OID) );

      _TS_DEBUG_PRINTF("  %s:", _oidbuff2str(*p, len));

      mbedtls_x509_buf oid;
      oid.p = *p;
      oid.len = len;
      *p += len;

      CHKR(mbedtls_asn1_get_tag(p, eap, &len,
                                MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SET) );

      unsigned char *evap = *p + len;

      // Is it the digest - as that is the one we need.
      if ( MBEDTLS_OID_CMP( MBEDTLS_OID_PKCS9_MESSAGE_DIGEST, &oid ) == 0 ) {
        CHKR(out.sig_digest != NULL);
        CHKR(mbedtls_asn1_get_tag(p, evap, &len, MBEDTLS_ASN1_OCTET_STRING) );

        // Only pick it if it is longer.
        if (len > out.sig_digest_len) {
          out.sig_digest = *p;
          out.sig_digest_len = len;
        };
      }
      else if ( MBEDTLS_OID_CMP( MBEDTLS_OID_PKCS9_SMIME_AA_SIGNING_CERT, &oid ) == 0 ) {
        CHKR(mbedtls_asn1_get_tag(p, evap, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE) );
        CHKR(mbedtls_asn1_get_tag(p, evap, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE) );
        CHKR(mbedtls_asn1_get_tag(p, evap, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE) );
        CHKR(mbedtls_asn1_get_tag(p, evap, &len, MBEDTLS_ASN1_OCTET_STRING) );

        // Only pick it if it is better.
        if (len > out.signing_cert_hash_len) {
          out.signing_cert_hash = *p;
          out.signing_cert_hash_len = len;
        };
      }
      else if ( MBEDTLS_OID_CMP( MBEDTLS_OID_PKCS9_SMIME_AA_SIGNING_CERT_V2, &oid ) == 0 ) {
        /*    SigningCertificateV2 ::=  SEQUENCE
           certs        SEQUENCE OF ESSCertIDv2
           policies     SEQUENCE OF PolicyInformation OPTIONAL

          ESSCertIDv2 ::=  SEQUENCE
            hashAlgorithm           AlgorithmIdentifier   DEFAULT {algorithm id-sha256},
            certHash                 Hash,
            issuerSerial             IssuerSerial OPTIONAL

          Hash ::= OCTET STRING  IssuerSerial ::= SEQUENCE
          issuer                   GeneralNames,
          serialNumber             CertificateSerialNumber
        */
        CHKR(mbedtls_asn1_get_tag(p, evap, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE) );

        mbedtls_md_type_t md_alg;
        CHKR(mbedtls_asn1_get_algorithm_itentifiers(p, evap, (int *)&md_alg));

        out.signing_cert_hash_type = md_alg;

        // Sort of pray for a general name that is a normal hash
        CHKR(mbedtls_asn1_get_tag(p, evap, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE) );
        CHKR(mbedtls_asn1_get_tag(p, evap, &len, MBEDTLS_ASN1_OCTET_STRING) );

        out.signing_cert_hash = *p;
        out.signing_cert_hash_len = len;
      }
      else {
        _TS_DEBUG_PRINTF("   Not decoded -- skipped");
      }
      // skip over all else.
      *p = evap;
    }; // while loop over  signed attributes
  }; // if signed attributes

  _TS_DEBUG_PRINTF("SignatureAlgorithmIdentifier:");
  CHKR(mbedtls_asn1_get_algorithm_itentifiers(p, *p + len, (int *) & (out.sig_type)));

  /*
        signatureAlgorithm SignatureAlgorithmIdentifier,
        signature SignatureValue, <----
        unsignedAttrs [1] IMPLICIT UnsignedAttributes OPTIONAL
  */
  CHKR(mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_OCTET_STRING) );

  out.sig = *p;
  out.sig_len = len;

  /* unsignedAttrs [1] IMPLICIT UnsignedAttributes OPTIONAL
  */
  *p = eap; // skip.

  *signer_info = out;

  return 0;
}
