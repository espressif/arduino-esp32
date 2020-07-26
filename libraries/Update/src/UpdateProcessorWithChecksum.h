#ifndef _UPDATE_PROCESSOR_WITH_CHECKSUM
#define _UPDATE_PROCESSOR_WITH_CHECKSUM
#include "UpdateProcessor.h"
#include "UpdateProcessorLegacy.h"

class UpdateProcessorWithChecksum : public UpdateProcessor {
  public:
    UpdateProcessorWithChecksum(UpdateProcessor * nxt = NULL);
    ~UpdateProcessorWithChecksum();

    void reset();

    secure_update_processor_err_t setChecksum(const char * crc, mbedtls_md_type_t md_type = MBEDTLS_MD_NONE);

    secure_update_processor_err_t process_header(uint32_t *command, uint8_t * buffer, size_t *len) { 
	return secure_update_processor_OK; 
	};

    secure_update_processor_err_t process_payload(uint8_t * buff, size_t *len);
    secure_update_processor_err_t process_end();
  private:
    UpdateProcessor * _next;
    const mbedtls_md_info_t *_md_info;
    mbedtls_md_context_t * _md_ctx;
    unsigned char _md[MBEDTLS_MD_MAX_SIZE];
};
#endif
