/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Update.h"
#include "Arduino.h"
#include "spi_flash_mmap.h"
#include "esp_ota_ops.h"
#include "esp_image_format.h"
#ifndef UPDATE_NOCRYPT
#include "mbedtls/aes.h"
#endif /* UPDATE_NOCRYPT */

static const char *_err2str(uint8_t _error) {
  if (_error == UPDATE_ERROR_OK) {
    return ("No Error");
  } else if (_error == UPDATE_ERROR_WRITE) {
    return ("Flash Write Failed");
  } else if (_error == UPDATE_ERROR_ERASE) {
    return ("Flash Erase Failed");
  } else if (_error == UPDATE_ERROR_READ) {
    return ("Flash Read Failed");
  } else if (_error == UPDATE_ERROR_SPACE) {
    return ("Not Enough Space");
  } else if (_error == UPDATE_ERROR_SIZE) {
    return ("Bad Size Given");
  } else if (_error == UPDATE_ERROR_STREAM) {
    return ("Stream Read Timeout");
  } else if (_error == UPDATE_ERROR_MD5) {
    return ("MD5 Check Failed");
  } else if (_error == UPDATE_ERROR_MAGIC_BYTE) {
    return ("Wrong Magic Byte");
  } else if (_error == UPDATE_ERROR_ACTIVATE) {
    return ("Could Not Activate The Firmware");
  } else if (_error == UPDATE_ERROR_NO_PARTITION) {
    return ("Partition Could Not be Found");
  } else if (_error == UPDATE_ERROR_BAD_ARGUMENT) {
    return ("Bad Argument");
  } else if (_error == UPDATE_ERROR_ABORT) {
    return ("Aborted");
#ifndef UPDATE_NOCRYPT
  } else if (_error == UPDATE_ERROR_DECRYPT) {
    return ("Decryption error");
#endif /* UPDATE_NOCRYPT */
#ifdef UPDATE_SIGN
  } else if (_error == UPDATE_ERROR_SIGN) {
    return ("Signature Verification Failed");
#endif /* UPDATE_SIGN */
  }
  return ("UNKNOWN");
}

static bool _partitionIsBootable(const esp_partition_t *partition) {
  uint8_t buf[ENCRYPTED_BLOCK_SIZE];
  if (!partition) {
    return false;
  }
  if (!ESP.partitionRead(partition, 0, (uint32_t *)buf, ENCRYPTED_BLOCK_SIZE)) {
    return false;
  }

  if (buf[0] != ESP_IMAGE_HEADER_MAGIC) {
    return false;
  }
  return true;
}

bool UpdateClass::_enablePartition(const esp_partition_t *partition) {
  if (!partition) {
    return false;
  }
  return ESP.partitionWrite(partition, 0, (uint32_t *)_skipBuffer, ENCRYPTED_BLOCK_SIZE);
}

UpdateClass::UpdateClass()
  : _error(0),
#ifndef UPDATE_NOCRYPT
    _cryptKey(0), _cryptBuffer(0),
#endif /* UPDATE_NOCRYPT */
    _buffer(0), _skipBuffer(0), _bufferLen(0), _size(0), _progress_callback(NULL), _progress(0), _command(U_FLASH), _partition(NULL)
#ifndef UPDATE_NOCRYPT
    ,
    _cryptMode(U_AES_DECRYPT_AUTO), _cryptAddress(0), _cryptCfg(0xf)
#endif /* UPDATE_NOCRYPT */
#ifdef UPDATE_SIGN
    ,
    _hash(NULL), _sign(NULL), _signatureBuffer(NULL), _signatureSize(0), _hashType(-1)
#endif /* UPDATE_SIGN */
{
}

UpdateClass &UpdateClass::onProgress(THandlerFunction_Progress fn) {
  _progress_callback = fn;
  return *this;
}

