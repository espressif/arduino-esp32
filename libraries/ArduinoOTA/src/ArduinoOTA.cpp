
#ifndef LWIP_OPEN_SRC
#define LWIP_OPEN_SRC
#endif

#include <functional>
#include <WiFiUdp.h>
#include "ArduinoOTA.h"
#include "ESPmDNS.h"
#include "MD5Builder.h"
#include "Update.h"
#include <stdarg.h>

// #define OTA_DEBUG Serial

ArduinoOTAClass::ArduinoOTAClass()
  : _port(0)
  , _initialized(false)
  , _rebootOnSuccess(true)
  , _mdnsEnabled(true)
  , _state(OTA_IDLE)
  , _size(0)
  , _cmd(0)
  , _ota_port(0)
  , _ota_timeout(1000)
  , _md_info(NULL)
  , _md_auth_info(NULL)
  , _md_ctx(NULL)
  , _md_min(MBEDTLS_MD_MD5)
  , _digestAsHexString("")
  , _start_callback(NULL)
  , _end_callback(NULL)
  , _error_callback(NULL)
  , _progress_callback(NULL)
  , _approve_reboot_callback(NULL)
{
}

ArduinoOTAClass::~ArduinoOTAClass() {
  _udp_ota.stop();
  if (_md_ctx) {
    mbedtls_md_free(_md_ctx);
    free(_md_ctx);
  };
  _md_ctx = NULL;
}

ArduinoOTAClass& ArduinoOTAClass::onStart(THandlerFunction fn) {
  _start_callback = fn;
  return *this;
}

ArduinoOTAClass& ArduinoOTAClass::onEnd(THandlerFunction fn) {
  _end_callback = fn;
  return *this;
}

ArduinoOTAClass& ArduinoOTAClass::onProgress(THandlerFunction_Progress fn) {
  _progress_callback = fn;
  return *this;
}

ArduinoOTAClass& ArduinoOTAClass::onError(THandlerFunction_Error fn) {
  _error_callback = fn;
  return *this;
}

ArduinoOTAClass& ArduinoOTAClass::approveReboot(THandlerFunction_ApproveReboot fn) {
  _approve_reboot_callback = fn;
  return *this;
}


ArduinoOTAClass& ArduinoOTAClass::setPort(uint16_t port) {
  if (!_initialized && !_port && port) {
    _port = port;
  }
  return *this;
}

ArduinoOTAClass& ArduinoOTAClass::setHostname(const char * hostname) {
  if (!_initialized && !_hostname.length() && hostname) {
    _hostname = hostname;
  }
  return *this;
}

String ArduinoOTAClass::getHostname() {
  return _hostname;
}

ArduinoOTAClass& ArduinoOTAClass::setPassword(const char *password, const char * digest_type) {
  String p = String(password);
  if (digest_type == NULL)
    digest_type = "MD5";

  if (!_initialized && !_hostname.length() && password)
    _password = _hexDigest(mbedtls_md_info_from_string(digest_type), p);

  return *this;
}

ArduinoOTAClass& ArduinoOTAClass::setPasswordHash(const char * password) {
  if (!_initialized && !_password.length() && password) {
    _password = password;
  }
  return *this;
}

ArduinoOTAClass& ArduinoOTAClass::setRebootOnSuccess(bool reboot) {
  _rebootOnSuccess = reboot;
  return *this;
}

ArduinoOTAClass& ArduinoOTAClass::setMdnsEnabled(bool enabled) {
  _mdnsEnabled = enabled;
  return *this;
}

ArduinoOTAClass&  ArduinoOTAClass::setProcessor(UpdateProcessor * processor) {
  Update.setProcessor(processor);
  return *this;
}

