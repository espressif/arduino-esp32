#ifndef _UPDATE_PROCESSOR_LEGACY
#define _UPDATE_PROCESSOR_LEGACY
#include "UpdateProcessor.h"


#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF

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
#endif
