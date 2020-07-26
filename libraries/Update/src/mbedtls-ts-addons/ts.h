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


#include "x509_ts_utils.h"
#include "sign.h"

#ifndef _H_TS
#define _H_TS
/*
  TimeStampResp ::= SEQUENCE  {
      status                  PKIStatusInfo,
          PKIStatusInfo ::= SEQUENCE {
            status        PKIStatus,
              KIStatus ::= INTEGER {
                granted                (0), -- when the PKIStatus contains the value zero a TimeStampToken, as requested, is present.
                grantedWithMods        (1), -- when the PKIStatus contains the value one a TimeStampToken, with modifications, is present.
                rejection              (2),
                waiting                (3),
                revocationWarning      (4), -- this message contains a warning that a revocation is imminent
                revocationNotification (5)  -- notification that a revocation has occurred
              } // end of PKIStatusInfo
            statusString  PKIFreeText     OPTIONAL,
              PKIFreeText ::= SEQUENCE SIZE (1..MAX) OF UTF8String
            failInfo      PKIFailureInfo  OPTIONAL
              PKIFailureInfo ::= BIT STRING {
                badAlg               (0),-- unrecognized or unsupported Algorithm Identifier
                badRequest           (2),-- transaction not permitted or supported
                badDataFormat        (5),-- the data submitted has the wrong format
                timeNotAvailable    (14),-- the TSA's time source is not available
                unacceptedPolicy    (15),-- the requested TSA policy is not supported by the TSA.
                unacceptedExtension (16),-- the requested extension is not supported by the TSA.
                addInfoNotAvailable (17),-- the additional information requested could not be understood or is not available
                systemFailure       (25)-- the request cannot be handled due to system failure
              } // end of   PKIFailureInfo
            } // end of PKIStatusInfo
      timeStampToken          TimeStampToken     OPTIONAL
        TimeStampToken ::= ContentInfo
           ContentInfo ::= SEQUENCE {
             -- contentType is id-signedData as defined in [CMS]
             contentType  id-signedData OBJECT IDENTIFIER ::= { iso(1) member-body(2) us(840) rsadsi(113549) pkcs(1) pkcs7(7) 2 },
             -- content is SignedData as defined in([CMS])
             content [0] EXPLICIT ANY DEFINED BY contentType
                SignedData ::= SEQUENCE {
                   version CMSVersion,
                   digestAlgorithms DigestAlgorithmIdentifiers,
                         DigestAlgorithmIdentifiers ::= SET OF DigestAlgorithmIdentifier
                             DigestAlgorithmIdentifier ::= AlgorithmIdentifier
                   encapContentInfo EncapsulatedContentInfo,
                      EncapsulatedContentInfo ::= SEQUENCE {
                          -- eContentType within SignedData is id-ct-TSTInfo
                          eContentType id-ct-TSTInfo  OBJECT IDENTIFIER ::= { iso(1) member-body(2) us(840) rsadsi(113549) pkcs(1) pkcs-9(9) smime(16) ct(1) 4},
                          -- eContent within SignedData is TSTInfo
                          eContent [0] EXPLICIT OCTET STRING OPTIONAL
                          TSTInfo ::= SEQUENCE  {
                              version                      INTEGER  { v1(1) },
                              policy                       TSAPolicyId,
                                  TSAPolicyId ::= OBJECT IDENTIFIER
                              messageImprint               MessageImprint, -- MUST have the same value as the similar field in TimeStampReq
                                MessageImprint ::= SEQUENCE  {
                                   hashAlgorithm                AlgorithmIdentifier,
                                      AlgorithmIdentifier  ::=  SEQUENCE  {
                                          algorithm               OBJECT IDENTIFIER,
                                          parameters              ANY DEFINED BY algorithm OPTIONAL
                                      } // end of algo identifier
                                   hashedMessage                OCTET STRING
                              } // end of MessageImprint
                              serialNumber                 INTEGER,  -- Time-Stamping users MUST be ready to accommodate integers up to 160 bits.
                              genTime                      GeneralizedTime, -- YYMMDDHHMM[SS[.D*]Z[stuff]
                              accuracy                     Accuracy                 OPTIONAL,
                                Accuracy ::= SEQUENCE {
                                  seconds        INTEGER           OPTIONAL,
                                  millis     [0] INTEGER  (1..999) OPTIONAL,
                                  micros     [1] INTEGER  (1..999) OPTIONAL
                              } // end of Accuracy
                              ordering                     BOOLEAN             DEFAULT FALSE,
                              nonce                        INTEGER                  OPTIONAL,   -- MUST be present if the similar field was present in TimeStampReq.  In that case it MUST have the same value.
                              tsa                          [0] GeneralName          OPTIONAL,
                                 GeneralName ::= CHOICE {
                                     otherName                       [0]     OtherName,
                                     rfc822Name                      [1]     IA5String,
                                     dNSName                         [2]     IA5String,
                                     x400Address                     [3]     ORAddress,
                                     directoryName                   [4]     Name,
                                     ediPartyName                    [5]     EDIPartyName,
                                     uniformResourceIdentifier       [6]     IA5String,
                                     iPAddress                       [7]     OCTET STRING,
                                     registeredID                    [8]     OBJECT IDENTIFIER
                                  } // end of GeneralName
                              extensions                   [1] IMPLICIT Extensions  OPTIONAL
                                 Extensions  ::=  SEQUENCE SIZE (1..MAX) OF Extension
                                    Extension  ::=  SEQUENCE  {
                                        extnID      OBJECT IDENTIFIER,
                                        critical    BOOLEAN DEFAULT FALSE,
                                        extnValue   OCTET STRING
                                    } // Extension
                                // Extensions
                          } // end of eContent GeneralName
                   } // end of encapContentInfo
                   certificates [0] IMPLICIT CertificateSet OPTIONAL,
                   crls [1] IMPLICIT RevocationInfoChoices OPTIONAL,
                   signerInfos SignerInfos
                     SignerInfos ::= SET OF SignerInfo
                       SignerInfo ::= SEQUENCE {
                          version CMSVersion,
                                CMSVersion ::= INTEGER
                          sid SignerIdentifier,
                              SignerIdentifier ::= CHOICE {
                                 issuerAndSerialNumber IssuerAndSerialNumber,
                                 subjectKeyIdentifier [0] SubjectKeyIdentifier
                              }
                          digestAlgorithm DigestAlgorithmIdentifier,
                          signedAttrs [0] IMPLICIT SignedAttributes OPTIONAL,
                                SignedAttributes ::= SET SIZE (1..MAX) OF Attribute
                                   Attribute ::= SEQUENCE {
                                      attrType OBJECT IDENTIFIER,
                                      attrValues SET OF AttributeValue
                                            AttributeValue ::= ANY
                                   }
                          signatureAlgorithm SignatureAlgorithmIdentifier,
                          signature SignatureValue,
                                SignatureValue ::= OCTET STRING
                          unsignedAttrs [1] IMPLICIT UnsignedAttributes OPTIONAL
                                UnsignedAttributes ::= SET SIZE (1..MAX) OF Attribute
                                   Attribute ::= SEQUENCE {
                                      attrType OBJECT IDENTIFIER,
                                      attrValues SET OF AttributeValue
                                            AttributeValue ::= ANY
                                   }
                       } // SignerInfo
                     // SET of SignerInfo / SignerInfos
                   } // end of content (a SignedData)
           } // end of ContentInfo

      } // end of TimeStampResp



  TSTInfo ::= SEQUENCE  {
    version                      INTEGER  { v1(1) },
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

  Accuracy ::= SEQUENCE {
                seconds        INTEGER           OPTIONAL,
                millis     [0] INTEGER  (1..999) OPTIONAL,
                micros     [1] INTEGER  (1..999) OPTIONAL  }
*/
#define MBEDTLS_ERR_TS_CORRUPTION_DETECTED  -10000
#define MBEDTLS_ERR_TS_BAD_INPUT_DATA       -10001
#define MBEDTLS_ERR_TS_REPLY_INVALID_FORMAT -10002