void UpdateClass::_reset() {
  if (_buffer) {
    delete[] _buffer;
  }
  if (_skipBuffer) {
    delete[] _skipBuffer;
  }
#ifdef UPDATE_SIGN
  if (_signatureBuffer) {
    delete[] _signatureBuffer;
    _signatureBuffer = nullptr;
  }
  if (_hash && _hashType >= 0) {
    // Clean up internally-created hash object
    delete _hash;
    _hash = nullptr;
  }
#endif /* UPDATE_SIGN */

#ifndef UPDATE_NOCRYPT
  _cryptBuffer = nullptr;
#endif /* UPDATE_NOCRYPT */
  _buffer = nullptr;
  _skipBuffer = nullptr;
  _bufferLen = 0;
  _progress = 0;
  _size = 0;
  _command = U_FLASH;
#ifdef UPDATE_SIGN
  _signatureSize = 0;
#endif /* UPDATE_SIGN */

  if (_ledPin != -1) {
    digitalWrite(_ledPin, !_ledOn);  // off
  }
}

bool UpdateClass::canRollBack() {
  if (_buffer) {  //Update is running
    return false;
  }
  const esp_partition_t *partition = esp_ota_get_next_update_partition(NULL);
  return _partitionIsBootable(partition);
}

bool UpdateClass::rollBack() {
  if (_buffer) {  //Update is running
    return false;
  }
  const esp_partition_t *partition = esp_ota_get_next_update_partition(NULL);
  return _partitionIsBootable(partition) && !esp_ota_set_boot_partition(partition);
}

#ifdef UPDATE_SIGN
bool UpdateClass::installSignature(UpdaterVerifyClass *sign) {
  if (_size > 0) {
    log_w("Update already running");
    return false;
  }
  if (!sign) {
    log_e("Invalid verifier");
    return false;
  }

  int hashType = sign->getHashType();
  if (hashType != HASH_SHA256 && hashType != HASH_SHA384 && hashType != HASH_SHA512) {
    log_e("Invalid hash type: %d", hashType);
    return false;
  }

  _sign = sign;
  _hashType = hashType;
  _signatureSize = 512;  // Fixed signature size (padded to 512 bytes)

  [[maybe_unused]]
  const char *hashName = (hashType == HASH_SHA256)   ? "SHA-256"
                         : (hashType == HASH_SHA384) ? "SHA-384"
                                                     : "SHA-512";
  log_i("Signature verification installed (hash: %s, signature size: %u bytes)", hashName, _signatureSize);
  return true;
}
#endif /* UPDATE_SIGN */

