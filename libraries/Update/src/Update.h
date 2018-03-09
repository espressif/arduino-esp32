#ifndef ESP8266UPDATER_H
#define ESP8266UPDATER_H

#include <Arduino.h>
#include <MD5Builder.h>
#include <functional>
#include "esp_partition.h"

#define UPDATE_ERROR_OK                 (0)
#define UPDATE_ERROR_WRITE              (1)
#define UPDATE_ERROR_ERASE              (2)
#define UPDATE_ERROR_READ               (3)
#define UPDATE_ERROR_SPACE              (4)
#define UPDATE_ERROR_SIZE               (5)
#define UPDATE_ERROR_STREAM             (6)
#define UPDATE_ERROR_MD5                (7)
#define UPDATE_ERROR_MAGIC_BYTE         (8)
#define UPDATE_ERROR_ACTIVATE           (9)
#define UPDATE_ERROR_NO_PARTITION       (10)
#define UPDATE_ERROR_BAD_ARGUMENT       (11)
#define UPDATE_ERROR_ABORT              (12)

#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF

#define U_FLASH   0
#define U_SPIFFS  100
#define U_AUTH    200

class UpdateClass {
  public:
    typedef std::function<void(size_t, size_t)> THandlerFunction_Progress;

    UpdateClass();

    /*
      This callback will be called when Update is receiving data
    */
    UpdateClass& onProgress(THandlerFunction_Progress fn);

    /*
      Call this to check the space needed for the update
      Will return false if there is not enough space
    */
    bool begin(size_t size=UPDATE_SIZE_UNKNOWN, int command = U_FLASH);

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
    size_t writeStream(Stream &data);

    /*
      If all bytes are written
      this call will write the config to eboot
      and return true
      If there is already an update running but is not finished and !evenIfRemainanig
      or there is an error
      this will clear everything and return false
      the last error is available through getError()
      evenIfRemaining is helpfull when you update without knowing the final size first
    */
    bool end(bool evenIfRemaining = false);

    /*
      Aborts the running update
    */
    void abort();

    /*
      Prints the last error to an output stream
    */
    void printError(Stream &out);

    /*
      sets the expected MD5 for the firmware (hexString)
    */
    bool setMD5(const char * expected_md5);

    /*
      returns the MD5 String of the sucessfully ended firmware
    */
    String md5String(void){ return _md5.toString(); }

    /*
      populated the result with the md5 bytes of the sucessfully ended firmware
    */
    void md5(uint8_t * result){ return _md5.getBytes(result); }

    //Helpers
    uint8_t getError(){ return _error; }
    void clearError(){ _error = UPDATE_ERROR_OK; }
    bool hasError(){ return _error != UPDATE_ERROR_OK; }
    bool isRunning(){ return _size > 0; }
    bool isFinished(){ return _progress == _size; }
    size_t size(){ return _size; }
    size_t progress(){ return _progress; }
    size_t remaining(){ return _size - _progress; }

    /*
      Template to write from objects that expose
      available() and read(uint8_t*, size_t) methods
      faster than the writeStream method
      writes only what is available
    */
    template<typename T>
    size_t write(T &data){
      size_t written = 0;
      if (hasError() || !isRunning())
        return 0;

      size_t available = data.available();
      while(available) {
        if(_bufferLen + available > remaining()){
          available = remaining() - _bufferLen;
        }
        if(_bufferLen + available > 4096) {
          size_t toBuff = 4096 - _bufferLen;
          data.read(_buffer + _bufferLen, toBuff);
          _bufferLen += toBuff;
          if(!_writeBuffer())
            return written;
          written += toBuff;
        } else {
          data.read(_buffer + _bufferLen, available);
          _bufferLen += available;
          written += available;
          if(_bufferLen == remaining()) {
            if(!_writeBuffer()) {
              return written;
            }
          }
        }
        if(remaining() == 0)
          return written;
        available = data.available();
      }
      return written;
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
    void _reset();
    void _abort(uint8_t err);
    bool _writeBuffer();
    bool _verifyHeader(uint8_t data);
    bool _verifyEnd();


    uint8_t _error;
    uint8_t *_buffer;
    size_t _bufferLen;
    size_t _size;
    THandlerFunction_Progress _progress_callback;
    uint32_t _progress;
    uint32_t _command;
    const esp_partition_t* _partition;

    String _target_md5;
    MD5Builder _md5;
};

extern UpdateClass Update;

#endif
