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
#ifdef UPDATE_SIGN
#include "Updater_Signing.h"
#endif /* UPDATE_SIGN */

#define UPDATE_ERROR_OK           (0)   ///< Update completed without error
#define UPDATE_ERROR_WRITE        (1)   ///< Write operation failed
#define UPDATE_ERROR_ERASE        (2)   ///< Erase operation failed
#define UPDATE_ERROR_READ         (3)   ///< Read operation failed
#define UPDATE_ERROR_SPACE        (4)   ///< Not enough space for update
#define UPDATE_ERROR_SIZE         (5)   ///< Provided size is invalid
#define UPDATE_ERROR_STREAM       (6)   ///< Stream read timeout or error
#define UPDATE_ERROR_MD5          (7)   ///< MD5 checksum mismatch
#define UPDATE_ERROR_MAGIC_BYTE   (8)   ///< Magic byte/header mismatch
#define UPDATE_ERROR_ACTIVATE     (9)   ///< Activation failed
#define UPDATE_ERROR_NO_PARTITION (10)  ///< No suitable partition found
#define UPDATE_ERROR_BAD_ARGUMENT (11)  ///< Bad argument provided to API
#define UPDATE_ERROR_ABORT        (12)  ///< Update was aborted
#define UPDATE_ERROR_DECRYPT      (13)  ///< Decryption failed
#define UPDATE_ERROR_SIGN         (14)  ///< Signature verification failed

#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF  ///< Constant indicating update size is unknown

#define U_FLASH    0    ///< Update target: Flash (OTA)
#define U_FLASHFS  100  ///< Update target: Flash filesystem (legacy)
#define U_SPIFFS   101  ///< Update target: SPIFFS filesystem
#define U_FATFS    102  ///< Update target: FAT filesystem
#define U_LITTLEFS 103  ///< Update target: LittleFS filesystem
#define U_AUTH     200  ///< Update target: Authentication/secure update

#define ENCRYPTED_BLOCK_SIZE       16  ///< Encrypted block size in bytes
#define ENCRYPTED_TWEAK_BLOCK_SIZE 32  ///< Tweak block size used in encrypted images
#define ENCRYPTED_KEY_SIZE         32  ///< Encryption key size in bytes

#define U_AES_DECRYPT_NONE         0  ///< No AES decryption: image is not encrypted
#define U_AES_DECRYPT_AUTO         1  ///< Auto-detect decryption needs
#define U_AES_DECRYPT_ON           2  ///< Force AES decryption on image
#define U_AES_DECRYPT_MODE_MASK    3  ///< Mask for decryption mode bits
#define U_AES_IMAGE_DECRYPTING_BIT 4  ///< Bit flag indicating image is being decrypted

#define SPI_SECTORS_PER_BLOCK 16  // usually large erase block is 32k/64k ///< Number of SPI sectors per erase block (platform dependent)
#define SPI_FLASH_BLOCK_SIZE  (SPI_SECTORS_PER_BLOCK * SPI_FLASH_SEC_SIZE)  ///< Calculated SPI flash block size in bytes

class UpdateClass {
public:
  typedef std::function<void(size_t, size_t)> THandlerFunction_Progress;

  /**
   * @brief Construct a new UpdateClass object
   */
  UpdateClass();

  /**
   * @brief Register a progress callback
   *
   * The callback will be called with two arguments: number of bytes
   * processed and total number of bytes expected.
   *
   * @param fn Callback function to receive progress updates
   * @return Reference to this UpdateClass for chaining
   */
  UpdateClass &onProgress(THandlerFunction_Progress fn);

  /**
   * @brief Prepare the updater and reserve space
   *
   * @param size Size of incoming update in bytes or UPDATE_SIZE_UNKNOWN
   * @param command Target command/partition type (e.g., `U_FLASH`)
   * @param ledPin Optional LED pin to toggle during update
   * @param ledOn LED active state (HIGH/LOW)
   * @param label Optional partition label for selection
   * @return true if preparation succeeded and there is enough space
   * @return false on failure
   */
  bool begin(size_t size = UPDATE_SIZE_UNKNOWN, int command = U_FLASH, int ledPin = -1, uint8_t ledOn = LOW, const char *label = NULL);

#ifndef UPDATE_NOCRYPT
  /**
   * @brief Configure decryption parameters for encrypted images
   *
   * @param cryptKey Pointer to 32-byte AES key (or NULL to leave unchanged)
   * @param cryptAddress Address/offset used for tweak calculation
   * @param cryptConfig Lower nibble configuration bits for decryption
   * @param cryptMode Decryption mode (`U_AES_DECRYPT_AUTO`, `U_AES_DECRYPT_ON`, etc.)
   * @return true on success
   * @return false on failure
   */
  bool setupCrypt(const uint8_t *cryptKey = 0, size_t cryptAddress = 0, uint8_t cryptConfig = 0xf, int cryptMode = U_AES_DECRYPT_AUTO);
#endif /* UPDATE_NOCRYPT */

