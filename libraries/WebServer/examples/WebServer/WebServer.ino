// @file WebServer.ino
// @brief Example WebServer implementation using the ESP32 WebServer
// and most common use cases related to web servers.
//
// * Setup a web server
// * redirect when accessing the url with servername only
// * get real time by using builtin NTP functionality
// * send HTML responses from Sketch (see builtinfiles.h)
// * use a LittleFS file system on the data partition for static files
// * use http ETag Header for client side caching of static files
// * use custom ETag calculation for static files
// * extended FileServerHandler for uploading and deleting static files
// * extended FileServerHandler for uploading and deleting static files
// * serve APIs using REST services (/api/list, /api/sysinfo)
// * define HTML response when no file/api/handler was found
//
// See also README.md for instructions and hints.
//
// Please use the following Arduino IDE configuration
//
// * Board: ESP32 Dev Module
// * Partition Scheme: Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)
//     but LittleFS will be used in the partition (not SPIFFS)
// * other setting as applicable
//
// Changelog:
// 21.07.2021 creation, first version
// 08.01.2023 ESP32 version with ETag

#include <Arduino.h>
#include <WebServer.h>

#include "secrets.h"  // add WLAN Credentials in here.

#include <FS.h>        // File System for Web Server Files
#include <LittleFS.h>  // This file system is used.

// mark parameters not used in example
#define UNUSED __attribute__((unused))

// TRACE output simplified, can be deactivated here
#define TRACE(...) Serial.printf(__VA_ARGS__)

// name of the server. You reach it using http://webserver
#define HOSTNAME "webserver"

// local time zone definition (Berlin)
#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"

// need a WebServer for http access on port 80.
WebServer server(80);

// The text of builtin files are in this header file
#include "builtinfiles.h"

// enable the CUSTOM_ETAG_CALC to enable calculation of ETags by a custom function
#define CUSTOM_ETAG_CALC

// ===== Simple functions used to answer simple GET requests =====

// This function is called when the WebServer was requested without giving a filename.
// This will redirect to the file index.htm when it is existing otherwise to the built-in $upload.htm page
void handleRedirect() {
  TRACE("Redirect...\n");
  String url = "/index.htm";

  if (!LittleFS.exists(url)) { url = "/$upload.htm"; }

  server.sendHeader("Location", url, true);
  server.send(302);
}  // handleRedirect()


// This function is called when the WebServer was requested to list all existing files in the filesystem.
// a JSON array with file information is returned.
void handleListFiles() {
  File dir = LittleFS.open("/", "r");
  String result;

  result += "[\n";
  while (File entry = dir.openNextFile()) {
    if (result.length() > 4) { result += ",\n"; }
    result += "  {";
    result += "\"type\": \"file\", ";
    result += "\"name\": \"" + String(entry.name()) + "\", ";
    result += "\"size\": " + String(entry.size()) + ", ";
    result += "\"time\": " + String(entry.getLastWrite());
    result += "}";
  }  // while

  result += "\n]";
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "text/javascript; charset=utf-8", result);
}  // handleListFiles()


// This function is called when the sysInfo service was requested.
void handleSysInfo() {
  String result;

  result += "{\n";
  result += "  \"Chip Model\": " + String(ESP.getChipModel()) + ",\n";
  result += "  \"Chip Cores\": " + String(ESP.getChipCores()) + ",\n";
  result += "  \"Chip Revision\": " + String(ESP.getChipRevision()) + ",\n";
  result += "  \"flashSize\": " + String(ESP.getFlashChipSize()) + ",\n";
  result += "  \"freeHeap\": " + String(ESP.getFreeHeap()) + ",\n";
  result += "  \"fsTotalBytes\": " + String(LittleFS.totalBytes()) + ",\n";
  result += "  \"fsUsedBytes\": " + String(LittleFS.usedBytes()) + ",\n";
  result += "}";

  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "text/javascript; charset=utf-8", result);
}  // handleSysInfo()


// ===== Request Handler class used to answer more complex requests =====

// The FileServerHandler is registered to the web server to support DELETE and UPLOAD of files into the filesystem.
class FileServerHandler : public RequestHandler {
public:
  // @brief Construct a new File Server Handler object
  // @param fs The file system to be used.
  // @param path Path to the root folder in the file system that is used for serving static data down and upload.
  // @param cache_header Cache Header to be used in replies.
  FileServerHandler() {
    TRACE("FileServerHandler is registered\n");
  }


  // @brief check incoming request. Can handle POST for uploads and DELETE.
  // @param requestMethod method of the http request line.
  // @param requestUri request ressource from the http request line.
  // @return true when method can be handled.
  bool canHandle(HTTPMethod requestMethod, String UNUSED uri) override {
    return ((requestMethod == HTTP_POST) || (requestMethod == HTTP_DELETE));
  }  // canHandle()


  bool canUpload(String uri) override {
    // only allow upload on root fs level.
    return (uri == "/");
  }  // canUpload()


