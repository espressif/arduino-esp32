#include "Update.h"
#include "Arduino.h"
#include "esp_spi_flash.h"
#include "esp_ota_ops.h"
#include "esp_image_format.h"

static const char * _err2str(uint8_t _error) {
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
  }
  return ("UNKNOWN");
}

static bool _partitionIsBootable(const esp_partition_t* partition) {
  uint8_t buf[4];
  if (!partition) {
    log_e("Invalid partition");
    return false;
  }
  if (!ESP.flashRead(partition->address, (uint32_t*)buf, 4)) {
    log_e("Could not read first 4 bytes of the partition");
    return false;
  }

  if (buf[0] != ESP_IMAGE_HEADER_MAGIC) {
    log_e("Invalid magic (%02x %02x %02x %02x)", buf[0], buf[1], buf[2], buf[3]);
    return false;
  }
  return true;
}

static bool _enablePartition(const esp_partition_t* partition) {
  uint8_t buf[4];
  if (!partition) {
    log_e("Invalid partition");
    return false;
  }
  if (!ESP.flashRead(partition->address, (uint32_t*)buf, 4)) {
    log_e("Could not read first 4 bytes of the partition");
    return false;
  }
  buf[0] = ESP_IMAGE_HEADER_MAGIC;

  if (!ESP.flashWrite(partition->address, (uint32_t*)buf, 4)) {
    log_e("Could not repair the magic (%02x %02x %02x %02x)", buf[0], buf[1], buf[2], buf[3]);
    return false;
  }
  return true;
}



UpdateClass::UpdateClass(UpdateProcessor  * processor)
  : _processor(0)
  , _error(0)
  , _buffer(0)
  , _bufferLen(0)
  , _size(0)
  , _progress_callback(NULL)
  , _progress(0)
  ,  _progress_flash(0)
  , _command(U_FLASH)
  , _partition(NULL)
  , _post_header(false)
{
  // Old legacy behaviour is the default.
  if (_processor == NULL)
    _processor = new UpdateProcessorLegacy();
}

UpdateClass::~UpdateClass() {
  _reset();
  if (_processor)
    delete(_processor);
}

void UpdateClass::setProcessor(UpdateProcessor * processor) {
  _reset();
  if (_processor)
    delete(_processor);
  _processor = processor;
};

UpdateClass& UpdateClass::onProgress(THandlerFunction_Progress fn) {
  _progress_callback = fn;
  return *this;
}

void UpdateClass::_reset() {
  if (_buffer)
    delete[] _buffer;
  _buffer = 0;
  _bufferLen = 0;
  _progress = 0;
  _progress_flash = 0;
  _size = 0;
  _command = U_FLASH;
  _post_header = false;

  if (_processor)
    _processor->reset();

  if (_ledPin != -1) {
    digitalWrite(_ledPin, !_ledOn); // off
  }
}

bool UpdateClass::canRollBack() {
  if (_buffer) { //Update is running
    return false;
  }
  const esp_partition_t* partition = esp_ota_get_next_update_partition(NULL);
  return _partitionIsBootable(partition);
}

bool UpdateClass::rollBack() {
  if (_buffer) { //Update is running
    return false;
  }
  const esp_partition_t* partition = esp_ota_get_next_update_partition(NULL);
  return _partitionIsBootable(partition) && !esp_ota_set_boot_partition(partition);
}

bool UpdateClass::begin(size_t size, int command, int ledPin, uint8_t ledOn) {
  if (_size > 0) {
    log_w("already running");
    return false;
  }

  _ledPin = ledPin;
  _ledOn = !!ledOn; // 0(LOW) or 1(HIGH)

  _reset();
  _error = 0;

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
  }
  else if (command == U_SPIFFS) {
    _partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
    if (!_partition) {
      _error = UPDATE_ERROR_NO_PARTITION;
      return false;
    }
  }
  else {
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
  assert(_buffer == NULL);

  _buffer = (uint8_t*)malloc(SPI_FLASH_SEC_SIZE);
  if (!_buffer) {
    log_e("malloc failed");
    return false;
  }
  _size = size;
  _command = command;
  return true;
}

void UpdateClass::_abort(uint8_t err) {
  _reset();
  _error = err;
}