bool UpdateClass::begin(size_t size, int command, int ledPin, uint8_t ledOn, const char *label) {
  (void)label;

  if (_size > 0) {
    log_w("already running");
    return false;
  }

  _ledPin = ledPin;
  _ledOn = !!ledOn;  // 0(LOW) or 1(HIGH)

  _reset();
  _error = 0;
  _target_md5 = emptyString;
  _md5 = MD5Builder();

#ifdef UPDATE_SIGN
  // Create and initialize signature hash if signature verification is enabled
  if (_sign && _hashType >= 0) {
    // Create the appropriate hash builder based on hashType
    switch (_hashType) {
      case HASH_SHA256: _hash = new SHA256Builder(); break;
      case HASH_SHA384: _hash = new SHA384Builder(); break;
      case HASH_SHA512: _hash = new SHA512Builder(); break;
      default:          log_e("Invalid hash type"); return false;
    }

    if (_hash) {
      _hash->begin();
      log_i("Signature hash initialized");
    } else {
      log_e("Failed to create hash builder");
      return false;
    }
  }
#endif /* UPDATE_SIGN */

  if (size == 0) {
    _error = UPDATE_ERROR_SIZE;
    return false;
  }

#ifdef UPDATE_SIGN
  // Validate size is large enough to contain firmware + signature
  if (_signatureSize > 0 && size < _signatureSize) {
    _error = UPDATE_ERROR_SIZE;
    log_e("Size too small for signature: %u < %u", size, _signatureSize);
    return false;
  }
#endif /* UPDATE_SIGN */

  if (command == U_FLASH) {
    _partition = esp_ota_get_next_update_partition(NULL);
    if (!_partition || _partition == esp_ota_get_running_partition()) {
      _error = UPDATE_ERROR_NO_PARTITION;
      return false;
    }
    log_d("OTA Partition: %s", _partition->label);
  } else if (command == U_SPIFFS) {
    _partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
    if (!_partition) {
      _error = UPDATE_ERROR_NO_PARTITION;
      return false;
    }
    log_d("SPIFFS Partition: %s", _partition->label);
  } else if (command == U_FATFS) {
    _partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, NULL);
    if (!_partition) {
      _error = UPDATE_ERROR_NO_PARTITION;
      return false;
    }
    log_d("FATFS Partition: %s", _partition->label);
  } else if (command == U_LITTLEFS) {
    _partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_LITTLEFS, NULL);
    if (!_partition) {
      _error = UPDATE_ERROR_NO_PARTITION;
      return false;
    }
    log_d("LittleFS Partition: %s", _partition->label);
  } else if (command == U_FLASHFS) {
    _partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
    if (!_partition) {
      _partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, NULL);
    }
    if (!_partition) {
      _partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_LITTLEFS, NULL);
    }
    if (!_partition) {
      _error = UPDATE_ERROR_NO_PARTITION;
      return false;
    }
    log_d("FS Partition: %s", _partition->label);
  } else {
    _error = UPDATE_ERROR_BAD_ARGUMENT;
    log_e("bad command %u", command);
    return false;
  }

  if (size == UPDATE_SIZE_UNKNOWN) {
    size = _partition->size;
  } else if (size > _partition->size) {
    _error = UPDATE_ERROR_SIZE;
    log_e("too large %u > %u", size, _partition->size);
    return false;
  }

  //initialize
  _buffer = new (std::nothrow) uint8_t[SPI_FLASH_SEC_SIZE];
  if (!_buffer) {
    log_e("_buffer allocation failed");
    return false;
  }
  _size = size;
  _command = command;
  _md5.begin();
  return true;
}

#ifndef UPDATE_NOCRYPT
bool UpdateClass::setupCrypt(const uint8_t *cryptKey, size_t cryptAddress, uint8_t cryptConfig, int cryptMode) {
  if (setCryptKey(cryptKey)) {
    if (setCryptMode(cryptMode)) {
      setCryptAddress(cryptAddress);
      setCryptConfig(cryptConfig);
      return true;
    }
  }
  return false;
}

bool UpdateClass::setCryptKey(const uint8_t *cryptKey) {
  if (!cryptKey) {
    if (_cryptKey) {
      delete[] _cryptKey;
      _cryptKey = 0;
      log_d("AES key unset");
    }
    return false;  //key cleared, no key to decrypt with
  }
  //initialize
  if (!_cryptKey) {
    _cryptKey = new (std::nothrow) uint8_t[ENCRYPTED_KEY_SIZE];
  }
  if (!_cryptKey) {
    log_e("new failed");
    return false;
  }
  memcpy(_cryptKey, cryptKey, ENCRYPTED_KEY_SIZE);
  return true;
}

bool UpdateClass::setCryptMode(const int cryptMode) {
  if (cryptMode >= U_AES_DECRYPT_NONE && cryptMode <= U_AES_DECRYPT_ON) {
    _cryptMode = cryptMode;
  } else {
    log_e("bad crypt mode argument %i", cryptMode);
    return false;
  }
  return true;
}
#endif /* UPDATE_NOCRYPT */

void UpdateClass::_abort(uint8_t err) {
  _reset();
  _error = err;
}

void UpdateClass::abort() {
  _abort(UPDATE_ERROR_ABORT);
}

