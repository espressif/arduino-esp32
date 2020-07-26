#ifndef ESP32_UPDATER_H
#define ESP32_UPDATER_H

#include <Arduino.h>
#include <functional>
#include "esp_partition.h"

#include "UpdateProcessor.h"

#define UPDATE_ERROR_OK                 (0)
#define UPDATE_ERROR_WRITE              (1)
#define UPDATE_ERROR_ERASE              (2)
#define UPDATE_ERROR_READ               (3)
#define UPDATE_ERROR_SPACE              (4)
#define UPDATE_ERROR_SIZE               (5)
#define UPDATE_ERROR_STREAM             (6)
#define UPDATE_ERROR_CHECKSUM           (7)
#define UPDATE_ERROR_MAGIC_BYTE         (8)
#define UPDATE_ERROR_ACTIVATE           (9)
#define UPDATE_ERROR_NO_PARTITION       (10)
#define UPDATE_ERROR_BAD_ARGUMENT       (11)
#define UPDATE_ERROR_ABORT              (12)

/* to keep the API backward compatible */
#define UPDATE_ERROR_MD5 UPDATE_ERROR_CHECKSUM

class UpdateClass {
  public:
    typedef std::function<void(size_t, size_t)> THandlerFunction_Progress;
    typedef std::function<bool(void)> THandlerFunction_ApproveOTA;

    UpdateClass(UpdateProcessor * processor = NULL);
    ~UpdateClass();
    /*
      This callback will be called when Update is receiving data
    */
    UpdateClass& onProgress(THandlerFunction_Progress fn);
    UpdateClass& approveReboot(THandlerFunction_ApproveOTA fn);

    /* Specify an optional processor - that will check, validate or
     * otherwise process the data. If none is specified; the default
     * (straight write through) will be used.
     */
    void setProcessor(UpdateProcessor * processor);

    /*
      Call this to check the space needed for the update
      Will return false if there is not enough space
    */
    bool begin(size_t size = UPDATE_SIZE_UNKNOWN, int command = U_FLASH, int ledPin = -1, uint8_t ledOn = LOW);

    /*
      Writes a buffer to the flash and increments the address
      Returns the amount written
    */
    size_t write(uint8_t *data, size_t len);

    /*
      Writes the remaining bytes from the Stream to the flash
      Uses readBytes() and sets UPDATE_ERROR_STREAM on timeout
      Returns the bytes written
      Should be equal to the remaining bytes when called
      Usable for slow streams like Serial
    */
    size_t writeStream(Stream & data);

    /*
      Template to write from objects that expose
      available() and read(uint8_t*, size_t) methods
      faster than the writeStream method
      writes only what is available
    */
    template<typename T>
    size_t write(T & data);

    /*
      If all bytes are written, any checksums match; then this call will return true

      If there is already an update running but is not finished and !evenIfRemaining
      or there is an error

      this will clear everything and return false
      the last error is available through getError()

      evenIfRemaining is helpfull when you update without knowing the final size first
    */
    bool end(bool evenIfRemaining = false);

    /*       this call will write the config to eboot
          and return true
    */
    bool activate();
    /*
      Aborts the running update
    */
    void abort();

    /*
      Prints the last error to an output stream
    */
    void printError(Stream & out);

    const char * errorString();

    /*
      Helpers
    */
    uint8_t getError() {
      return _error;
    }
    void clearError() {
      _error = UPDATE_ERROR_OK;
    }
    bool hasError() {
      return _error != UPDATE_ERROR_OK;
    }
    bool isRunning() {
      return _size > 0;
    }
    bool isFinished() {
      return _progress == _size;
    }
    size_t size() {
      return _size;
    }
    size_t progress() {
      return _progress;
    }
    size_t remaining() {
      return _size - _progress;
    }


    /*
      check if there is a firmware on the other OTA partition that you can bootinto
    */
    bool canRollBack();
    /*
      set the other OTA partition as bootable (reboot to enable)
    */
    bool rollBack();

  private:
    UpdateProcessor *_processor;
    void _reset();
    void _abort(uint8_t err);
    bool _writeBuffer();
    bool _verifyHeader(uint8_t * buffer, size_t len);

    uint8_t _error;
    uint8_t *_buffer;
    size_t _bufferLen;
    size_t _size;
    THandlerFunction_Progress _progress_callback;

    uint32_t _progress, _progress_flash;
    uint32_t _command;
    const esp_partition_t* _partition;
    bool _post_header;
    String _md;

    int _ledPin;
    uint8_t _ledOn;
};

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_UPDATE)
extern UpdateClass Update;
#endif

#endif
