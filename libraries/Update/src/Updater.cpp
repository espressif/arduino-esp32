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
#include "mbedtls/aes.h"

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
  } else if (_error == UPDATE_ERROR_DECRYPT) {
    return ("Decryption error");
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
  : _error(0), _cryptKey(0), _cryptBuffer(0), _buffer(0), _skipBuffer(0), _bufferLen(0), _size(0), _progress_callback(NULL), _progress(0), _paroffset(0), _command(U_FLASH), _partition(NULL), _cryptMode(U_AES_DECRYPT_AUTO), _cryptAddress(0), _cryptCfg(0xf) {
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

  _cryptBuffer = nullptr;
  _buffer = nullptr;
  _skipBuffer = nullptr;
  _bufferLen = 0;
  _progress = 0;
  _size = 0;
  _command = U_FLASH;

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

bool UpdateClass::begin(size_t size, int command, int ledPin, uint8_t ledOn, const char *label) {
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

  if (size == 0) {
    _error = UPDATE_ERROR_SIZE;
    return false;
  }

  if (command == U_FLASH) {
    _partition = esp_ota_get_next_update_partition(NULL);
    if (!_partition) {
      _error = UPDATE_ERROR_NO_PARTITION;
      return false;
    }
    log_d("OTA Partition: %s", _partition->label);
  } else if (command == U_SPIFFS) {
    _partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, label);
    _paroffset = 0;
    if (!_partition) {
      _partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, NULL);
      _paroffset = 0x1000;  //Offset for ffat, assuming size is already corrected
      if (!_partition) {
        _error = UPDATE_ERROR_NO_PARTITION;
        return false;
      }
    }
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

void UpdateClass::_abort(uint8_t err) {
  _reset();
  _error = err;
}

void UpdateClass::abort() {
  _abort(UPDATE_ERROR_ABORT);
}

void UpdateClass::_cryptKeyTweak(size_t cryptAddress, uint8_t *tweaked_key) {
  memcpy(tweaked_key, _cryptKey, ENCRYPTED_KEY_SIZE);
  if (_cryptCfg == 0) return;  //no tweaking needed, use crypt key as-is

  const uint8_t pattern[] = { 23, 23, 23, 14, 23, 23, 23, 12, 23, 23, 23, 10, 23, 23, 23, 8 };
  int pattern_idx = 0;
  int key_idx = 0;
  int bit_len = 0;
  uint32_t tweak = 0;
  cryptAddress &= 0x00ffffe0;  //bit 23-5
  cryptAddress <<= 8;          //bit23 shifted to bit31(MSB)
  while (pattern_idx < sizeof(pattern)) {
    tweak = cryptAddress << (23 - pattern[pattern_idx]);  //bit shift for small patterns
    // alternative to: tweak = rotl32(tweak,8 - bit_len);
    tweak = (tweak << (8 - bit_len)) | (tweak >> (24 + bit_len));  //rotate to line up with end of previous tweak bits
    bit_len += pattern[pattern_idx++] - 4;                         //add number of bits in next pattern(23-4 = 19bits = 23bit to 5bit)
    while (bit_len > 7) {
      tweaked_key[key_idx++] ^= tweak;  //XOR byte
      // alternative to: tweak = rotl32(tweak, 8);
      tweak = (tweak << 8) | (tweak >> 24);  //compiler should optimize to use rotate(fast)
      bit_len -= 8;
    }
    tweaked_key[key_idx] ^= tweak;  //XOR remaining bits, will XOR zeros if no remaining bits
  }
  if (_cryptCfg == 0xf) return;  //return with fully tweaked key

  //some of tweaked key bits need to be restore back to crypt key bits
  const uint8_t cfg_bits[] = { 67, 65, 63, 61 };
  key_idx = 0;
  pattern_idx = 0;
  while (key_idx < ENCRYPTED_KEY_SIZE) {
    bit_len += cfg_bits[pattern_idx];
    if ((_cryptCfg & (1 << pattern_idx)) == 0) {  //restore crypt key bits
      while (bit_len > 0) {
        if (bit_len > 7 || ((_cryptCfg & (2 << pattern_idx)) == 0)) {  //restore a crypt key byte
          tweaked_key[key_idx] = _cryptKey[key_idx];
        } else {  //MSBits restore crypt key bits, LSBits keep as tweaked bits
          tweaked_key[key_idx] &= (0xff >> bit_len);
          tweaked_key[key_idx] |= (_cryptKey[key_idx] & (~(0xff >> bit_len)));
        }
        key_idx++;
        bit_len -= 8;
      }
    } else {  //keep tweaked key bits
      while (bit_len > 0) {
        if (bit_len < 8 && ((_cryptCfg & (2 << pattern_idx)) == 0)) {  //MSBits keep as tweaked bits, LSBits restore crypt key bits
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
  uint8_t tweaked_key[ENCRYPTED_KEY_SIZE];  //tweaked crypt key
  int done = 0;

  /*
        Mbedtls functions will be replaced with esp_aes functions when hardware acceleration is available

        To Do:
        Replace mbedtls for the cases where there's no hardware acceleration
     */

  mbedtls_aes_context ctx;  //initialize AES
  mbedtls_aes_init(&ctx);
  while ((_bufferLen - done) >= ENCRYPTED_BLOCK_SIZE) {
    for (int i = 0; i < ENCRYPTED_BLOCK_SIZE; i++) _cryptBuffer[(ENCRYPTED_BLOCK_SIZE - 1) - i] = _buffer[i + done];  //reverse order 16 bytes to decrypt
    if (((_cryptAddress + _progress + done) % ENCRYPTED_TWEAK_BLOCK_SIZE) == 0 || done == 0) {
      _cryptKeyTweak(_cryptAddress + _progress + done, tweaked_key);  //update tweaked crypt key
      if (mbedtls_aes_setkey_enc(&ctx, tweaked_key, 256)) {
        return false;
      }
      if (mbedtls_aes_setkey_dec(&ctx, tweaked_key, 256)) {
        return false;
      }
    }
    if (mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT, _cryptBuffer, _cryptBuffer)) {  //use MBEDTLS_AES_ENCRYPT to decrypt flash code
      return false;
    }
    for (int i = 0; i < ENCRYPTED_BLOCK_SIZE; i++) _buffer[i + done] = _cryptBuffer[(ENCRYPTED_BLOCK_SIZE - 1) - i];  //reverse order 16 bytes from decrypt
    done += ENCRYPTED_BLOCK_SIZE;
  }
  return true;
}

bool UpdateClass::_writeBuffer() {
  //first bytes of loading image, check to see if loading image needs decrypting
  if (!_progress) {
    _cryptMode &= U_AES_DECRYPT_MODE_MASK;
    if ((_cryptMode == U_AES_DECRYPT_ON)
        || ((_command == U_FLASH) && (_cryptMode & U_AES_DECRYPT_AUTO) && (_buffer[0] != ESP_IMAGE_HEADER_MAGIC))) {
      _cryptMode |= U_AES_IMAGE_DECRYPTING_BIT;  //set to decrypt the loading image
      log_d("Decrypting OTA Image");
    }
  }
  //check if data in buffer needs decrypting
  if (_cryptMode & U_AES_IMAGE_DECRYPTING_BIT) {
    if (!_decryptBuffer()) {
      _abort(UPDATE_ERROR_DECRYPT);
      return false;
    }
  }
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
  bool block_erase = (_size - _progress >= SPI_FLASH_BLOCK_SIZE) && (offset % SPI_FLASH_BLOCK_SIZE == 0);                                                   // if it's the block boundary, than erase the whole block from here
  bool part_head_sectors = _partition->address % SPI_FLASH_BLOCK_SIZE && offset < (_partition->address / SPI_FLASH_BLOCK_SIZE + 1) * SPI_FLASH_BLOCK_SIZE;  // sector belong to unaligned partition heading block
  bool part_tail_sectors = offset >= (_partition->address + _size) / SPI_FLASH_BLOCK_SIZE * SPI_FLASH_BLOCK_SIZE;                                           // sector belong to unaligned partition tailing block
  if (block_erase || part_head_sectors || part_tail_sectors) {
    if (!ESP.partitionEraseRange(_partition, _progress, block_erase ? SPI_FLASH_BLOCK_SIZE : SPI_FLASH_SEC_SIZE)) {
      _abort(UPDATE_ERROR_ERASE);
      return false;
    }
  }

  // try to skip empty blocks on unecrypted partitions
  if ((_partition->encrypted || _chkDataInBlock(_buffer + skip / sizeof(uint32_t), _bufferLen - skip)) && !ESP.partitionWrite(_partition, _progress + skip, (uint32_t *)_buffer + skip / sizeof(uint32_t), _bufferLen - skip)) {
    _abort(UPDATE_ERROR_WRITE);
    return false;
  }

  //restore magic or md5 will fail
  if (!_progress && _command == U_FLASH) {
    _buffer[0] = ESP_IMAGE_HEADER_MAGIC;
  }
  _md5.add(_buffer, _bufferLen);
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
  } else if (_command == U_SPIFFS) {
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
  } else if (_command == U_SPIFFS) {
    _reset();
    return true;
  }
  return false;
}

bool UpdateClass::setMD5(const char *expected_md5) {
  if (strlen(expected_md5) != 32) {
    return false;
  }
  _target_md5 = expected_md5;
  _target_md5.toLowerCase();
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

  if (hasError() || !isRunning())
    return 0;

  if (!_verifyHeader(data.peek())) {
    _reset();
    return 0;
  }

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
    if ((_bufferLen == remaining() || _bufferLen == SPI_FLASH_SEC_SIZE) && !_writeBuffer())
      return written;
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
  if (!len || len % sizeof(uint32_t))
    return true;

  size_t dwl = len / sizeof(uint32_t);

  do {
    if (*(uint32_t *)data ^ 0xffffffff)  // for SPI NOR flash empty blocks are all one's, i.e. filled with 0xff byte
      return true;

    data += sizeof(uint32_t);
  } while (--dwl);
  return false;
}

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_UPDATE)
UpdateClass Update;
#endif