  bool handle(WebServer &server, HTTPMethod requestMethod, String requestUri) override {
    // ensure that filename starts with '/'
    String fName = requestUri;
    if (!fName.startsWith("/")) { fName = "/" + fName; }

    TRACE("handle %s\n", fName.c_str());

    if (requestMethod == HTTP_POST) {
      // all done in upload. no other forms.

    } else if (requestMethod == HTTP_DELETE) {
      if (LittleFS.exists(fName)) {
        TRACE("DELETE %s\n", fName.c_str());
        LittleFS.remove(fName);
      }
    }  // if

    server.send(200);  // all done.
    return (true);
  }  // handle()


  // uploading process
  void
  upload(WebServer UNUSED &server, String UNUSED _requestUri, HTTPUpload &upload) override {
    // ensure that filename starts with '/'
    static size_t uploadSize;

    if (upload.status == UPLOAD_FILE_START) {
      String fName = upload.filename;

      // Open the file for writing
      if (!fName.startsWith("/")) { fName = "/" + fName; }
      TRACE("start uploading file %s...\n", fName.c_str());

      if (LittleFS.exists(fName)) {
        LittleFS.remove(fName);
      }  // if
      _fsUploadFile = LittleFS.open(fName, "w");
      uploadSize = 0;

    } else if (upload.status == UPLOAD_FILE_WRITE) {
      // Write received bytes
      if (_fsUploadFile) {
        size_t written = _fsUploadFile.write(upload.buf, upload.currentSize);
        if (written < upload.currentSize) {
          // upload failed
          TRACE("  write error!\n");
          _fsUploadFile.close();

          // delete file to free up space in filesystem
          String fName = upload.filename;
          if (!fName.startsWith("/")) { fName = "/" + fName; }
          LittleFS.remove(fName);
        }
        uploadSize += upload.currentSize;
        // TRACE("free:: %d of %d\n", LittleFS.usedBytes(), LittleFS.totalBytes());
        // TRACE("written:: %d of %d\n", written, upload.currentSize);
        // TRACE("totalSize: %d\n", upload.currentSize + upload.totalSize);
      }  // if

    } else if (upload.status == UPLOAD_FILE_END) {
        TRACE("finished.\n");
      // Close the file
      if (_fsUploadFile) {
        _fsUploadFile.close();
        TRACE(" %d bytes uploaded.\n", upload.totalSize);
      }
    }  // if

  }  // upload()


protected:
  File _fsUploadFile;
};


// Setup everything to make the webserver work.
void setup(void) {
  delay(3000);  // wait for serial monitor to start completely.

  // Use Serial port for some trace information from the example
  Serial.begin(115200);
  Serial.setDebugOutput(false);

  TRACE("Starting WebServer example...\n");

  TRACE("Mounting the filesystem...\n");
  if (!LittleFS.begin()) {
    TRACE("could not mount the filesystem...\n");
    delay(2000);
    TRACE("formatting...\n");
    LittleFS.format();
    delay(2000);
    TRACE("restart.\n");
    delay(2000);
    ESP.restart();
  }

  // allow to address the device by the given name e.g. http://webserver
  WiFi.setHostname(HOSTNAME);

  // start WiFI
  WiFi.mode(WIFI_STA);
  if (strlen(ssid) == 0) {
    WiFi.begin();
  } else {
    WiFi.begin(ssid, passPhrase);
  }

  TRACE("Connect to WiFi...\n");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    TRACE(".");
  }
  TRACE("connected.\n");

  // Ask for the current time using NTP request builtin into ESP firmware.
  TRACE("Setup ntp...\n");
  configTzTime(TIMEZONE, "pool.ntp.org");

  TRACE("Register redirect...\n");

  // register a redirect handler when only domain name is given.
  server.on("/", HTTP_GET, handleRedirect);

  TRACE("Register service handlers...\n");

  // serve a built-in htm page
  server.on("/$upload.htm", []() {
    server.send(200, "text/html", FPSTR(uploadContent));
  });

  // register some REST services
  server.on("/api/list", HTTP_GET, handleListFiles);
  server.on("/api/sysinfo", HTTP_GET, handleSysInfo);

  TRACE("Register file system handlers...\n");

  // UPLOAD and DELETE of files in the file system using a request handler.
  server.addHandler(new FileServerHandler());

  // // enable CORS header in webserver results
  server.enableCORS(true);

  // enable ETAG header in webserver results (used by serveStatic handler)
#if defined(CUSTOM_ETAG_CALC)
  // This is a fast custom eTag generator. It returns a value based on the time the file was updated like
  // ETag: 63bbceb5
  server.enableETag(true, [](FS &fs, const String &path) -> String {
    File f = fs.open(path, "r");
    String eTag = String(f.getLastWrite(), 16);  // use file modification timestamp to create ETag
    f.close();
    return (eTag);
  });

#else
  // enable standard ETAG calculation using md5 checksum of file content.
  server.enableETag(true);
#endif

  // serve all static files
  server.serveStatic("/", LittleFS, "/");

  TRACE("Register default (not found) answer...\n");

  // handle cases when file is not found
  server.onNotFound([]() {
    // standard not found in browser.
    server.send(404, "text/html", FPSTR(notFoundContent));
  });

  server.begin();

  TRACE("open <http://%s> or <http://%s>\n",
        WiFi.getHostname(),
        WiFi.localIP().toString().c_str());
}  // setup


// run the server...
void loop(void) {
  server.handleClient();
}  // loop()

// end.
