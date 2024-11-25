/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ESP32UPDATER_H
#define ESP32UPDATER_H

#include <Arduino.h>
#include <MD5Builder.h>
#include <functional>
#include "esp_partition.h"

#define UPDATE_ERROR_OK           (0)
#define UPDATE_ERROR_WRITE        (1)
#define UPDATE_ERROR_ERASE        (2)
#define UPDATE_ERROR_READ         (3)
#define UPDATE_ERROR_SPACE        (4)
#define UPDATE_ERROR_SIZE         (5)
#define UPDATE_ERROR_STREAM       (6)
#define UPDATE_ERROR_MD5          (7)
#define UPDATE_ERROR_MAGIC_BYTE   (8)
#define UPDATE_ERROR_ACTIVATE     (9)
#define UPDATE_ERROR_NO_PARTITION (10)
#define UPDATE_ERROR_BAD_ARGUMENT (11)
#define UPDATE_ERROR_ABORT        (12)
#define UPDATE_ERROR_DECRYPT      (13)

#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF

#define U_FLASH  0
#define U_SPIFFS 100
#define U_AUTH   200

#define ENCRYPTED_BLOCK_SIZE       16
#define ENCRYPTED_TWEAK_BLOCK_SIZE 32
#define ENCRYPTED_KEY_SIZE         32

#define U_AES_DECRYPT_NONE         0
#define U_AES_DECRYPT_AUTO         1
#define U_AES_DECRYPT_ON           2
#define U_AES_DECRYPT_MODE_MASK    3
#define U_AES_IMAGE_DECRYPTING_BIT 4

#define SPI_SECTORS_PER_BLOCK 16  // usually large erase block is 32k/64k
#define SPI_FLASH_BLOCK_SIZE  (SPI_SECTORS_PER_BLOCK * SPI_FLASH_SEC_SIZE)

class UpdateClass {
public:
  typedef std::function<void(size_t, size_t)> THandlerFunction_Progress;

  UpdateClass();

  /*
      This callback will be called when Update is receiving data
    */
  UpdateClass &onProgress(THandlerFunction_Progress fn);

  /*
      Call this to check the space needed for the update
      Will return false if there is not enough space
    */
  bool begin(size_t size = UPDATE_SIZE_UNKNOWN, int command = U_FLASH, int ledPin = -1, uint8_t ledOn = LOW, const char *label = NULL);

#ifndef UPDATE_NOCRYPT
  /*
     Setup decryption configuration
     Crypt Key is 32bytes(256bits) block of data, use the same key as used to encrypt image file
     Crypt Address, use the same value as used to encrypt image file
     Crypt Config,  use the same value as used to encrypt image file
     Crypt Mode,    used to select if image files should be decrypted or not
    */
  bool setupCrypt(const uint8_t *cryptKey = 0, size_t cryptAddress = 0, uint8_t cryptConfig = 0xf, int cryptMode = U_AES_DECRYPT_AUTO);
#endif /* UPDATE_NOCRYPT */

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
      If there is already an update running but is not finished and !evenIfRemaining
      or there is an error
      this will clear everything and return false
      the last error is available through getError()
      evenIfRemaining is helpful when you update without knowing the final size first
    */
  bool end(bool evenIfRemaining = false);

#ifndef UPDATE_NOCRYPT
  /*
      sets AES256 key(32 bytes) used for decrypting image file
    */
  bool setCryptKey(const uint8_t *cryptKey);

  /*
      sets crypt mode used on image files
    */
  bool setCryptMode(const int cryptMode);

  /*
      sets address used for decrypting image file
    */
  void setCryptAddress(const size_t cryptAddress) {
    _cryptAddress = cryptAddress & 0x00fffff0;
  }

  /*
      sets crypt config used for decrypting image file
    */
  void setCryptConfig(const uint8_t cryptConfig) {
    _cryptCfg = cryptConfig & 0x0f;
  }
#endif /* UPDATE_NOCRYPT */

  /*
      Aborts the running update
    */
  void abort();

  /*
      Prints the last error to an output stream
    */
  void printError(Print &out);

  const char *errorString();

  /*
      sets the expected MD5 for the firmware (hexString)
      If calc_post_decryption is true, the update library will calculate the MD5 after the decryption, if false the calculation occurs before the decryption
    */
  bool setMD5(
    const char *expected_md5
#ifndef UPDATE_NOCRYPT
    ,
    bool calc_post_decryption = true
#endif /* #ifdef UPDATE_NOCRYPT */
  );

  /*
      returns the MD5 String of the successfully ended firmware
    */
  String md5String(void) {
    return _md5.toString();
  }

  /*
      populated the result with the md5 bytes of the successfully ended firmware
    */
  void md5(uint8_t *result) {
    return _md5.getBytes(result);
  }

  //Helpers
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
      Template to write from objects that expose
      available() and read(uint8_t*, size_t) methods
      faster than the writeStream method
      writes only what is available
    */
  template<typename T> size_t write(T &data) {
    size_t written = 0;
    if (hasError() || !isRunning()) {
      return 0;
    }

    size_t available = data.available();
    while (available) {
      if (_bufferLen + available > remaining()) {
        available = remaining() - _bufferLen;
      }
      if (_bufferLen + available > 4096) {
        size_t toBuff = 4096 - _bufferLen;
        data.read(_buffer + _bufferLen, toBuff);
        _bufferLen += toBuff;
        if (!_writeBuffer()) {
          return written;
        }
        written += toBuff;
      } else {
        data.read(_buffer + _bufferLen, available);
        _bufferLen += available;
        written += available;
        if (_bufferLen == remaining()) {
          if (!_writeBuffer()) {
            return written;
          }
        }
      }
      if (remaining() == 0) {
        return written;
      }
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
#ifndef UPDATE_NOCRYPT
  void _cryptKeyTweak(size_t cryptAddress, uint8_t *tweaked_key);
  bool _decryptBuffer();
#endif /* UPDATE_NOCRYPT */
  bool _writeBuffer();
  bool _verifyHeader(uint8_t data);
  bool _verifyEnd();
  bool _enablePartition(const esp_partition_t *partition);
  bool _chkDataInBlock(const uint8_t *data, size_t len) const;  // check if block contains any data or is empty

  uint8_t _error;
#ifndef UPDATE_NOCRYPT
  uint8_t *_cryptKey;
  uint8_t *_cryptBuffer;
#endif /* UPDATE_NOCRYPT */
  uint8_t *_buffer;
  uint8_t *_skipBuffer;
  size_t _bufferLen;
  size_t _size;
  THandlerFunction_Progress _progress_callback;
  uint32_t _progress;
  uint32_t _paroffset;
  uint32_t _command;
  const esp_partition_t *_partition;

  String _target_md5;
#ifndef UPDATE_NOCRYPT
  bool _target_md5_decrypted = true;
#endif /* UPDATE_NOCRYPT */
  MD5Builder _md5;

  int _ledPin;
  uint8_t _ledOn;

#ifndef UPDATE_NOCRYPT
  uint8_t _cryptMode;
  size_t _cryptAddress;
  uint8_t _cryptCfg;
#endif /* UPDATE_NOCRYPT */
};

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_UPDATE)
extern UpdateClass Update;
#endif

#endif
