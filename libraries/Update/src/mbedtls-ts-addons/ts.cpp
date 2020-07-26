
#include <string.h>
#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdio.h>
#include <stdlib.h>
#define mbedtls_free      free
#define mbedtls_calloc    calloc
#define mbedtls_printf    _TS_DEBUG_PRINTF
#define mbedtls_snprintf  _TS_DEBUG_SNPRINTF
#endif

#include "ts.h"
#include "sign.h"
#include <ctype.h>

#ifdef _TS_DEBUG
#define CHKR(r) { if ( ( ret = (r) ) != 0 )  { assert(0); return ret; };  }
#define CHKRE(r,e) { if ( (r)  != 0 )  { assert(0); return e; };  }
#define CHK(r) { if ( ( ret = (r) ) != 0 )  { _TS_DEBUG_PRINTF("At @%d in rfc", p-from); assert(0); goto ts_reply_free_and_exit; };  }
#define CHKE(r,e) { if ( ( (r) ) != 0 )  { _TS_DEBUG_PRINTF("At @%d in rfc", p-from); assert(0); ret = (e); goto ts_reply_free_and_exit; };  }
#else
#define CHKR(r) { if ( ( ret = (r) ) != 0 )  { return ret; };  }
#define CHKRE(r,e) { if ( (r)  != 0 )  { return (e); };  }
#define CHK(r) { if ( ( ret = (r) ) != 0 )  { goto ts_reply_free_and_exit; };  }
#define CHKE(r,e) { if ( ( (r) ) != 0 )  { ret = (e); goto ts_reply_free_and_exit; };  }
#endif

/*
  PKIStatusInfo ::= SEQUENCE {
      status        PKIStatus,
      statusString  PKIFreeText     OPTIONAL,
      failInfo      PKIFailureInfo  OPTIONAL  }

   PKIFailureInfo ::= BIT STRING {
    badAlg               (0),
    -- unrecognized or unsupported Algorithm Identifier
    badRequest           (2),
    -- transaction not permitted or supported
    badDataFormat        (5),
    -- the data submitted has the wrong format
    timeNotAvailable    (14),
    -- the TSA's time source is not available
    unacceptedPolicy    (15),
    -- the requested TSA policy is not supported by the TSA
    unacceptedExtension (16),
    -- the requested extension is not supported by the TSA
    addInfoNotAvailable (17)
    -- the additional information requested could not be understood
    -- or is not available
    systemFailure       (25)
    -- the request cannot be handled due to system failure
    }
*/
int mbedtls_asn1_get_ts_pkistatusinfo(unsigned char **p, unsigned char * end, mbedtls_asn1_ts_pki_status_info * status)
{
  mbedtls_asn1_ts_pki_status_info  out;
  size_t len;
  char statusBuff[2 * 1024];
  int ret = MBEDTLS_ERR_X509_INVALID_FORMAT;

  out.statusString = NULL;
  out.failInfo = -1;
  out.pki_status = -1;

  CHKR(mbedtls_asn1_get_tag( p, end, &len,
                             MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE ));

  unsigned char *endp = *p + len;

  CHKRE((len >= (size_t) (end - *p )), MBEDTLS_ERR_X509_INVALID_FORMAT);

  CHKR(mbedtls_asn1_get_int( p, end, &(out.pki_status)));

  _TS_DEBUG_PRINTF("PKIStatus: %d", out.pki_status);

  if (*p < endp)  {
    if (mbedtls_asn1_get_tag(p, *p + len, &len,
                             MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE ) == 0) {

      // we have a sequence of PKIFreeText
      //      PKIFreeText ::= SEQUENCE SIZE (1..MAX) OF UTF8String
      // which we'll concatenate in one buffer, with '\n' in between.
      //
      unsigned char * endp = *p + len;
      while (*p <  endp) {
        CHKR(mbedtls_asn1_get_tag( p, end, &len,  MBEDTLS_ASN1_UTF8_STRING ));

        int at = 0, l = len;
        if (statusBuff[0]) {
          at = strlen(statusBuff);
          statusBuff[at] = '\n';
          at++;
        };
        if (at + l > sizeof(statusBuff) + 1) {
          l = sizeof(statusBuff) - at - 1;
        }
        if (l > 0) {
          memcpy(statusBuff + at, *p, l);
          statusBuff[l] = '\0';
        };
        *p += len;
      };
      _TS_DEBUG_PRINTF("PKIFreeText: %s", statusBuff);
    }

    mbedtls_x509_bitstring bs;
    if ( mbedtls_asn1_get_bitstring(p, *p + len,  &bs )  == 0 ) {
      _TS_DEBUG_PRINTF("PKIFailureInfo: %s", _bitstr(&bs));
    };

    CHKRE((bs.len > 1), MBEDTLS_ERR_X509_INVALID_FORMAT);

    if (bs.len == 0) {
      out.failInfo = 0;
    } else {
      unsigned char mask = 0xFF;
      mask = mask >> bs.unused_bits;
      out.failInfo = bs.p[0] & mask;
    };
  };

  /* check that we've read the whole PKIStatusInfo */
  CHKRE((*p != endp), MBEDTLS_ERR_X509_INVALID_FORMAT);

  if (statusBuff[0])
    out.statusString = strdup(statusBuff);

  *status = out;
  return 0;
};