#ifndef UPDATE_NOCRYPT
/*
 * Generates an address-tweaked encryption key for ESP32 flash encryption.
 *
 * Key Tweaking Overview:
 * ----------------------
 * ESP32 flash encryption uses "key tweaking" to derive a unique effective key
 * for each 32-byte region of flash. This prevents attackers from:
 *   - Swapping encrypted blocks between different flash addresses
 *   - Using known-plaintext from one region to attack another
 *
 * The tweak is computed by XORing address bits into the base key according
 * to a specific pattern that matches ESP32's hardware flash encryption.
 *
 * Parameters:
 *   cryptAddress - Flash address used to derive the tweak (aligned to 32 bytes)
 *   tweaked_key  - Output buffer for the 32-byte tweaked key
 *
 * Configuration (_cryptCfg):
 * --------------------------
 * The _cryptCfg value (0x0 to 0xF) controls how much address information
 * is mixed into the key:
 *   - 0x0: No tweaking, use base key as-is (lowest security)
 *   - 0xF: Full tweaking, maximum address mixing (highest security, default)
 *   - Other values: Partial tweaking for compatibility
 *
 * This matches the --flash_crypt_conf parameter in espsecure.py
 */
void UpdateClass::_cryptKeyTweak(size_t cryptAddress, uint8_t *tweaked_key) {
  memcpy(tweaked_key, _cryptKey, ENCRYPTED_KEY_SIZE);
  if (_cryptCfg == 0) {
    return;  // No tweaking needed, use base key as-is
  }

  // Pattern defines which address bits to mix into each key region
  // Values represent bit positions (23, 14, 12, 10, 8) used for tweaking
  const uint8_t pattern[] = {23, 23, 23, 14, 23, 23, 23, 12, 23, 23, 23, 10, 23, 23, 23, 8};
  int pattern_idx = 0;
  int key_idx = 0;
  int bit_len = 0;
  uint32_t tweak = 0;

  // Extract address bits 23-5 (aligned to 32-byte boundary)
  cryptAddress &= 0x00ffffe0;
  cryptAddress <<= 8;  // Shift bit 23 to MSB position for easier manipulation

  // XOR address-derived bits into the key
  while (pattern_idx < sizeof(pattern)) {
    tweak = cryptAddress << (23 - pattern[pattern_idx]);
    // Rotate to align with previous tweak bits
    tweak = (tweak << (8 - bit_len)) | (tweak >> (24 + bit_len));
    bit_len += pattern[pattern_idx++] - 4;

    // XOR full bytes
    while (bit_len > 7) {
      tweaked_key[key_idx++] ^= tweak;
      tweak = (tweak << 8) | (tweak >> 24);  // Rotate left 8 bits
      bit_len -= 8;
    }
    tweaked_key[key_idx] ^= tweak;  // XOR remaining bits
  }

  if (_cryptCfg == 0xf) {
    return;  // Full tweaking complete
  }

  // Partial tweaking: restore some key bits based on _cryptCfg
  // Each bit in _cryptCfg controls whether a key region stays tweaked or reverts
  const uint8_t cfg_bits[] = {67, 65, 63, 61};
  key_idx = 0;
  pattern_idx = 0;
  while (key_idx < ENCRYPTED_KEY_SIZE) {
    bit_len += cfg_bits[pattern_idx];
    if ((_cryptCfg & (1 << pattern_idx)) == 0) {
      // Restore original key bits for this region
      while (bit_len > 0) {
        if (bit_len > 7 || ((_cryptCfg & (2 << pattern_idx)) == 0)) {
          tweaked_key[key_idx] = _cryptKey[key_idx];
        } else {
          // Partial byte: MSBits from original, LSBits stay tweaked
          tweaked_key[key_idx] &= (0xff >> bit_len);
          tweaked_key[key_idx] |= (_cryptKey[key_idx] & (~(0xff >> bit_len)));
        }
        key_idx++;
        bit_len -= 8;
      }
    } else {
      // Keep tweaked bits for this region
      while (bit_len > 0) {
        if (bit_len < 8 && ((_cryptCfg & (2 << pattern_idx)) == 0)) {
          // Partial byte: MSBits stay tweaked, LSBits from original
          tweaked_key[key_idx] &= (~(0xff >> bit_len));
          tweaked_key[key_idx] |= (_cryptKey[key_idx] & (0xff >> bit_len));
        }
        key_idx++;
        bit_len -= 8;
      }
    }
    pattern_idx++;
  }
}