# define TS_STATUS_GRANTED                       0
# define TS_STATUS_GRANTED_WITH_MODS             1
# define TS_STATUS_REJECTION                     2
# define TS_STATUS_WAITING                       3
# define TS_STATUS_REVOCATION_WARNING            4
# define TS_STATUS_REVOCATION_NOTIFICATION       5

# define TS_INFO_BAD_ALG                         0
# define TS_INFO_BAD_REQUEST                     2
# define TS_INFO_BAD_DATA_FORMAT                 5
# define TS_INFO_TIME_NOT_AVAILABLE             14
# define TS_INFO_UNACCEPTED_POLICY              15
# define TS_INFO_UNACCEPTED_EXTENSION           16
# define TS_INFO_ADD_INFO_NOT_AVAILABLE         17
# define TS_INFO_SYSTEM_FAILURE                 25

// Should be moved to oid.h
#define MBEDTLS_OID_PKCS7                          MBEDTLS_OID_PKCS "\x07" /**< pkcs-7 OBJECT IDENTIFIER ::= { iso(1) member-body(2) us(840) rsadsi(113549) pkcs(1) 7 } */
#define MBEDTLS_OID_PKCS7_ID_SIGNED                MBEDTLS_OID_PKCS7 "\x02" /*< id-signedData OBJECT IDENTIFIER ::= { iso(1) member-body(2) us(840) rsadsi(113549) pkcs(1) pkcs7(7) 2 } */
#define MBEDTLS_OID_STINFO                         MBEDTLS_OID_PKCS9 "\x10" "\x01" "\x04" /* id-ct-TSTInfo  OBJECT IDENTIFIER ::= { iso(1) member-body(2)   us(840) rsadsi(113549) pkcs(1) pkcs-9(9) smime(16) ct(1) 4} */