/*
   TSTInfo ::= SEQUENCE  {
     version                      INTEGER  { v1(1) }, <---
     policy                       TSAPolicyId,
     messageImprint               MessageImprint,
       -- MUST have the same value as the similar field in
       -- TimeStampReq
     serialNumber                 INTEGER,
      -- Time-Stamping users MUST be ready to accommodate integers
      -- up to 160 bits.
     genTime                      GeneralizedTime,
     accuracy                     Accuracy                 OPTIONAL,
     ordering                     BOOLEAN             DEFAULT FALSE,
     nonce                        INTEGER                  OPTIONAL,
       -- MUST be present if the similar field was present
       -- in TimeStampReq.  In that case it MUST have the same value.
     tsa                          [0] GeneralName          OPTIONAL,
     extensions                   [1] IMPLICIT Extensions  OPTIONAL   }
*/
int mbedtls_ts_get_tsinfo(unsigned char **p, unsigned char * end, mbedtls_asn1_ts_pki_ts_info * ts_info)
{
  mbedtls_asn1_ts_pki_ts_info out;
  unsigned char * end_tsinfo = NULL;
  size_t len;
  int ret = MBEDTLS_ERR_TS_REPLY_INVALID_FORMAT;

  bzero(&out, sizeof(out));

  CHKR(mbedtls_asn1_get_tag( p, end, &len,
                             MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE) );

  end_tsinfo = *p + len;

  //         version                      INTEGER  { v1(1) }, <---
  int v = -1;
  if ( ( ret = mbedtls_asn1_get_int( p, end_tsinfo, &v)) != 0)
    return ret;

  CHKRE((v != 1), MBEDTLS_ERR_X509_INVALID_FORMAT);

  _TS_DEBUG_PRINTF("TSTInfo Version %d", v);

  //       policy                       TSAPolicyId,
  CHKR(mbedtls_asn1_get_tag(p, end_tsinfo, &len, MBEDTLS_ASN1_OID) );

  // ignoring the policy ID.
  *p += len;

  // messageImprint               MessageImprint,
  //    MessageImprint ::= SEQUENCE
  CHKR(mbedtls_asn1_get_tag( p, end_tsinfo, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE) );
  //        digestType
  CHKR(mbedtls_asn1_get_algorithm_itentifiers( p, end_tsinfo, (int *)&out.digest_type) );
  //        hashedMessage                OCTET STRING  }
  CHKR(mbedtls_asn1_get_tag( p, end_tsinfo, &len, MBEDTLS_ASN1_OCTET_STRING) );

  out.payload_digest = *p;
  out.payload_digest_len = len;
  *p += len;

  //      serialNumber                 INTEGER,
  {
    uint64_t serial = 0;
    CHKR(mbedtls_asn1_get_uint64(p, end_tsinfo, &serial));

    _TS_DEBUG_PRINTF("Serial %llu", serial);
  };
  /*
        genTime                      GeneralizedTime,
        accuracy                     Accuracy                 OPTIONAL,
        ordering                     BOOLEAN             DEFAULT FALSE,
  */

  CHKR(mbedtls_asn1_get_tag(p, end_tsinfo, &len, MBEDTLS_ASN1_GENERALIZED_TIME) );

  CHKR(x509_parse_time( p, len, 4, &(out.signed_time)));

  _TS_DEBUG_PRINTF("Signature timestap: %04d-%02d-%02d %02d:%02d:%02d UTC",
                   out.signed_time.year, out.signed_time.mon, out.signed_time.day, out.signed_time.hour, out.signed_time.min, out.signed_time.sec);

  if (mbedtls_asn1_get_tag( p, end_tsinfo, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE) == 0) {
    unsigned char *ep = *p + len;

    // we have accuracy of seconds, millis, micros
    int sec = 0, milli = 0, micro = 0;
    int tryit = mbedtls_asn1_get_int( p, ep, &sec);
    if (tryit == 0) tryit = mbedtls_asn1_get_int( p, ep, &milli);
    if (tryit == 0) tryit = mbedtls_asn1_get_int( p, ep, &micro);

    _TS_DEBUG_PRINTF("Accuracy %d.%d.%d (ignored %d bytes)", sec, milli, micro, ep - *p);

    // skip over any extra cruft (Observed at the BSI.de)
    *p = ep;
  }

  {
    // ordering                     BOOLEAN             DEFAULT FALSE,
    int ordering = 0;
    if (mbedtls_asn1_get_bool( p, end_tsinfo, &ordering) == 0) {
      _TS_DEBUG_PRINTF("Ordering: %d", ordering);
    }
  }
  //  nonce                        INTEGER                  OPTIONAL,
  {
    uint64_t nonce = 0;
    if (mbedtls_asn1_get_uint64( p, end_tsinfo, &nonce) == 0) {
      _TS_DEBUG_PRINTF("Nonce!");
    };
  };

  /*
     tsa [0] GeneralName          OPTIONAL,
       GeneralName ::= CHOICE {
       otherName                       [0]     AnotherName,
       rfc822Name                      [1]     IA5String,
       dNSName                         [2]     IA5String,
       x400Address                     [3]     ORAddress,
       directoryName                   [4]     Name,
       ediPartyName                    [5]     EDIPartyName,
       uniformResourceIdentifier       [6]     IA5String,
       iPAddress                       [7]     OCTET STRING,
       registeredID                    [8]     OBJECT IDENTIFIER }
  */
  if ((((*p)[0]) & 0xF) == 0) {
    // tsa [0] GeneralName          OPTIONAL,
    if ( mbedtls_asn1_get_tag( p, end_tsinfo, &len,
                               MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_CONTEXT_SPECIFIC) == 0) {
      mbedtls_x509_name directoryName;
      int c = (*p)[0];
      CHKR(mbedtls_asn1_get_tag( p, end_tsinfo, &len, c ));
      switch (c & 0x0F) {
        case 4: // directoryName - Name
          if ((ret = mbedtls_x509_get_names(p, *p + len, &directoryName)) != 0)
            return ret;
          char s[1024];
          mbedtls_x509_dn_gets(s, sizeof(s), &directoryName);
          _TS_DEBUG_PRINTF("directoryName: %s", s);
          break;
        default:
          assert(0);
          break;
      };
    };
  };

  //      extensions                   [1] IMPLICIT Extensions  OPTIONAL
  if ((((*p)[0]) & 0xF) == 1)
    if (mbedtls_asn1_get_tag(p, end_tsinfo, &len,
                             MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_CONTEXT_SPECIFIC) == 0) {
      _TS_DEBUG_PRINTF("Has extensions -- len %d (Unparsed %d)", len, end_tsinfo - *p);
      p += len;
    };

  assert(end_tsinfo == *p);

  // skip over anything else.
  *p = end_tsinfo;
  *ts_info = out;

  return 0;
}

