#ifndef __HTTP_UPDATE_SERVER_H
#define __HTTP_UPDATE_SERVER_H

#include <SPIFFS.h>
#include <StreamString.h>
#include <Update.h>
#include <WebServer.h>


static const char serverIndex[] PROGMEM =
  R"(<!DOCTYPE html>
     <html lang='en'>
     <head>
         <meta charset='utf-8'>
         <meta name='viewport' content='width=device-width,initial-scale=1'/>
     </head>
     <body>
     <form method='POST' action='' enctype='multipart/form-data'>
         Firmware:<br>
         <input type='file' accept='.bin,.bin.gz' name='firmware'>
         <input type='submit' value='Update Firmware'>
     </form>
     <form method='POST' action='' enctype='multipart/form-data'>
         FileSystem:<br>
         <input type='file' accept='.bin,.bin.gz,.image' name='filesystem'>
         <input type='submit' value='Update FileSystem'>
     </form>
     </body>
     </html>)";
static const char successResponse[] PROGMEM =
  "<META http-equiv=\"refresh\" content=\"15;URL=/\">Update Success! Rebooting...";

class HTTPUpdateServer {
public:
  HTTPUpdateServer(bool serial_debug = false) {
    _serial_output = serial_debug;
    _server = NULL;
    _username = emptyString;
    _password = emptyString;
    _authenticated = false;
  }

  void setup(WebServer* server) {
    setup(server, emptyString, emptyString);
  }

  void setup(WebServer* server, const String& path) {
    setup(server, path, emptyString, emptyString);
  }

  void setup(WebServer* server, const String& username, const String& password) {
    setup(server, "/update", username, password);
  }

  void setup(WebServer* server, const String& path, const String& username, const String& password) {

    _server = server;
    _username = username;
    _password = password;

    // handler for the /update form page
    _server->on(path.c_str(), HTTP_GET, [&]() {
      if (_username != emptyString && _password != emptyString && !_server->authenticate(_username.c_str(), _password.c_str()))
        return _server->requestAuthentication();
      _server->send_P(200, PSTR("text/html"), serverIndex);
    });

    // handler for the /update form POST (once file upload finishes)
    _server->on(
      path.c_str(), HTTP_POST, [&]() {
        if (!_authenticated)
          return _server->requestAuthentication();
        if (Update.hasError()) {
          _server->send(200, F("text/html"), String(F("Update error: ")) + _updaterError);
        } else {
          _server->client().setNoDelay(true);
          _server->send_P(200, PSTR("text/html"), successResponse);
          delay(100);
          _server->client().stop();
          ESP.restart();
        }
      },
      [&]() {
        // handler for the file upload, gets the sketch bytes, and writes
        // them through the Update object
        HTTPUpload& upload = _server->upload();

        if (upload.status == UPLOAD_FILE_START) {
          _updaterError.clear();
          if (_serial_output)
            Serial.setDebugOutput(true);

          _authenticated = (_username == emptyString || _password == emptyString || _server->authenticate(_username.c_str(), _password.c_str()));
          if (!_authenticated) {
            if (_serial_output)
              Serial.printf("Unauthenticated Update\n");
            return;
          }

          if (_serial_output)
            Serial.printf("Update: %s\n", upload.filename.c_str());
          if (upload.name == "filesystem") {
            if (!Update.begin(SPIFFS.totalBytes(), U_SPIFFS)) {  //start with max available size
              if (_serial_output) Update.printError(Serial);
            }
          } else {
            uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
            if (!Update.begin(maxSketchSpace, U_FLASH)) {  //start with max available size
              _setUpdaterError();
            }
          }
        } else if (_authenticated && upload.status == UPLOAD_FILE_WRITE && !_updaterError.length()) {
          if (_serial_output) Serial.printf(".");
          if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            _setUpdaterError();
          }
        } else if (_authenticated && upload.status == UPLOAD_FILE_END && !_updaterError.length()) {
          if (Update.end(true)) {  //true to set the size to the current progress
            if (_serial_output) Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
          } else {
            _setUpdaterError();
          }
          if (_serial_output) Serial.setDebugOutput(false);
        } else if (_authenticated && upload.status == UPLOAD_FILE_ABORTED) {
          Update.end();
          if (_serial_output) Serial.println("Update was aborted");
        }
        delay(0);
      });
  }

  void updateCredentials(const String& username, const String& password) {
    _username = username;
    _password = password;
  }

protected:
  void _setUpdaterError() {
    if (_serial_output) Update.printError(Serial);
    StreamString str;
    Update.printError(str);
    _updaterError = str.c_str();
  }

private:
  bool _serial_output;
  WebServer* _server;
  String _username;
  String _password;
  bool _authenticated;
  String _updaterError;
};


#endif
