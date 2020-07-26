#ifndef __SECURE_ARDUINO_OTA_H
#define __SECURE_ARDUINO_OTA_H

#include <WiFi.h>
#include <functional>
#include "Update.h"

#include <mbedtls/md.h>

#define INT_BUFFER_SIZE 16

typedef enum {
  OTA_IDLE,
  OTA_WAITAUTH,
  OTA_RUNUPDATE
} ota_state_t;

typedef enum {
  OTA_AUTH_ERROR,
  OTA_BEGIN_ERROR,
  OTA_CONNECT_ERROR,
  OTA_RECEIVE_ERROR,
  OTA_END_ERROR
} ota_error_t;

class ArduinoOTAClass
{
  public:
    typedef std::function<void()> THandlerFunction;
    typedef std::function<void(ota_error_t)> THandlerFunction_Error;
    typedef std::function<void(unsigned int, unsigned int)> THandlerFunction_Progress;
    typedef std::function<bool()> THandlerFunction_ApproveReboot;

    ArduinoOTAClass();
    ~ArduinoOTAClass();

    //Sets the service port. Default 3232
    ArduinoOTAClass& setPort(uint16_t port);

    //Sets the device hostname. Default esp32-xxxxxx
    ArduinoOTAClass& setHostname(const char *hostname);
    String getHostname();

    //Sets the password that will be required for OTA. Default NULL
    ArduinoOTAClass& setPassword(const char *password, const char * digest_type = "MD5");

    //Sets the password as above but in the form MD5(password). Default NULL
    ArduinoOTAClass& setPasswordHash(const char *password);

    //Sets if the device should be rebooted after successful update. Default true
    ArduinoOTAClass& setRebootOnSuccess(bool reboot);

    //Sets if the device should advertise itself to Arduino IDE. Default true
    ArduinoOTAClass& setMdnsEnabled(bool enabled);

    //This callback will be called when OTA connection has begun
    ArduinoOTAClass& onStart(THandlerFunction fn);

    //This callback will be called when OTA has finished
    ArduinoOTAClass& onEnd(THandlerFunction fn);

    //This callback will be called when OTA encountered Error
    ArduinoOTAClass& onError(THandlerFunction_Error fn);

    //This callback will be called when OTA is receiving data
    ArduinoOTAClass& onProgress(THandlerFunction_Progress fn);

    // This callback will be called just before rebooting; giving
    // a last chance to abort the process.
    ArduinoOTAClass& approveReboot(THandlerFunction_ApproveReboot fn);

    ArduinoOTAClass& setProcessor(UpdateProcessor * processor);

    //Starts the ArduinoOTA service
    void begin();

    //Ends the ArduinoOTA service
    void end();

    //Call this in loop() to run the service
    void handle();

    //Gets update command type after OTA has started. Either U_FLASH or U_SPIFFS
    int getCommand();

    void setTimeout(int timeoutInMillis);

    int setMinimalDigestLevel(const mbedtls_md_info_t * m) {
      if (m == NULL)
        return -1;
      _md_min = mbedtls_md_get_type(m);
      return 0;
    };

    int setMinimalDigestLevel(String str) {
      return setMinimalDigestLevel(str.c_str());
    };

    int setMinimalDigestLevel(const char * str) {
      return setMinimalDigestLevel(mbedtls_md_info_from_string(str));
    };

    int setMinimalDigestLevel(mbedtls_md_type_t min) {
      return setMinimalDigestLevel(mbedtls_md_info_from_type(min));
    };


  private:
    int _port;
    String _password;
    String _hostname;
    String _nonce;
    WiFiUDP _udp_ota;
    bool _initialized;
    bool _rebootOnSuccess;
    bool _mdnsEnabled;
    ota_state_t _state;
    int _size;
    int _cmd;
    int _ota_port;
    int _ota_timeout;
    IPAddress _ota_ip;

    const mbedtls_md_info_t *_md_info, *_md_auth_info;
    mbedtls_md_context_t * _md_ctx;
    unsigned char _md_buff[MBEDTLS_MD_MAX_SIZE];
    mbedtls_md_type_t _md_min;
    String _digestAsHexString;

    THandlerFunction _start_callback;
    THandlerFunction _end_callback;
    THandlerFunction_Error _error_callback;
    THandlerFunction_Progress _progress_callback;
    THandlerFunction_ApproveReboot _approve_reboot_callback;

    void  _reportErrorAndAbort(ota_error_t error, const char * errmsg, ...);
    void  _goIdle();
    void _replyPacket(char * msg, ...);

    String _hexDigest(const mbedtls_md_info_t * t, String &str);

    void _onRxHeaderPhase();
    char * _processsOldStyleHeader();
    char * _processsHeader();

    void _onRxDigestAuthPhase();
    void _runUpdate(void);

    char * _handleUpdate(WiFiClient & client);

    int parseInt(void);
    String readStringUntil(char end);
};

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_ARDUINOOTA)
extern ArduinoOTAClass ArduinoOTA;
#endif

#endif /* __ARDUINO_OTA_H */