/*
 * Decrypts the OTA update buffer using ESP32 flash encryption compatible algorithm.
 *
 * ESP32 Flash Encryption Scheme:
 * ------------------------------
 * This function implements a decryption algorithm compatible with ESP32's flash
 * encryption, which uses a symmetric (involutory) construction:
 *
 *   Transform(data) = ByteReverse(AES_Encrypt(ByteReverse(data)))
 *
 * This transform is its own inverse: Transform(Transform(data)) = data
 * Therefore, the SAME operation is used for both encryption and decryption.
 *
 * Algorithm steps for each 16-byte block:
 *   1. Reverse the byte order of the block
 *   2. Apply AES-256 encryption (NOT decryption!) with address-tweaked key
 *   3. Reverse the byte order of the result
 *
 * Why AES_ENCRYPT for decryption?
 * -------------------------------
 * The byte reversal combined with the key tweaking creates a mathematical
 * structure where AES encryption serves as its own inverse. This design:
 *   - Matches ESP32 hardware flash encryption controller behavior
 *   - Simplifies bootloader (only needs encrypt logic)
 *   - Allows same code path for encrypt/decrypt operations
 *
 * Key Tweaking:
 * -------------
 * The encryption key is "tweaked" based on the flash address every 32 bytes
 * (ENCRYPTED_TWEAK_BLOCK_SIZE). This provides:
 *   - Different effective keys for different flash regions
 *   - Protection against block-swapping attacks
 *
 * Note: Since we use MBEDTLS_AES_ENCRYPT mode, we must call mbedtls_aes_setkey_enc()
 * to set up the correct round keys. The encryption key schedule is required even
 * though this function performs decryption.
 *
 * Reference: ESP-IDF flash encryption documentation
 * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/flash-encryption.html
 */
bool UpdateClass::_decryptBuffer() {
  if (!_cryptKey) {
    log_w("AES key not set");
    return false;
  }
  if (_bufferLen % ENCRYPTED_BLOCK_SIZE != 0) {
    log_e("buffer size error");
    return false;
  }
  if (!_cryptBuffer) {
    _cryptBuffer = new (std::nothrow) uint8_t[ENCRYPTED_BLOCK_SIZE];
  }
  if (!_cryptBuffer) {
    log_e("new failed");
    return false;
  }

  uint8_t tweaked_key[ENCRYPTED_KEY_SIZE];
  int done = 0;

  mbedtls_aes_context ctx;
  mbedtls_aes_init(&ctx);

  while ((_bufferLen - done) >= ENCRYPTED_BLOCK_SIZE) {
    // Step 1: Reverse byte order of the 16-byte block
    for (int i = 0; i < ENCRYPTED_BLOCK_SIZE; i++) {
      _cryptBuffer[(ENCRYPTED_BLOCK_SIZE - 1) - i] = _buffer[i + done];
    }

    // Update tweaked key every ENCRYPTED_TWEAK_BLOCK_SIZE (32) bytes or at start
    if (((_cryptAddress + _progress + done) % ENCRYPTED_TWEAK_BLOCK_SIZE) == 0 || done == 0) {
      _cryptKeyTweak(_cryptAddress + _progress + done, tweaked_key);
      // Use setkey_enc because we perform AES_ENCRYPT operation below
      if (mbedtls_aes_setkey_enc(&ctx, tweaked_key, 256)) {
        return false;
      }
    }

    // Step 2: Apply AES encryption (this decrypts due to the involutory scheme)
    if (mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT, _cryptBuffer, _cryptBuffer)) {
      return false;
    }

    // Step 3: Reverse byte order back to get the decrypted plaintext
    for (int i = 0; i < ENCRYPTED_BLOCK_SIZE; i++) {
      _buffer[i + done] = _cryptBuffer[(ENCRYPTED_BLOCK_SIZE - 1) - i];
    }

    done += ENCRYPTED_BLOCK_SIZE;
  }

  return true;
}
#endif /* UPDATE_NOCRYPT */