#define MBEDTLS_OID_PKCS9_MESSAGE_DIGEST           MBEDTLS_OID_PKCS9 "\x04" /**< messageDigest AttributeType ::= { pkcs-9 4 } */

#define MBEDTLS_OID_PKCS9_SMIME                    MBEDTLS_OID_PKCS9 "\x10" //{iso(1) member-body(2) us(840) rsadsi(113549) pkcs(1) pkcs-9(9) smime(16) 
#define MBEDTLS_OID_PKCS9_SMIME_AA_SIGNING_CERT    MBEDTLS_OID_PKCS9_SMIME "\x02\x0C" //  smime(16) id-aa(2) signing-certificate(12)}
#define MBEDTLS_OID_PKCS9_SMIME_AA_SIGNING_CERT_V2 MBEDTLS_OID_PKCS9_SMIME"\x02\x2F"  //  smime(16) id-aa(2) signing-certificateV2(47)}

typedef mbedtls_asn1_buf mbedtls_ts_buf;

// ASN1: PKIStatusInfo
//
typedef struct mbedtls_asn1_ts_pki_status_info {
  int                                   pki_status;
  char                                * statusString;
  int                                   failInfo;
} mbedtls_asn1_ts_pki_status_info;

// ASN1: TSTInfo (the RFC3161 timestamp specific eContent)
//
typedef struct mbedtls_asn1_ts_pki_ts_info {
  mbedtls_md_type_t                     digest_type;

  unsigned char *                       payload_digest;
  size_t                                payload_digest_len;

  mbedtls_x509_time                     signed_time;

} mbedtls_asn1_ts_pki_ts_info;

// ASN1: TimeStampResp
typedef struct mbedtls_ts_reply
{
  mbedtls_asn1_ts_pki_status_info      status;
  // mbedtls_asn1_ts_pki_time_stamp_token timestamptoken;
  mbedtls_asn1_ts_pki_ts_info          ts_info;
  mbedtls_asn1_ts_pki_signer_info      signer_info;
  mbedtls_x509_crt                    * chain;
} mbedtls_ts_reply;

int mbedtls_asn1_get_ts_pkistatusinfo(unsigned char **p, unsigned char * end, mbedtls_asn1_ts_pki_status_info * status);
int mbedtls_ts_get_tsinfo(unsigned char **p, unsigned char * end, mbedtls_asn1_ts_pki_ts_info * ts_info);
int mbedtls_x509_ts_reply_parse_der(const unsigned char **buf, size_t * buflen, mbedtls_ts_reply * reply);

int mbedtls_ts_pki_status_info_free(mbedtls_asn1_ts_pki_status_info * status);
int mbedtls_ts_reply_free(struct mbedtls_ts_reply *reply);

int mbedtls_ts_reply_free(struct mbedtls_ts_reply *reply);
#endif
