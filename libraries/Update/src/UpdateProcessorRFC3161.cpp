
#include "Arduino.h"
#include "esp_spi_flash.h"
#include "esp_ota_ops.h"
#include "esp_image_format.h"
#include "esp_partition.h"
#include "esp_spi_flash.h"

#include "UpdateProcessorRFC3161.h"

/* We have three formats;

  1) original 'raw' package.

   Starts with 0xE7 / ESP_IMAGE_HEADER_MAGIC.

   Which has no checksum or other protection (beyond the size check and the check of the first byte).

  2) The experimental format; starting with 'RedWaxEU\r\n' followed by a raw ECDSAignature; against
   a list of hardcoded public keys.

  3) A more interoperable that allows for both a timestamp and a signature; see
   https://tools.ietf.org/id/draft-moran-suit-architecture-02.html for why both
   are needed (Downgrades).

   It starts with RedWax/A.BB; where A=1 and B can be any digits.

   It may be followed by key-value pairs. This header line is ended by a '\n'
   and can not be longer than 128 bytes (\n included).

   For version 1.XX it is immediately followed by a DER encoded
   RFC3161 timestamp/signature 'reply'. And this is followed by
   the original 'raw' package - i.e as per above '1'.

   This is validated both for SPIFFS(firmware) as wel as for FLASH (code).

  BUT: (and this caveat applies to all methods) -- a corrupted or nefarious
   upload will be detected; but will only in the case of a OTA FLASH update
   lead to that new code not getting activated. For a SPIFFS update that is
   not an option (as you are overwriting the 'real' one). So in this case
   the calling code may want to wipe the SPIFFs filesystem.
*/

#define REDWAX_MAGIC_HEADER "RedWax/1."

UpdateProcessorRFC3161::UpdateProcessorRFC3161(UpdateProcessor  * nxt)
  : _state(INIT)
  , _rfc3161(NULL)
  , _rfc3161_at(0)
  , _rfc3161_len(0)
  , _progress_flash(0)
  , _next(nxt)
  , _md_info(NULL)
  , _md_ctx(NULL)
  , _payload_len(0)
  , _trustChain(NULL)
  , _legacyAllowed(false)
{
  // we need to hook this in - as the legacy processor does the actual
  // burning of the firmware.
  //
  if (_next == NULL)
    _next = new UpdateProcessorLegacy();
};

static void _freeChain(mbedtls_x509_crt *c) {
  while (c) {
    mbedtls_x509_crt * p = c;
    c = c->next;
    free(p);
  };
}

UpdateProcessorRFC3161:: ~UpdateProcessorRFC3161() {
  reset();
  delete _next;
  _freeChain(_trustChain);
};

int UpdateProcessorRFC3161::addTrustedCertAsDER(const unsigned char * der, size_t len) {
  int ret;
  mbedtls_x509_crt * crt;

  if ((crt = (mbedtls_x509_crt*)malloc(sizeof(mbedtls_x509_crt))) == NULL)
    return MBEDTLS_ERR_X509_ALLOC_FAILED;

  mbedtls_x509_crt_init(crt);

  if ((ret = mbedtls_x509_crt_parse(crt, der, len)) != 0) {
    free(crt);
    return ret;
  }
  crt->next = _trustChain;
  _trustChain = crt;
  return 0;
}

int UpdateProcessorRFC3161::setTrustedCerts(mbedtls_x509_crt * trustChain) {
  _freeChain(_trustChain);
  _trustChain = trustChain;
  return 0;
}

int UpdateProcessorRFC3161::setAllowLegacyUploads(bool legacyAllowed) {
  _legacyAllowed = legacyAllowed;
  return 0;
}

void UpdateProcessorRFC3161::reset() {
  log_d("RESET");
  if (_rfc3161)
    free(_rfc3161);

  if (_md_ctx) {
    mbedtls_md_free(_md_ctx);
    free(_md_ctx);
    _md_ctx = NULL;
  }

  _rfc3161 = NULL;
  _rfc3161_len = 0;
  _rfc3161_at = 0;

  _payload_len = 0;
  _state = INIT;
};