bool UpdateClass::_writeBuffer() {
#ifndef UPDATE_NOCRYPT
  //first bytes of loading image, check to see if loading image needs decrypting
  if (!_progress) {
    _cryptMode &= U_AES_DECRYPT_MODE_MASK;
    if ((_cryptMode == U_AES_DECRYPT_ON) || ((_command == U_FLASH) && (_cryptMode & U_AES_DECRYPT_AUTO) && (_buffer[0] != ESP_IMAGE_HEADER_MAGIC))) {
      _cryptMode |= U_AES_IMAGE_DECRYPTING_BIT;  //set to decrypt the loading image
      log_d("Decrypting OTA Image");
    }
  }

  if (!_target_md5_decrypted) {
    _md5.add(_buffer, _bufferLen);
  }

  //check if data in buffer needs decrypting
  if (_cryptMode & U_AES_IMAGE_DECRYPTING_BIT) {
    if (!_decryptBuffer()) {
      _abort(UPDATE_ERROR_DECRYPT);
      return false;
    }
  }
#endif /* UPDATE_NOCRYPT */
  //first bytes of new firmware
  uint8_t skip = 0;
  if (!_progress && _command == U_FLASH) {
    //check magic
    if (_buffer[0] != ESP_IMAGE_HEADER_MAGIC) {
      _abort(UPDATE_ERROR_MAGIC_BYTE);
      return false;
    }

    //Stash the first 16 bytes of data and set the offset so they are
    //not written at this point so that partially written firmware
    //will not be bootable
    skip = ENCRYPTED_BLOCK_SIZE;
    _skipBuffer = new (std::nothrow) uint8_t[skip];
    if (!_skipBuffer) {
      log_e("_skipBuffer allocation failed");
      return false;
    }
    memcpy(_skipBuffer, _buffer, skip);
  }
  if (!_progress && _progress_callback) {
    _progress_callback(0, _size);
  }
  size_t offset = _partition->address + _progress;
  bool block_erase =
    (_size - _progress >= SPI_FLASH_BLOCK_SIZE) && (offset % SPI_FLASH_BLOCK_SIZE == 0);  // if it's the block boundary, than erase the whole block from here
  bool part_head_sectors =
    _partition->address % SPI_FLASH_BLOCK_SIZE
    && offset < (_partition->address / SPI_FLASH_BLOCK_SIZE + 1) * SPI_FLASH_BLOCK_SIZE;  // sector belong to unaligned partition heading block
  bool part_tail_sectors =
    offset >= (_partition->address + _size) / SPI_FLASH_BLOCK_SIZE * SPI_FLASH_BLOCK_SIZE;  // sector belong to unaligned partition tailing block
  if (block_erase || part_head_sectors || part_tail_sectors) {
    if (!ESP.partitionEraseRange(_partition, _progress, block_erase ? SPI_FLASH_BLOCK_SIZE : SPI_FLASH_SEC_SIZE)) {
      _abort(UPDATE_ERROR_ERASE);
      return false;
    }
  }

  // try to skip empty blocks on unencrypted partitions
  if ((_partition->encrypted || _chkDataInBlock(_buffer + skip / sizeof(uint32_t), _bufferLen - skip))
      && !ESP.partitionWrite(_partition, _progress + skip, (uint32_t *)_buffer + skip / sizeof(uint32_t), _bufferLen - skip)) {
    _abort(UPDATE_ERROR_WRITE);
    return false;
  }

  //restore magic or md5 will fail
  if (!_progress && _command == U_FLASH) {
    _buffer[0] = ESP_IMAGE_HEADER_MAGIC;
  }
#ifndef UPDATE_NOCRYPT
  if (_target_md5_decrypted) {
#endif /* UPDATE_NOCRYPT */
    _md5.add(_buffer, _bufferLen);
#ifndef UPDATE_NOCRYPT
  }
#endif /* UPDATE_NOCRYPT */

#ifdef UPDATE_SIGN
  // Add data to signature hash if signature verification is enabled
  // Only hash firmware bytes, not the signature bytes at the end
  if (_hash && _signatureSize > 0) {
    size_t firmwareSize = _size - _signatureSize;
    if (_progress < firmwareSize) {
      // Calculate how many bytes of this buffer are firmware (not signature)
      size_t bytesToHash = _bufferLen;
      if (_progress + _bufferLen > firmwareSize) {
        bytesToHash = firmwareSize - _progress;
      }
      _hash->add(_buffer, bytesToHash);
    }
  }
#endif /* UPDATE_SIGN */

  _progress += _bufferLen;
  _bufferLen = 0;
  if (_progress_callback) {
    _progress_callback(_progress, _size);
  }
  return true;
}

