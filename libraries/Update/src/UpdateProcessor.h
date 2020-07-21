#ifndef _UPDATE_PROCESSOR
#define _UPDATE_PROCESSOR

#include <Arduino.h>
#include <functional>
#include <mbedtls/md.h>

#include "mbedtls-ts-addons/ts.h"

#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF

// Potential commands
#ifndef U_FLASH
#define U_FLASH   (0)
#endif

#ifndef U_SPIFFS
#define U_SPIFFS  (100)
#endif

#ifndef U_AUTH
#define U_AUTH    (200)
#endif

class UpdateProcessor {
  public:
    typedef enum secure_update_processor_err_t {
      secure_update_processor_OK = 0,
      secure_update_processor_AGAIN, // cannot (yet) act; not enough data passed
      secure_update_processor_DECLINED,
      secure_update_processor_ERROR,
    } secure_update_processor_err_t;
    virtual ~UpdateProcessor() {};
    virtual void reset();
    virtual secure_update_processor_err_t process_header(uint32_t *command, uint8_t * buffer, size_t *len);
    virtual secure_update_processor_err_t process_payload(uint8_t * buff, size_t *len);
    virtual secure_update_processor_err_t process_end();
};

class UpdateProcessorLegacy : public UpdateProcessor {
  public:
    void reset() {};
    secure_update_processor_err_t process_header(uint32_t *command, uint8_t * buffer, size_t *len);
    secure_update_processor_err_t process_payload(uint8_t * buff, size_t *len) {
      return secure_update_processor_OK;
    }
    secure_update_processor_err_t process_end() {
      return secure_update_processor_OK;
    };
};

class UpdateProcessorWithChecksum : public UpdateProcessor {
  public:
    UpdateProcessorWithChecksum() : _md_info(NULL), _md_ctx(NULL) {};
    ~UpdateProcessorWithChecksum();
    void reset();
    secure_update_processor_err_t setChecksum(const char * crc, mbedtls_md_type_t md_type = MBEDTLS_MD_NONE);
    secure_update_processor_err_t process_header(uint32_t *command, uint8_t * buffer, size_t *len) { 
	return secure_update_processor_OK; 
	};
    secure_update_processor_err_t process_payload(uint8_t * buff, size_t *len);
    secure_update_processor_err_t process_end();
  private:
    const mbedtls_md_info_t *_md_info;
    mbedtls_md_context_t * _md_ctx;
    unsigned char _md[MBEDTLS_MD_MAX_SIZE];
};
#endif