UpdateProcessor::secure_update_processor_err_t UpdateProcessorRFC3161::process_header(uint32_t *command, uint8_t * buffer, size_t *len) {
  unsigned char * p;
  uint32_t results = 0;
  size_t l;

  switch (_state) {
    case INIT:
      if (*len < 128) {
        log_d("Not enough bytes yet");
        return secure_update_processor_AGAIN;
      };

      if ((buffer[0] != REDWAX_MAGIC_HEADER[0])  || (bcmp(REDWAX_MAGIC_HEADER, buffer, sizeof(REDWAX_MAGIC_HEADER) - 1))) {
        if (_legacyAllowed && _next)
          return _next->process_header(command, buffer, len);
        return secure_update_processor_ERROR;
      };

      p = (unsigned char *)memchr(buffer, '\n', *len);

      if (p == NULL) {
        log_e("No EOL fo%s: %d\n\n", REDWAX_MAGIC_HEADER, *len);
        return secure_update_processor_ERROR;
      };

      if (1) {
        *p = '\0';
        char *q = index((char *)buffer, ' ');
        if (q == NULL) q = (char *)buffer;
        log_d("Ignoring params: %s", q);
      };

      // what now follows is the DER encoded rfc3161 blob.
      // 30 (sequence) 82 (elems) 09 07 length
      p++;
      if (p[0] != 0x30 || p[1] != 0x82) {
        log_e("No TSR ? %x %x\n\n", p[0], p[1]);
        return secure_update_processor_ERROR;
      };
      _rfc3161_len = (p[2] << 8) + p[3] + 4 /* header */;

      assert(_rfc3161 == NULL);
      if (_rfc3161_len > 16 * 1024 || (_rfc3161 = (unsigned char *)malloc(_rfc3161_len)) == NULL) {
        log_e("Size problems %x %x ->  %d\n\n", p[2], p[3], _rfc3161_len);
        return secure_update_processor_ERROR;
      };

      // Skip over our header.
      *len -= (p - buffer);
      bcopy(p, buffer, *len);
      _rfc3161_at = 0;

      _state = RFC;
      log_d("RFC 3161 signed payload");
      return secure_update_processor_AGAIN;
      break;

    case RFC:
      // are we still working on the pre-amble; or actually
      // aready reading the real thing ?
      //
      l = *len;
      if (l > _rfc3161_len - _rfc3161_at)
        l =  _rfc3161_len - _rfc3161_at;

      bcopy( buffer, _rfc3161 + _rfc3161_at, l);
      _rfc3161_at += l;
      *len -= l;

      // move anything that was unprocessed in the buffer (if any)
      //
      if (_rfc3161_at >= _rfc3161_len && *len) {
        bcopy( buffer + l, buffer, *len);
      };

      if (_rfc3161_at != _rfc3161_len)
        return secure_update_processor_AGAIN;

      // Validate the RFC 3161 signature package. And when valid, use
      // the signed digest (typically a SHA256) to validate the file
      // that follows.
      //
      log_d("Processing RFC 3161 signed payload");

      bzero(&_reply, sizeof(_reply));

      if ((mbedtls_x509_ts_reply_parse_der((const unsigned char **)&_rfc3161, &_rfc3161_len, &_reply)) != 0) {
        // _abort(UPDATE_ERROR_CHECKSUM);
        log_e("RFC 3161 signature invalid.");

        return secure_update_processor_ERROR;
      };

      if (_reply.ts_info.digest_type < MBEDTLS_MD_SHA256) {
        Serial.printf("Digest %s not accepatble", mbedtls_md_get_name(mbedtls_md_info_from_type(_reply.ts_info.digest_type)));
        return secure_update_processor_ERROR;
      }
      log_d("Processed RFC 3161 signed payload");

      if ((_md_info = mbedtls_md_info_from_type(_reply.ts_info.digest_type)) == NULL ||
          (_md_ctx = (mbedtls_md_context_t*)malloc(sizeof(*_md_ctx))) == NULL
         ) {
        log_e("Invalid/memory issue digest.");
        return secure_update_processor_ERROR;
      };
      log_i("Processing plaintext with %s specified RFC3161 digest.", mbedtls_md_get_name(_md_info));

      mbedtls_md_init(_md_ctx);
      if (
        (mbedtls_md_setup(_md_ctx, _md_info, 0) != 0) ||
        (mbedtls_md_starts(_md_ctx) != 0)
      ) {
        log_e("Setup  issue digest.");
        return secure_update_processor_ERROR;
      };

      log_d("RFC3161 signature verified.");

      if (!_trustChain) {
        log_e("No chain to validate RFC3161 signature. Aborting.");
        return secure_update_processor_ERROR;
      };

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
      {
        log_d("Signatures in the trust chain:");
        for (mbedtls_x509_crt * c = _trustChain; c; c = c->next) {
          char buf[1024 * 2];
          mbedtls_x509_crt_info(buf, sizeof(buf), " - ", c);
          log_d("  %s", buf);
        };

        log_d("Signatures in the RFC3161 wrapper:");
        for (mbedtls_x509_crt * c = _reply.chain; c; c = c->next) {
          char buf[1024 * 2];
          mbedtls_x509_crt_info(buf, sizeof(buf), " - ", c);
          log_d("  %s", buf);
        };
      }
#endif
      if (mbedtls_x509_crt_verify(_reply.chain, _trustChain, NULL /* no CRL */, NULL /* any name fine */, &results, NULL, NULL) != 0) {
        log_e("RFC3161 signature could not be validated against the chain. Aborting.");
        return secure_update_processor_ERROR;
      }
      log_i("RFC3161 signature on timestamp and payload digest verified.");

      _state = POST;
      return secure_update_processor_AGAIN;
      break;

    case POST:
      // we're just a wrapper/prefix around the old format; so at
      // this point - hand off.
      //
      log_d("RFC3161: processing payload.");
      return _next->process_header(command, buffer, len);
      break;
    default:
      return secure_update_processor_ERROR;
      break;
  };
  // _abort(UPDATE_ERROR_MAGIC_BYTE);
  return secure_update_processor_ERROR;
};