  /**
   * @brief Write a buffer to the update target
   *
   * Writes `len` bytes from `data` into the current update partition and
   * advances the internal write pointer.
   *
   * @param data Pointer to data buffer
   * @param len Number of bytes to write
   * @return Number of bytes actually written
   */
  size_t write(uint8_t *data, size_t len);

  /**
   * @brief Write remaining bytes from a `Stream` to the update target
   *
   * Uses `readBytes()` and will set `UPDATE_ERROR_STREAM` on timeout.
   * Suitable for slow sources such as `Serial`.
   *
   * @param data Stream to read from
   * @return Number of bytes written
   */
  size_t writeStream(Stream &data);

  /**
   * @brief Finalize the update
   *
   * If all bytes have been written, this writes update configuration
   * (e.g., eboot) and returns true. On error or if incomplete (and
   * `evenIfRemaining` is false) it aborts and returns false.
   *
   * @param evenIfRemaining If true, accept incomplete updates and finalize
   * @return true on successful finalization
   * @return false on failure
   */
  bool end(bool evenIfRemaining = false);

#ifndef UPDATE_NOCRYPT
  /**
   * @brief Set AES256 decryption key
   *
   * @param cryptKey Pointer to 32-byte key
   * @return true on success
   */
  bool setCryptKey(const uint8_t *cryptKey);

  /**
   * @brief Set crypt mode used for image decryption
   *
   * @param cryptMode One of `U_AES_DECRYPT_*` values
   * @return true on success
   */
  bool setCryptMode(const int cryptMode);

  /**
   * @brief Set decryption address/offset
   *
   * The address is used when deriving tweaks for per-block decryption.
   * Only the aligned part of the address is used (low bits cleared).
   *
   * @param cryptAddress Address/offset value
   */
  void setCryptAddress(const size_t cryptAddress) {
    _cryptAddress = cryptAddress & 0x00fffff0;
  }

  /**
   * @brief Set decryption configuration bits
   *
   * @param cryptConfig Configuration nibble used by the decryptor
   */
  void setCryptConfig(const uint8_t cryptConfig) {
    _cryptCfg = cryptConfig & 0x0f;
  }
#endif /* UPDATE_NOCRYPT */

  /**
   * @brief Abort the running update and clear state
   */
  void abort();

  /**
   * @brief Print the last error code and message to a `Print` stream
   *
   * @param out Output stream (e.g., `Serial`)
   */
  void printError(Print &out);

  /**
   * @brief Return a human-readable string for the last error
   *
   * @return const char* Pointer to a static error description
   */
  const char *errorString();

  /**
   * @brief Set expected MD5 checksum for the incoming firmware image
   *
   * @param expected_md5 Hex string containing expected MD5 digest
   * @param calc_post_decryption If true, calculate MD5 after decryption
   * @return true if MD5 was accepted
   */
  bool setMD5(
    const char *expected_md5
#ifndef UPDATE_NOCRYPT
    ,
    bool calc_post_decryption = true
#endif /* #ifdef UPDATE_NOCRYPT */
  );

  /**
   * @brief Get MD5 digest string of the successfully completed firmware
   *
   * @return String Hex representation of MD5 digest
   */
  String md5String(void) {
    return _md5.toString();
  }

  /**
   * @brief Retrieve the raw MD5 bytes of the completed firmware
   *
   * @param result Pointer to a 16-byte buffer to receive MD5 bytes
   */
  void md5(uint8_t *result) {
    return _md5.getBytes(result);
  }

#ifdef UPDATE_SIGN
  /**
   * @brief Install signature verification for update images
   *
   * Call before `begin()` to enable signature verification. The verifier
   * determines hash type and signature format.
   *
   * @param sign Pointer to a verifier instance
   * @return true if verifier was installed successfully
   */
  bool installSignature(UpdaterVerifyClass *sign);
#endif /* UPDATE_SIGN */

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

#ifdef UPDATE_SIGN
  SHA2Builder *_hash;
  UpdaterVerifyClass *_sign;
  uint8_t *_signatureBuffer;
  size_t _signatureSize;
  int _hashType;
#endif /* UPDATE_SIGN */
};

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_UPDATE)
extern UpdateClass Update;
#endif

#endif