bool UpdateClass::_verifyHeader(uint8_t data) {
  if (_command == U_FLASH) {
    if (data != ESP_IMAGE_HEADER_MAGIC) {
      _abort(UPDATE_ERROR_MAGIC_BYTE);
      return false;
    }
    return true;
  } else {
    return true;
  }
  return false;
}

bool UpdateClass::_verifyEnd() {
  if (_command == U_FLASH) {
    if (!_enablePartition(_partition) || !_partitionIsBootable(_partition)) {
      _abort(UPDATE_ERROR_READ);
      return false;
    }

    if (esp_ota_set_boot_partition(_partition)) {
      _abort(UPDATE_ERROR_ACTIVATE);
      return false;
    }
    _reset();
    return true;
  } else {
    _reset();
    return true;
  }
  return false;
}

bool UpdateClass::setMD5(
  const char *expected_md5
#ifndef UPDATE_NOCRYPT
  ,
  bool calc_post_decryption
#endif /* UPDATE_NOCRYPT */
) {
  if (strlen(expected_md5) != 32) {
    return false;
  }
  _target_md5 = expected_md5;
  _target_md5.toLowerCase();
#ifndef UPDATE_NOCRYPT
  _target_md5_decrypted = calc_post_decryption;
#endif /* UPDATE_NOCRYPT */
  return true;
}

bool UpdateClass::end(bool evenIfRemaining) {
  if (hasError() || _size == 0) {
    return false;
  }

  if (!isFinished() && !evenIfRemaining) {
    log_e("premature end: res:%u, pos:%u/%u\n", getError(), progress(), _size);
    _abort(UPDATE_ERROR_ABORT);
    return false;
  }

  if (evenIfRemaining) {
    if (_bufferLen > 0) {
      _writeBuffer();
    }
    _size = progress();
  }

  _md5.calculate();
  if (_target_md5.length()) {
    if (_target_md5 != _md5.toString()) {
      _abort(UPDATE_ERROR_MD5);
      return false;
    }
  }

#ifdef UPDATE_SIGN
  // Verify signature if signature verification is enabled
  if (_hash && _sign && _signatureSize > 0) {
    log_i("Verifying signature...");
    _hash->calculate();

    // Allocate buffer for signature (max 512 bytes for RSA-4096)
    const size_t maxSigSize = 512;
    _signatureBuffer = new (std::nothrow) uint8_t[maxSigSize];
    if (!_signatureBuffer) {
      log_e("Failed to allocate signature buffer");
      _abort(UPDATE_ERROR_SIGN);
      return false;
    }

    // Read signature from partition (last 512 bytes of what was written)
    size_t firmwareSize = _size - _signatureSize;
    log_d("Reading signature from offset %u (firmware size: %u, total size: %u)", firmwareSize, firmwareSize, _size);
    if (!ESP.partitionRead(_partition, firmwareSize, (uint32_t *)_signatureBuffer, maxSigSize)) {
      log_e("Failed to read signature from partition");
      _abort(UPDATE_ERROR_SIGN);
      return false;
    }

    // Verify the signature
    if (!_sign->verify(_hash, _signatureBuffer, maxSigSize)) {
      log_e("Signature verification failed");
      _abort(UPDATE_ERROR_SIGN);
      return false;
    }

    log_i("Signature verified successfully");
  }
#endif /* UPDATE_SIGN */

  return _verifyEnd();
}