UpdateProcessor::secure_update_processor_err_t UpdateProcessorRFC3161::process_payload(uint8_t * buffer, size_t *len) {
  // calculate the digest using the digest type specified in the RFC 3161 TS Info section.
  //
  // assuming we unwisely allow for this.
  if (_rfc3161 == NULL) {
    if (_legacyAllowed && _next)
      return _next->process_payload(buffer, len);
    log_e("RFC3161: cannot process payload - no signature.");
    return secure_update_processor_ERROR;
  };

  if (mbedtls_md_update(_md_ctx, buffer, *len)) {
    log_e("Failed to update digest.");
    return secure_update_processor_ERROR;
  }
  _payload_len += *len;

#if 0
  static size_t _progress_flash = 0;
  Serial.printf("---\n");
  for (int j = 0; j * 16 < *len; j++) {
    Serial.printf("\n%08x  ", _progress_flash + j * 16);
    for (int i = 0; i < 16; i++) {
      if (j * 16 < *len)
        Serial.printf("%02x ", buffer[j * 16 +  i ]);
      else
        Serial.printf("-- ");
      if (i == 7)
        Serial.printf(" ");
    };
    Serial.printf(" |");
    for (int i = 0; i < 16; i++) {
      char c = buffer[ j * 16 + i ];
      Serial.printf("%c", c >= 32 && c < 127 ? c : '.' );
    };
    Serial.printf("|");
    Serial.flush();
    _progress_flash += *len;
  }
  Serial.println("\n");
#endif

  return UpdateProcessor::secure_update_processor_OK;
};


UpdateProcessor::secure_update_processor_err_t UpdateProcessorRFC3161::process_end() {
  unsigned char buff[MBEDTLS_MD_MAX_SIZE];
  log_d("RFC3161 Finalizing payload digest");

  // assuming it is not mandatory ?!
  if (_rfc3161 == NULL && _legacyAllowed) {
    if (_legacyAllowed && _next)
      return _next->process_end();
    log_e("RFC3161 - cannot finalize, no signature.");
    return secure_update_processor_ERROR;
  }

  // Verify the digest with the one from the RFC 3161 TS Info section.
  if (mbedtls_md_finish(_md_ctx, buff)) {
    log_e("Failed to finalize digest.");
    return secure_update_processor_ERROR;
  }

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
  String out = "";
  for (int i = 0; i < mbedtls_md_get_size(_md_info); i++)
    out += String(buff[i], HEX);
  log_d("Payload calculated %s Digest %d:%s", mbedtls_md_get_name(_md_info), mbedtls_md_get_size(_md_info), out.c_str());

  String out2 = "";
  for (int i = 0; i < mbedtls_md_get_size(_md_info); i++)
    out2 += String(_reply.ts_info.payload_digest[i], HEX);
  log_d("RFC3161 Receveived %s Digest %d:%s", mbedtls_md_get_name(_md_info), mbedtls_md_get_size(_md_info), out2.c_str());
#endif

  int res = memcmp(_reply.ts_info.payload_digest, buff, mbedtls_md_get_size(_md_info));

  mbedtls_md_free(_md_ctx);
  free(_md_ctx);
  _md_ctx = NULL;

  // compare this with the digest we got from the signed TSInfo
  if (res) {
    log_e("RFC3161 Payload digest does NOT match signed digest.");
    return UpdateProcessor::secure_update_processor_ERROR;
  }

  log_i(" RFC3161 Payload digest matches signed digest.");
  return UpdateProcessor::secure_update_processor_OK;
};