void ArduinoOTAClass::begin() {
  if (_initialized) {
    log_w("already initialized");
    return;
  }

  if (!_port) {
    _port = 3232;
  }

  if (!_udp_ota.begin(_port)) {
    log_e("udp bind failed");
    return;
  }

  if (!_hostname.length()) {
    char tmp[20];
    uint8_t mac[6];
    WiFi.macAddress(mac);
    sprintf(tmp, "esp32-%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    _hostname = tmp;
  }
  if (_mdnsEnabled) {
    MDNS.begin(_hostname.c_str());
    MDNS.enableArduino(_port, (_password.length() > 0));
  }
  _initialized = true;
  _state = OTA_IDLE;
  log_i("OTA server at: %s.local:%u", _hostname.c_str(), _port);
}

int ArduinoOTAClass::parseInt() {
  char data[INT_BUFFER_SIZE];
  uint8_t index = 0;
  char value;
  while (_udp_ota.peek() == ' ') _udp_ota.read();
  while (index < INT_BUFFER_SIZE - 1) {
    value = _udp_ota.peek();
    if (value < '0' || value > '9') {
      data[index++] = '\0';
      return atoi(data);
    }
    data[index++] = _udp_ota.read();
  }
  return 0;
}

String ArduinoOTAClass::readStringUntil(char end) {
  String res = "";
  int value;
  while (true) {
    value = _udp_ota.read();
    if (value <= 0 || value == end) {
      return res;
    }
    res += (char)value;
  }
  return res;
}

// default / original format
char * ArduinoOTAClass::_processsOldStyleHeader() {
  _cmd = parseInt();
  _ota_port = parseInt();
  _size = parseInt();
  _udp_ota.read();

  _digestAsHexString = readStringUntil('\n');
  _digestAsHexString.trim();

  if (_digestAsHexString.length() == 32)
    _md_info = mbedtls_md_info_from_type(MBEDTLS_MD_MD5);
  else if (_digestAsHexString.length() == 40)
    _md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
  else if (_digestAsHexString.length() == 64)
    _md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  else {
    return (char *)"Unknown digest length";
  };

  // The password, if any; is always MD5;
  _md_auth_info = mbedtls_md_info_from_type(MBEDTLS_MD_MD5);
  return NULL;
}

char * ArduinoOTAClass::_processsHeader() {
  char buff[256];

  _ota_port = _size = 0;
  _md_info = NULL;
  _cmd = -1;
  _digestAsHexString = "";

  if (isdigit(_udp_ota.peek()))
    return _processsOldStyleHeader();

  if ((_udp_ota.readBytesUntil('/', buff, sizeof(buff)) != 6) || bcmp(buff, "RedWax", 6))
    return (char *)"Unknown protocol, ignoring.";

  if (_udp_ota.readStringUntil(' ').toInt() != 1)
    return (char *)"Can only handle the RedWax/1.XX hdr";

  bool eol = false;
  while (!eol) {
    size_t l;

    while (_udp_ota.peek() == ' ')
      _udp_ota.read();

    if (_udp_ota.peek() == '\n')
      break;

    // readStringUntil a SPACE or a NL; and bail on ay non-ASCCI
    // chars in the header.
    //
    for (l = 0; l < sizeof(buff) - 1; l++) {
      buff[l] = _udp_ota.read();
      if (buff[l] > ' ' && buff[l] < 127 && l <  sizeof(buff) - 1)
        continue;
      if (buff[l] != '\n' && buff[l] != ' ')
        return (char *)"Illegal char/length of RedWax/1.XX hdr";
      break;
    }
    eol =  (buff[l] == '\n');
    buff[l] = 0;

    if (0 == bcmp("cmd=", buff, 4))
      _cmd = atoi(buff + 4);
    else if (0 == bcmp("port=", buff, 5))
      _ota_port = atoi(buff + 5);
    else if (0 == bcmp("size=", buff, 5))
      _size = atoi(buff + 5);
    else if (0 == bcmp("digest=", buff, 7))
      _digestAsHexString = String(buff + 7);
    else if (0 == bcmp("md=", buff, 3)) {
      if ((_md_info = mbedtls_md_info_from_string(buff + 3)) == NULL)
        return (char *)"Unknown digest";
    }
    else if (0 == bcmp("authmd=", buff, 7) && ((_md_auth_info = mbedtls_md_info_from_string(buff + 7)) == NULL)) {
      return (char *)"Unknown auth digest";
    } else {
      log_i("Ignoring OTA key/value param '%s'", buff);
    };
  } // while loop.

  if (!_ota_port || !_size || !_md_info || _cmd == -1 || _digestAsHexString == "" )
    return (char *)"Midding OTA parameters";

  if (_digestAsHexString.length()  != 2 * mbedtls_md_get_size(_md_info))
    return (char *)"Wrong digest size";

  return NULL;
}


void ArduinoOTAClass::_onRxHeaderPhase() {
  char * e;
  if ((e = _processsHeader()) != NULL) {
    _reportErrorAndAbort(OTA_BEGIN_ERROR, (char *)"Header error: %s", e);
    return;
  };

  if (_cmd != U_FLASH && _cmd != U_SPIFFS) {
    _reportErrorAndAbort(OTA_BEGIN_ERROR, (char *)"Unknown command (%04x)", _cmd);
    return;
  };

  log_i("OTA Outer Digest: %s: %s", mbedtls_md_get_name(_md_info), _digestAsHexString.c_str());

  if (mbedtls_md_get_type(_md_info) < _md_min) {
    _reportErrorAndAbort(OTA_AUTH_ERROR, (char *)"Insecure digest (%s or better expected)", mbedtls_md_get_name(mbedtls_md_info_from_type(_md_min)));
    return;
  };

  for (int i = 0; i < mbedtls_md_get_size(_md_info); i++) {
    _md_buff[i] = strtoul(_digestAsHexString.substring(i * 2, i * 2 + 2).c_str(), NULL, 16);
  };

  int ret;
  if (_md_ctx) {
    mbedtls_md_free(_md_ctx);
    free(_md_ctx);
  }
  if ((_md_ctx = (mbedtls_md_context_t *)malloc(sizeof(*_md_ctx))) == NULL)
  {
    _reportErrorAndAbort(OTA_AUTH_ERROR, (char *)"could not malloc digest");
    return;
  }
  mbedtls_md_init(_md_ctx);
  if ((ret = mbedtls_md_setup(_md_ctx, _md_info, 0)) != 0) {
    _reportErrorAndAbort(OTA_AUTH_ERROR, (char *)"could not setup digest: %x", -ret);
    return;
  }
  // beyond here we rely on the _state going back to IDE for
  // cleaning up the digest memory.

  if (_password.length()) {
    if (mbedtls_md_get_type(_md_auth_info) < _md_min) {
      _reportErrorAndAbort(OTA_AUTH_ERROR, (char *)"Insecure Auth Digest (%s or better expected)", mbedtls_md_get_name(mbedtls_md_info_from_type(_md_min)));
      return;
    };

    log_i("Auth Digest: %s\n", mbedtls_md_get_name(_md_auth_info));

    String seed = String(micros()) + String(esp_random()) + _hostname;

    _nonce = _hexDigest(_md_auth_info, seed);
    if (_nonce.length() != mbedtls_md_get_size(_md_auth_info) * 2) {
      _reportErrorAndAbort(OTA_AUTH_ERROR, (char *)"Auth Digest creation error", mbedtls_md_get_name(mbedtls_md_info_from_type(_md_min)));
      return;
    };
    _replyPacket((char *)"AUTH %s", _nonce.c_str());
    _state = OTA_WAITAUTH;
    return;
  };
  _replyPacket((char *)"OK");

  _ota_ip = _udp_ota.remoteIP();
  _state = OTA_RUNUPDATE;
};

String ArduinoOTAClass::_hexDigest(const mbedtls_md_info_t * t, String & str) {
  unsigned char buff[MBEDTLS_MD_MAX_SIZE];
  mbedtls_md_context_t ctx;
  String out = "";

  mbedtls_md_init(&ctx);
  if (
    (mbedtls_md_setup(&ctx, t, 0) == 0) &&
    (mbedtls_md_update(&ctx, (const unsigned char*)(str.c_str()), str.length()) == 0) &&
    (mbedtls_md_finish(&ctx, buff) == 0)
  ) {
    for (int i = 0; i < mbedtls_md_get_size(t); i++)
      out += String(buff[i], HEX);
  };
  mbedtls_md_free(&ctx);
  return out;
}

void ArduinoOTAClass::_onRxDigestAuthPhase() {
  int cmd = parseInt();
  if (cmd != U_AUTH) {
    _reportErrorAndAbort(OTA_BEGIN_ERROR, (char *)"Did not get expected AUTH (%d) command: %d", U_AUTH, cmd);
    return;
  }

  _udp_ota.read();
  String cnonce = readStringUntil(' ');
  String response = readStringUntil('\n');

  if (cnonce.length()  != mbedtls_md_get_size(_md_auth_info) || response.length() != mbedtls_md_get_size(_md_auth_info)) {
    _reportErrorAndAbort(OTA_AUTH_ERROR, (char *)"auth param fail");
    return;
  }

  String challenge = _password + ":" + String(_nonce) + ":" + cnonce;
  String result = _hexDigest(_md_auth_info, challenge);

  if (result.length() != mbedtls_md_get_size(_md_auth_info) * 2) {
    _reportErrorAndAbort(OTA_AUTH_ERROR, (char *)"Auth Digest error", mbedtls_md_get_name(mbedtls_md_info_from_type(_md_min)));
    return;
  }

  if (!result.equals(response)) {
    _reportErrorAndAbort(OTA_AUTH_ERROR, (char *)"Authentication Failed");
    return;
  };

  _replyPacket((char *)"OK");
  _ota_ip = _udp_ota.remoteIP();
  _state = OTA_RUNUPDATE;
};


void ArduinoOTAClass::_reportErrorAndAbort(ota_error_t error, const char * fmt, ...) {
  va_list ap;
  char _err[128];
  va_start(ap, fmt);
  vsnprintf(_err, sizeof(_err), fmt, ap);
  va_end(ap);

  if (_error_callback)
    _error_callback(error);

  log_e( "Error: %s", _err);

  _replyPacket(_err);

  _goIdle();
  return;
}

void ArduinoOTAClass::_goIdle() {
  if (_state == OTA_RUNUPDATE) {
    Update.abort();
  };
  if (_md_ctx) {
    mbedtls_md_free(_md_ctx);
    free(_md_ctx);
    _md_ctx = NULL;
  }
  _state = OTA_IDLE;

}

void ArduinoOTAClass::_replyPacket(char * fmt, ...) {
  va_list ap;
  char buff[128];
  va_start(ap, fmt);
  vsnprintf(buff, sizeof(buff), fmt, ap);
  va_end(ap);

  _udp_ota.beginPacket(_udp_ota.remoteIP(), _udp_ota.remotePort());
  _udp_ota.print(buff);
  _udp_ota.endPacket();
};


char * ArduinoOTAClass::_handleUpdate(WiFiClient & client) {
  uint32_t written = 0, total = 0, timeouts = 0, errs = 0;

  while (!Update.isFinished() && client.connected()) {
    size_t waited = _ota_timeout;
    size_t available = client.available();

    while (!available && waited) {
      delay(1 /* miliseconds */);
      waited--;
      available = client.available();
    }
    if (waited == 0) {
      log_i("ReTry[%u]: %u", waited, timeouts);

      if (timeouts++ > 3)
        return (char *)"Receive timeout";

      if (!client.printf("%u\n", written))
        return (char *)"failed to return bytes written";

      continue;
    }
    if (!available)
      return (char *)"No Data";

    timeouts = 0;
    static uint8_t buf[1460];
    if (available > 1460) {
      available = 1460;
    }
    size_t r = client.read(buf, available);
    if (r == -1 && errs < 3) {
      errs++;
    }
    if (r != available) {
      log_w("Didn't read enough! %u != %u", r, available);
    }

    written = Update.write(buf, r);
    if (written <= 0 || written != r)
      return (char *)"Firmware Write error";
    if (written != r)
      return (char *)"Incomplete Firware Write";

    if ((mbedtls_md_update(_md_ctx, buf, written)) != 0)
      return (char *)"Digest update failed";

    total += written;

    if (_progress_callback) {
      _progress_callback(total, _size);
    }

    if (!client.printf("%u\n", written))
      return (char *)"failed to return bytes written";
  } // while we're connected and there is data available.
  return NULL;
}

void ArduinoOTAClass::_runUpdate() {

  if (!Update.begin(_size, _cmd)) {
    _reportErrorAndAbort(OTA_BEGIN_ERROR, (char *)"Begin ERROR: %s\n", Update.errorString());
    return;
  }

  if (_start_callback) {
    _start_callback();
  }

  if (_progress_callback) {
    _progress_callback(0, _size);
  }
  mbedtls_md_starts(_md_ctx);

  WiFiClient client;
  if (!client.connect(_ota_ip, _ota_port)) {
    _reportErrorAndAbort(OTA_CONNECT_ERROR, (char *)"Connect ERROR: %s:%d %s\n", String(_ota_ip).c_str(), _ota_port, Update.errorString());
    return;
  }

  char * err = _handleUpdate(client);
  if (err)
    printf("\nHandling error: %s\n", err);
  else
    printf("OK!");

  if (err)
    client.print(err);
  else
    client.println("OK");

  client.stop();
  delay(10);

  if (err) {
    _reportErrorAndAbort(OTA_RECEIVE_ERROR, err);
    return;
  };

  if (!Update.end()) {
    _reportErrorAndAbort(OTA_END_ERROR, "Could not finalize update");
    return;
  }

  unsigned char buff[MBEDTLS_MD_MAX_SIZE];
  int ret;
  if ((ret = mbedtls_md_finish(_md_ctx, buff)) != 0) {
    _reportErrorAndAbort(OTA_END_ERROR, "Outer digest calculation failed: %x", -ret);
    return;
  }

  if (bcmp(buff, _md_buff, mbedtls_md_get_size(_md_info)) != 0) {
    _reportErrorAndAbort(OTA_END_ERROR, "Outer digest mismatch, rejecting");
    return;
  };

  log_d("OTA Outer Digest matched.");

  if (_end_callback) {
    _end_callback();
  }

  if (_approve_reboot_callback && !(_approve_reboot_callback())) {
    _replyPacket((char *)"No permission to activate from current firmware");
    _goIdle();
    return;
  };

  if (!Update.activate()) {
    _reportErrorAndAbort(OTA_END_ERROR, "Failed to activate");
    return;
  };

  _goIdle();

  if (_rebootOnSuccess) {
    //let serial/network finish tasks that might be given in _end_callback
    delay(100);
    ESP.restart();
  }
  return;
}

void ArduinoOTAClass::end() {
  _initialized = false;
  _goIdle();
  _udp_ota.stop();
  if (_mdnsEnabled) {
    MDNS.end();
  }
  log_i("OTA server stopped.");
}

void ArduinoOTAClass::handle() {
  if (!_initialized) {
    return;
  }
  switch (_state) {
    case OTA_RUNUPDATE:
      _runUpdate();
      break;
    case OTA_WAITAUTH:
      if (_udp_ota.parsePacket())
        _onRxDigestAuthPhase();
      break;
    case OTA_IDLE:
      if (_udp_ota.parsePacket())
        _onRxHeaderPhase();
      break;
    default:
      _reportErrorAndAbort(OTA_BEGIN_ERROR, "Confused.");
      break;
  }
  _udp_ota.flush(); // always flush, even zero length packets must be flushed.
}

int ArduinoOTAClass::getCommand() {
  return _cmd;
}

void ArduinoOTAClass::setTimeout(int timeoutInMillis) {
  _ota_timeout = timeoutInMillis;
}

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_ARDUINOOTA)
ArduinoOTAClass ArduinoOTA;
#endif