size_t UpdateClass::write(uint8_t *data, size_t len) {
  if (hasError() || !isRunning()) {
    return 0;
  }

  if (len > remaining()) {
    _abort(UPDATE_ERROR_SPACE);
    return 0;
  }

  size_t left = len;

  while ((_bufferLen + left) > SPI_FLASH_SEC_SIZE) {
    size_t toBuff = SPI_FLASH_SEC_SIZE - _bufferLen;
    memcpy(_buffer + _bufferLen, data + (len - left), toBuff);
    _bufferLen += toBuff;
    if (!_writeBuffer()) {
      return len - left;
    }
    left -= toBuff;
  }
  memcpy(_buffer + _bufferLen, data + (len - left), left);
  _bufferLen += left;
  if (_bufferLen == remaining()) {
    if (!_writeBuffer()) {
      return len - left;
    }
  }
  return len;
}

size_t UpdateClass::writeStream(Stream &data) {
  size_t written = 0;
  size_t toRead = 0;
  int timeout_failures = 0;

  if (hasError() || !isRunning()) {
    return 0;
  }

#ifndef UPDATE_NOCRYPT
  if (_command == U_FLASH && !_cryptMode) {
#endif /* UPDATE_NOCRYPT */
    if (!_verifyHeader(data.peek())) {
      _reset();
      return 0;
    }
#ifndef UPDATE_NOCRYPT
  }
#endif /* UPDATE_NOCRYPT */

  if (_ledPin != -1) {
    pinMode(_ledPin, OUTPUT);
  }

  while (remaining()) {
    if (_ledPin != -1) {
      digitalWrite(_ledPin, _ledOn);  // Switch LED on
    }
    size_t bytesToRead = SPI_FLASH_SEC_SIZE - _bufferLen;
    if (bytesToRead > remaining()) {
      bytesToRead = remaining();
    }

    /*
        Init read&timeout counters and try to read, if read failed, increase counter,
        wait 100ms and try to read again. If counter > 300 (30 sec), give up/abort
        */
    toRead = 0;
    timeout_failures = 0;
    while (!toRead) {
      toRead = data.readBytes(_buffer + _bufferLen, bytesToRead);
      if (toRead == 0) {
        timeout_failures++;
        if (timeout_failures >= 300) {
          _abort(UPDATE_ERROR_STREAM);
          return written;
        }
        delay(100);
      }
    }

    if (_ledPin != -1) {
      digitalWrite(_ledPin, !_ledOn);  // Switch LED off
    }
    _bufferLen += toRead;
    if ((_bufferLen == remaining() || _bufferLen == SPI_FLASH_SEC_SIZE) && !_writeBuffer()) {
      return written;
    }
    written += toRead;

#if CONFIG_FREERTOS_UNICORE
    delay(1);  // Fix solo WDT
#endif
  }
  return written;
}

void UpdateClass::printError(Print &out) {
  out.println(_err2str(_error));
}

const char *UpdateClass::errorString() {
  return _err2str(_error);
}

bool UpdateClass::_chkDataInBlock(const uint8_t *data, size_t len) const {
  // check 32-bit aligned blocks only
  if (!len || len % sizeof(uint32_t)) {
    return true;
  }

  size_t dwl = len / sizeof(uint32_t);

  do {
    if (*(uint32_t *)data ^ 0xffffffff) {  // for SPI NOR flash empty blocks are all one's, i.e. filled with 0xff byte
      return true;
    }

    data += sizeof(uint32_t);
  } while (--dwl);
  return false;
}

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_UPDATE)
UpdateClass Update;
#endif