void UpdateClass::abort() {
  log_d("Aborted.");
  _abort(UPDATE_ERROR_ABORT);
}

bool UpdateClass::_writeBuffer() {
  UpdateProcessor::secure_update_processor_err_t e = UpdateProcessor::secure_update_processor_ERROR;

  if (_progress_callback)
    _progress_callback(_progress, _size);

  if (_ledPin != -1)
    digitalWrite(_ledPin, !digitalRead(_ledPin)); // Flash LED.

  if (!_post_header) {
    size_t l = _bufferLen;
    e = _processor->process_header(&_command, _buffer, &_bufferLen);

    if ((e == UpdateProcessor::secure_update_processor_AGAIN) &&
        (_bufferLen  < SPI_FLASH_SEC_SIZE) &&
        (_bufferLen < _size)
       ) {
      _progress += (l - _bufferLen);
      return true;
    }

    if (e != UpdateProcessor::secure_update_processor_OK) {
      log_e("Error duing processing of process_header.");
      _abort(UPDATE_ERROR_BAD_ARGUMENT);
      return false;
    };

    _progress += (l - _bufferLen);
    _post_header = true;
    return true;
  };

  if (_bufferLen != SPI_FLASH_SEC_SIZE && remaining() != _bufferLen) {
    log_e("Buffer passed is too short (and not the last one) - should never happen");
    _abort(UPDATE_ERROR_STREAM);
    return false;
  }

  e = _processor->process_payload(_buffer, &_bufferLen);

  if (e == UpdateProcessor::secure_update_processor_AGAIN)
    return true;

  if (e != UpdateProcessor::secure_update_processor_OK) {
    log_e("Failed to process the payload.");
    _abort(UPDATE_ERROR_STREAM);
    return false;
  }

  if (_progress_flash == 0) {
    //remove magic byte from the firmware now and write it upon success
    //this ensures that partially written firmware will not be bootable
    assert(_buffer[0] == ESP_IMAGE_HEADER_MAGIC);
    _buffer[0] = 0xFF;
  }
  if (!ESP.flashEraseSector((_partition->address + _progress_flash) / SPI_FLASH_SEC_SIZE)) {
    _abort(UPDATE_ERROR_ERASE);
    return false;
  };


  if (!ESP.flashWrite(_partition->address + _progress_flash, (uint32_t*)_buffer, _bufferLen)) {
    _abort(UPDATE_ERROR_WRITE);
    return false;
  }

  _progress_flash += _bufferLen;
  _progress += _bufferLen;
  _bufferLen = 0;
  return true;
}


bool UpdateClass::activate() {
  if (_command == U_FLASH) {
    if (!_enablePartition(_partition) || !_partitionIsBootable(_partition)) {
      log_e("could not enable/bootable partition");
      _abort(UPDATE_ERROR_READ);
      return false;
    }

    if (esp_ota_set_boot_partition(_partition)) {
      log_e("could not set the boot partition");
      _abort(UPDATE_ERROR_ACTIVATE);
      return false;
    }
    _reset();

#if 0
    if (1) {
      unsigned char buff[MBEDTLS_MD_MAX_SIZE];
      mbedtls_md_context_t ctx;
      String out = "";
      const mbedtls_md_info_t * t = mbedtls_md_info_from_string("SHA256");

      mbedtls_md_init(&ctx);
      assert(0 == mbedtls_md_setup(&ctx, t, 0));

      for (size_t l = 0; l < _progress_flash;) {
        unsigned char buf[1024];
        size_t len = _progress_flash - l;
        if (len > sizeof(buff)) len = sizeof(buff);

        ESP.flashRead(_partition->address + l, (uint32_t*)buf, len);
        assert(0 == mbedtls_md_update(&ctx, buff, len));

        l += len;
      };

      assert(0 == mbedtls_md_finish(&ctx, buff));

      for (int i = 0; i < mbedtls_md_get_size(t); i++)
        out += String(buff[i], HEX);

      log_d("RAW %s Digest %s", mbedtls_md_get_name(t), out.c_str());
      mbedtls_md_free(&ctx);
    }
#endif

    return true;
  } else if (_command == U_SPIFFS) {
    _reset();
    return true;
  }
  return false;
}