int mbedtls_ts_pki_status_info_free(mbedtls_asn1_ts_pki_status_info * status) {
  if (status && status->statusString)
    free(status->statusString);
  return 0;
}
int mbedtls_asn1_ts_pki_ts_info_free(mbedtls_asn1_ts_pki_ts_info * tsinfo) {
  return 0;
}

#if 0
int mbedtls_asn1_ts_pki_time_stamp_token_free(mbedtls_asn1_ts_pki_time_stamp_token * tstoken) {
  if (tstoken && tstoken->chain)
    free(tstoken->chain);
  return 0;
}
#endif

int mbedtls_ts_reply_free(struct mbedtls_ts_reply * reply) {
  if (reply) {
    mbedtls_ts_pki_status_info_free(&(reply->status));
    mbedtls_asn1_ts_pki_ts_info_free(&(reply->ts_info));
    // mbedtls_asn1_ts_pki_time_stamp_token_free(&(reply->timestamptoken));
    if (reply->chain)
      free(reply->chain);
  }
  return 0;
};

int mbedtls_x509_ts_reply_parse_der(const unsigned char **buf, size_t * buflenp, mbedtls_ts_reply * reply)
{
  mbedtls_x509_crt ** chain = &(reply->chain), * crt = NULL;
  unsigned char *p = NULL, *end = NULL;
  int ret = MBEDTLS_ERR_TS_CORRUPTION_DETECTED;
  size_t len;

  // for the local verification.
  size_t signed_data_digest_len = 0;
  unsigned char  signed_data_digest[MBEDTLS_MD_MAX_SIZE];
  mbedtls_md_type_t signed_data_digest_type = MBEDTLS_MD_NONE;

  const mbedtls_md_info_t * md_info = NULL;

  unsigned char * signed_data = NULL;
  size_t signed_data_len = 0;
  mbedtls_md_context_t md;

  if ( reply == NULL || buf == NULL || buflenp == NULL) {
    return ( MBEDTLS_ERR_TS_BAD_INPUT_DATA ); // reuse
  };
  p = (unsigned char *)*buf;
  end = p + *buflenp;
#ifdef _TS_DEBUG
  unsigned char *from = p;
#endif

  /*    TimeStampResp ::= SEQUENCE
    status                  PKIStatusInfo,
    timeStampToken          TimeStampToken     OPTIONAL
  */
  CHK( mbedtls_asn1_get_tag( &p, end, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE ));

  end = p + len;

  CHK(mbedtls_asn1_get_ts_pkistatusinfo( &p, end, &(reply->status)));

  /*
    TimeStampResp ::= SEQUENCE  {
      status                  PKIStatusInfo,
      timeStampToken          TimeStampToken     OPTIONAL  <-----

    TimeStampToken ::= ContentInfo (a sequence)
     -- contentType is id-signedData as defined in [CMS]
     -- content is SignedData as defined in([CMS])
     -- eContentType within SignedData is id-ct-TSTInfo
     -- eContent within SignedData is TSTInfo
  */
  CHK(mbedtls_asn1_get_tag( &p, end, &len,   MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE ));


  // this should be the whole file - reject trailing cruft.
  if ( len != (size_t) ( end - p ) ) {
    ret = ( MBEDTLS_ERR_X509_INVALID_FORMAT );
    goto ts_reply_len_fault_and_exit;
  };

  CHK(mbedtls_asn1_get_tag(&p, end, &len, MBEDTLS_ASN1_OID));

  mbedtls_x509_buf idSig;
  idSig.p = p;
  idSig.len = len;
  p += len;

  _TS_DEBUG_PRINTF("idSig: %3d %s", len, _oid2str(&idSig));

  if ( MBEDTLS_OID_CMP( MBEDTLS_OID_PKCS7_ID_SIGNED, &idSig ) != 0 ) {
    ret = ( MBEDTLS_ERR_X509_INVALID_FORMAT );
    goto ts_reply_free_and_exit;
  }
  /*
         TimeStampToken ::= ContentInfo (a sequence)
     -- contentType is id-signedData as defined in [CMS]
    SignedData ::= SEQUENCE  <----
        version CMSVersion,
        digestAlgorithms DigestAlgorithmIdentifiers,
        encapContentInfo EncapsulatedContentInfo,
        certificates [0] IMPLICIT CertificateSet OPTIONAL,
        crls [1] IMPLICIT RevocationInfoChoices OPTIONAL,
        signerInfos SignerInfos

  */
  CHK(mbedtls_asn1_get_tag( &p, end, &len,
                            MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_CONTEXT_SPECIFIC) );

  CHK(mbedtls_asn1_get_tag( &p, end, &len,
                            MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE ) );

  CHKE(( len > (size_t) ( end - p )), MBEDTLS_ERR_X509_INVALID_FORMAT);

  // version CMSVersion,
  {
    int v;
    CHK(mbedtls_asn1_get_int( &p, end, &v));
    _TS_DEBUG_PRINTF("SignedData Version %d", v);
  };

  CHK(mbedtls_asn1_get_tag( &p, end, &len,
                            MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SET) );

  // DigestAlgorithmIdentifier(s)
  _TS_DEBUG_PRINTF("SignedData DigestAlgorithmIdentifier:");

  CHK(mbedtls_asn1_get_algorithm_itentifiers( &p, p + len, (int *)&signed_data_digest_type) );

  _TS_DEBUG_PRINTF("SignedData DigestAlgorithmIdentifier: %d", signed_data_digest_type);

  CHK(mbedtls_asn1_get_tag( &p, end, &len,
                            MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE ) );

  CHK(mbedtls_asn1_get_tag(&p, end, &len, MBEDTLS_ASN1_OID) );

  mbedtls_x509_buf eContentType;
  eContentType.p = p;
  eContentType.len = len;
  p += len;
  _TS_DEBUG_PRINTF("eContentType: %3d %s", len, _oid2str(&eContentType));

  CHKE((MBEDTLS_OID_CMP( MBEDTLS_OID_STINFO, &eContentType ) != 0 ), MBEDTLS_ERR_X509_INVALID_FORMAT);

  /*
    EncapsulatedContentInfo ::= SEQUENCE
        eContentType ContentType,
        eContent [0] EXPLICIT OCTET STRING OPTIONAL  <----
       -- eContent within SignedData is TSTInfo
  */
  CHK( mbedtls_asn1_get_tag( &p, end, &len,
                             MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_CONTEXT_SPECIFIC) );

  // this is the raw octed string that packages the bit that is 'signed'
  //
  CHK(mbedtls_asn1_get_tag( &p, end, &len,
                            MBEDTLS_ASN1_OCTET_STRING) );

  // Signed area is within the octed string. This is the TS block
  // that contains the digest of our plaintext.
  //
  signed_data = p;
  signed_data_len = len;

  CHK(mbedtls_ts_get_tsinfo(&p, end, &(reply->ts_info)));

  //        certificates [0] IMPLICIT CertificateSet OPTIONAL,
  if (mbedtls_asn1_get_tag( &p, end, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_CONTEXT_SPECIFIC) == 0) {
    CHK(mbedtls_asn1_get_certificate_set(&p, p + len, chain));
  } else {
    _TS_DEBUG_PRINTF("No Certs");
  };

  if ( mbedtls_asn1_get_tag( &p, end, &len,
                             MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_CONTEXT_SPECIFIC) == 0) {
    _TS_DEBUG_PRINTF("Has CRLs(ignored)");
    p += len;
  } else {
    _TS_DEBUG_PRINTF("No CRLs");
  };

  //  signerInfos SignerInfos <--
  // SignerInfos ::= SET OF SignerInfo
  CHK(mbedtls_asn1_get_tag( &p, end, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SET) );

  CHK(mbedtls_ts_get_signer_info(&p, end, &(reply->signer_info)));

  if (reply->signer_info.signing_cert_hash) {
    // XXX set minimal level ? > SHA1 ?
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type( reply->signer_info.signing_cert_hash_type );
    assert(md_info);

    if (reply->signer_info.signing_cert_hash_len != mbedtls_md_get_size(md_info)) {
      _TS_DEBUG_PRINTF("Hash of cert / mismatch %d != %d", reply->signer_info.signing_cert_hash_len, mbedtls_md_get_size(md_info));
      ret = MBEDTLS_ERR_PK_SIG_LEN_MISMATCH;
      goto ts_reply_free_and_exit;
    }

    crt = *chain;
    for (int d = 0; crt; crt = crt->next, d++) {
      uint8_t digest[MBEDTLS_MD_MAX_SIZE];

      mbedtls_md_context_t md;
      mbedtls_md_init(&md);
      mbedtls_md_setup(&md, md_info, 0);

      CHK(mbedtls_md_starts(&md));
      CHK(mbedtls_md_update(&md, crt->raw.p, crt->raw.len));
      CHK(mbedtls_md_finish(&md, digest));

      if (bcmp(digest, reply->signer_info.signing_cert_hash, mbedtls_md_get_size(md_info)) == 0) {
        _TS_DEBUG_PRINTF("Found one that matched");
        if (*chain != crt) {
          // not at the head of the chain; now make it so.
          mbedtls_x509_crt * parent;
          for (parent = *chain; parent && parent->next != crt; parent = parent->next);
          if (parent->next != crt) {
            ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
            goto ts_reply_free_and_exit;
          }
          parent->next = crt->next; // connect the parent to the tail of the chain.
          crt->next = *chain;       // connect this chain to myself.
          *chain =  crt;           // and wire myself at the top.
        }
        break;
      };
      _TS_DEBUG_PRINTF("Nope # %d did not match", d);
    };
  };


  /* RFC 5652:  Cryptographic Message Syntax (CMS)

     5.4.  Message Digest Calculation Process

     The message digest calculation process computes a message digest on
     either the content being signed or the content together with the
     signed attributes.  In either case, the initial input to the message
     digest calculation process is the "value" of the encapsulated content
     being signed.  Specifically, the initial input is the
     encapContentInfo eContent OCTET STRING to which the signing process
     is applied.  Only the octets comprising the value of the eContent
     OCTET STRING are input to the message digest algorithm, not the tag
     or the length octets.

     The result of the message digest calculation process depends on
     whether the signedAttrs field is present.  When the field is absent,
     the result is just the message digest of the content as described
     above.  When the field is present, however, the result is the message
     digest of the complete DER encoding of the SignedAttrs value
     contained in the signedAttrs field.  Since the SignedAttrs value,
     when present, must contain the content-type and the message-digest
     attributes, those values are indirectly included in the result.  The
     content-type attribute MUST NOT be included in a countersignature
     unsigned attribute as defined in Section 11.4.  A separate encoding
     of the signedAttrs field is performed for message digest calculation.
     The IMPLICIT [0] tag in the signedAttrs is not used for the DER
     encoding, rather an EXPLICIT SET OF tag is used.  That is, the DER
     encoding of the EXPLICIT SET OF tag, rather than of the IMPLICIT [0]
     tag, MUST be included in the message digest calculation along with
     the length and content octets of the SignedAttributes value.

     When the signedAttrs field is absent, only the octets comprising the
     value of the SignedData encapContentInfo eContent OCTET STRING (e.g.,
     the contents of a file) are input to the message digest calculation.
     This has the advantage that the length of the content being signed
     need not be known in advance of the signature generation process.

     Although the encapContentInfo eContent OCTET STRING tag and length
     octets are not included in the message digest calculation, they are
     protected by other means.  The length octets are protected by the
     nature of the message digest algorithm since it is computationally
     infeasible to find any two distinct message contents of any length
     that have the same message digest.

  */
  // Calculate the digest over the signed data area. I.e. the timestamp
  // and the digest of the plaintext.
  //
  mbedtls_md_init(&md);

  // XXX set minimal level ? > SHA1 ?
  md_info = mbedtls_md_info_from_type( signed_data_digest_type );
  assert(md_info);

  signed_data_digest_len = mbedtls_md_get_size(md_info);
  assert(signed_data_digest_len);
  mbedtls_md_setup(&md, md_info, 0);
  bzero(signed_data_digest, sizeof(signed_data_digest));
  assert(sizeof(signed_data_digest) == MBEDTLS_MD_MAX_SIZE);

  assert(signed_data);
  assert(signed_data_len);

  CHK(mbedtls_md_starts(&md));
  CHK(mbedtls_md_update(&md, signed_data, signed_data_len));
  CHK(mbedtls_md_finish(&md, signed_data_digest));

  if (reply->signer_info.signed_attribute_raw && reply->signer_info.sig_digest) {
    _TS_DEBUG_PRINTF("Indirected signing.");

    if (reply->signer_info.sig_digest_len != signed_data_digest_len) {
      _TS_DEBUG_PRINTF("Digest in signedAttrs has the wrong length %d != %d", reply->signer_info.sig_digest_len, signed_data_digest_len);
      ret = MBEDTLS_ERR_PK_SIG_LEN_MISMATCH;
      goto ts_reply_free_and_exit;
    };

    // XXX set minimal level ? > SHA1 ?
    if (((reply->signer_info.sig_digest_type != signed_data_digest_type)) || (signed_data_digest_type == MBEDTLS_MD_NONE)) {
      _TS_DEBUG_PRINTF("Digest in signedAttrs types do no match up");
      ret = MBEDTLS_ERR_X509_INVALID_SIGNATURE;
      goto ts_reply_free_and_exit;
    };

    // verify that the indirect signing is about the digest of the area that contains
    // the digest and plaintext of the plaintext
    //
    if (bcmp(signed_data_digest, reply->signer_info.sig_digest, reply->signer_info.sig_digest_len)) {
      _TS_DEBUG_PRINTF("Digest in signedAttrs lengths do not match up");
      ret = MBEDTLS_ERR_PK_SIG_LEN_MISMATCH;
      goto ts_reply_free_and_exit;
    };

    // And now verify that the hash of the area is in the signed section of
    // which the signature is checked.

    assert(reply->signer_info.sig_digest_type);

    mbedtls_md_context_t md;
    mbedtls_md_init(&md);
    md_info = mbedtls_md_info_from_type(reply->signer_info.sig_digest_type);
    mbedtls_md_setup(&md, md_info, 0);

    assert(md_info);
    assert(reply->signer_info.signed_attribute_raw);
    assert(reply->signer_info.signed_attribute_len);

    // We need to 'repair' the signed block before taking the hash. See above paragraph !
    unsigned char first_byte = MBEDTLS_ASN1_SET | MBEDTLS_ASN1_CONSTRUCTED;
    CHK(mbedtls_md_starts(&md));
    CHK(mbedtls_md_update(&md, &first_byte, 1));
    CHK(mbedtls_md_update(&md, reply->signer_info.signed_attribute_raw + 1, reply->signer_info.signed_attribute_len - 1));
    CHK(mbedtls_md_finish(&md, signed_data_digest));
  };

  assert(&(crt->pk));

  assert(signed_data_digest_type);
  assert(signed_data_digest_len);

  assert(reply->signer_info.sig);
  assert(reply->signer_info.sig_len);

  ret = mbedtls_pk_verify(&(crt->pk),
                          signed_data_digest_type, signed_data_digest, signed_data_digest_len,
                          reply->signer_info.sig, reply->signer_info.sig_len);
  switch (ret) {
    case 0:
      _TS_DEBUG_PRINTF("Signature on the Reply OK");
      break;
    case MBEDTLS_ERR_RSA_VERIFY_FAILED: {
        char buff[128]; mbedtls_strerror(ret, buff, sizeof(buff));
        _TS_DEBUG_PRINTF("Signature FAIL %x: %s", -ret, buff);
        goto ts_reply_free_and_exit;
      };
      break;
    case MBEDTLS_ERR_PK_SIG_LEN_MISMATCH: {
        char buff[128]; mbedtls_strerror(ret, buff, sizeof(buff));
        _TS_DEBUG_PRINTF("Signature config fail %x: %s", -ret, buff);
        goto ts_reply_free_and_exit;
      };
      break;
    default:
      printf("Other error %x %x", ret, -ret);
      goto ts_reply_free_and_exit;
      break;
  };

  // So all that is left to check now is if the cert used above - is indeed sufficiently trusted.
  return (0);

ts_reply_len_fault_and_exit:
  ret = MBEDTLS_ERR_TS_REPLY_INVALID_FORMAT + MBEDTLS_ERR_ASN1_LENGTH_MISMATCH;

ts_reply_free_and_exit:
  mbedtls_ts_reply_free( reply );
  return ret;
}
