#include "mbedtls/config.h"
#include "mbedtls/error.h"
#include "mbedtls/asn1.h"
#include "mbedtls/error.h"
#include "mbedtls/oid.h"
#include "mbedtls/config.h"
#include "mbedtls/error.h"
#include "mbedtls/asn1.h"
#include "mbedtls/error.h"
#include "mbedtls/oid.h"
#include "mbedtls/x509.h"
#include "mbedtls/x509_crt.h"
#include <mbedtls/sha256.h>
#include <mbedtls/sha1.h>
#include <assert.h>

#ifndef _H_SIGN_INFO
#define _H_SIGN_INFO

/* As used for S/MIME, RFC3161 and various others

   See: RFC 5652 - Cryptographic Message Syntax (CMS) 

   SignerInfo ::= SEQUENCE {
      version CMSVersion,
      sid SignerIdentifier,
      digestAlgorithm DigestAlgorithmIdentifier,
      signedAttrs [0] IMPLICIT SignedAttributes OPTIONAL,
      signatureAlgorithm SignatureAlgorithmIdentifier,
      signature SignatureValue,
      unsignedAttrs [1] IMPLICIT UnsignedAttributes OPTIONAL }

    SignerIdentifier ::= CHOICE {
      issuerAndSerialNumber IssuerAndSerialNumber,
      subjectKeyIdentifier [0] SubjectKeyIdentifier }

    SignedAttributes ::= SET SIZE (1..MAX) OF Attribute

    UnsignedAttributes ::= SET SIZE (1..MAX) OF Attribute

    Attribute ::= SEQUENCE {
      attrType OBJECT IDENTIFIER,
      attrValues SET OF AttributeValue }

    IssuerAndSerialNumber ::= SEQUENCE {
      issuer Name,
      serialNumber CertificateSerialNumber }

    AttributeValue ::= ANY

    SignatureValue ::= OCTET STRING
*/

typedef struct mbedtls_asn1_ts_pki_signer_info {

  // information on who placed the signature
  //
  mbedtls_x509_name sid_name;
  mbedtls_asn1_bitstring sid_serial;

  // signature digest that is signed
  //.
  mbedtls_md_type_t sig_digest_type;
  unsigned char * sig_digest;
  size_t sig_digest_len;

  // signature that was placed
  //
  mbedtls_pk_type_t sig_type;
  unsigned char * sig;
  size_t sig_len;

  // area that was signed
  //
  unsigned char * signed_attribute_raw;
  size_t signed_attribute_len;

  size_t signing_cert_hash_len = 0;
  mbedtls_md_type_t signing_cert_hash_type;
  unsigned char * signing_cert_hash;
  
} mbedtls_asn1_ts_pki_signer_info;

extern int mbedtls_ts_get_signer_info(unsigned char **p, unsigned char * end, mbedtls_asn1_ts_pki_signer_info * signer_info);
#endif