bool UpdateClass::end(bool evenIfRemaining) {

  if (hasError() || _size == 0) {
    log_e("Has error");
    _reset();
    return false;
  }

  if (!isFinished() && !evenIfRemaining) {
    log_e("premature end: res:%u, pos:%u/%u\n", getError(), progress(), _size);
    _abort(UPDATE_ERROR_ABORT);
    _reset();
    return false;
  }

  if (evenIfRemaining) {
    if (_bufferLen > 0) {
      _writeBuffer();
    }
    _size = progress();
  }

  if (_processor->process_end() != UpdateProcessor::secure_update_processor_OK) {
    log_e("Error during finalizing.");
    _abort(UPDATE_ERROR_ABORT);
    _reset();
    return false;
  };

  log_d("Reporting an OK back up the chain");
  return true;
}

size_t UpdateClass::write(uint8_t *data, size_t len) {
  size_t written = 0;

  if (hasError() || !isRunning()) {
    log_e("Error or not running");
    return 0;
  }

  if (len > remaining()) {
    log_e("Overfill (%d in buffer, %d left to go), did %d, would do %d, expecting %d", len, remaining(), _progress, _progress + len, size());
    _abort(UPDATE_ERROR_SPACE);
    return 0;
  }

  while (written < len) {
    size_t l = len - written;
    if (l + _bufferLen > SPI_FLASH_SEC_SIZE)
      l = SPI_FLASH_SEC_SIZE - _bufferLen;

    memcpy(_buffer + _bufferLen, data + written, l);
    _bufferLen += l;
    written += l;

    if (_bufferLen == SPI_FLASH_SEC_SIZE || _bufferLen == remaining()) {
      if (!_writeBuffer()) {
        log_d("Writebuffer fail after %d bytes written.", written);
        return written;
      };
    };
  }
  return written;
}

template<typename T>
size_t UpdateClass::write(T & data) {
  size_t written = 0;
  if (hasError() || !isRunning())
    return 0;

  size_t available = data.available();
  while (available) {
    if (_bufferLen + available > remaining()) {
      available = remaining() - _bufferLen;
    }

    size_t toBuff = _bufferLen + available;
    if (toBuff > SPI_FLASH_SEC_SIZE)
      toBuff = SPI_FLASH_SEC_SIZE;

    data.read(_buffer + _bufferLen, toBuff);
    _bufferLen += toBuff;

    if (_bufferLen == SPI_FLASH_SEC_SIZE || remaining() == _bufferLen) {
      log_d("_writeBuffer(%d), remain %d, SEC %d", _bufferLen,  remaining(), SPI_FLASH_SEC_SIZE);
      if (!_writeBuffer()) {
        return written;
      };
    } else {
      log_d("_writeBuffer(skip)");
    };

    written += toBuff;
    if (remaining() == 0)
      return written;

    available = data.available();
  }
  return written;
}


size_t UpdateClass::writeStream(Stream & data) {
  size_t written = 0;
  size_t toRead = 0;
  if (hasError() || !isRunning())
    return 0;

  while (remaining()) {
    size_t bytesToRead = SPI_FLASH_SEC_SIZE - _bufferLen;
    if (bytesToRead > remaining()) {
      bytesToRead = remaining();
    }

    toRead = data.readBytes(_buffer + _bufferLen,  bytesToRead);
    if (toRead == 0) { //Timeout
      delay(100);
      toRead = data.readBytes(_buffer + _bufferLen, bytesToRead);
      if (toRead == 0) { //Timeout
        _abort(UPDATE_ERROR_STREAM);
        return written;
      }
    }
    _bufferLen += toRead;
    if (_bufferLen == remaining() || _bufferLen == SPI_FLASH_SEC_SIZE) {
      log_d("_writeBuffer(%d), remain %d, SEC %d", _bufferLen,  remaining(), SPI_FLASH_SEC_SIZE);
      if (!_writeBuffer())
        return written;
    } else {
      log_d("_writeBuffer(skip)");
    };
    written += toRead;
  }
  return written;
}

void UpdateClass::printError(Stream & out) {
  out.println(_err2str(_error));
}

const char * UpdateClass::errorString() {
  return _err2str(_error);
}

UpdateClass Update;
