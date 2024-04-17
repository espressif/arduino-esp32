/*
An example of how to use HTTPClient to download an encrypted and plain image files OTA from a web server.
This example uses Wifi & HTTPClient to connect to webserver and two functions for obtaining firmware image from webserver.
One uses the example 'updater.php' code on server to check and/or send relevant download firmware image file,
the other directly downloads the firmware file from web server.

To use:-
Make a folder/directory on your webserver where your firmware images will be uploaded to. ie. /firmware
The 'updater.php' file can also be uploaded to the same folder. Edit and change definitions in 'update.php' to suit your needs.
In sketch:
   set HTTPUPDATE_HOST         to domain name or IP address if on LAN of your web server
   set HTTPUPDATE_UPDATER_URI  to path and file to call 'updater.php'
or set HTTPUPDATE_DIRECT_URI   to path and firmware file to download
   edit other HTTPUPDATE_ as needed

Encrypted image will help protect your app image file from being copied and used on blank devices, encrypt your image file by using espressif IDF.
First install an app on device that has Update setup with the OTA decrypt mode on, same key, address and flash_crypt_conf as used in IDF to encrypt image file or vice versa.

For easier development use the default U_AES_DECRYPT_AUTO decrypt mode. This mode allows both plain and encrypted app images to be uploaded.

Note:- App image can also encrypted on device, by using espressif IDF to configure & enabled FLASH encryption, suggest the use of a different 'OTA_KEY' key for update from the eFuses 'flash_encryption' key used by device.

  ie. "Update.setupCrypt(OTA_KEY, OTA_ADDRESS, OTA_CFG);"

defaults:- {if not set ie. "Update.setupCrypt();" }
  OTA_KEY     = 0  ( 0 = no key, disables decryption )
  OTA_ADDRESS = 0  ( suggest dont set address to app0=0x10000 usually or app1=varies )
  OTA_CFG     = 0xf
  OTA_MODE    = U_AES_DECRYPT_AUTO

OTA_MODE options:-
  U_AES_DECRYPT_NONE       decryption disabled, loads OTA image files as sent(plain)
  U_AES_DECRYPT_AUTO       auto loads both plain & encrypted OTA FLASH image files, and plain OTA SPIFFS image files
  U_AES_DECRYPT_ON         decrypts OTA image files

https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/

Example:
    espsecure.py encrypt_flash_data -k ota_key.bin --flash_crypt_conf 0xf -a 0x4320 -o output_filename.bin source_filename.bin

espsecure.py encrypt_flash_data  = runs the idf encryption function to make a encrypted output file from a source file
  -k text                        = path/filename to the AES 256bit(32byte) encryption key file
  --flash_crypt_conf 0xn         = 0x0 to 0xf, the more bits set the higher the security of encryption(address salting, 0x0 would use ota_key with no address salting)
  -a 0xnnnnnn00                  = 0x00 to 0x00fffff0 address offset(must be a multiple of 16, but better to use multiple of 32), used to offset the salting (has no effect when = --flash_crypt_conf 0x0)
  -o text                        = path/filename to save encrypted output file to
  text                           = path/filename to open source file from
*/

#include <Arduino.h>
#include <WiFi.h>
#include <NetworkClient.h>
#include <HTTPClient.h>
#include <Update.h>

//==========================================================================
//==========================================================================
const char* WIFI_SSID = "wifi-ssid";
const char* WIFI_PASSWORD = "wifi-password";

const uint8_t OTA_KEY[32] = { 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
                              0x38, 0x39, 0x20, 0x74, 0x68, 0x69, 0x73, 0x20,
                              0x61, 0x20, 0x73, 0x69, 0x6d, 0x70, 0x6c, 0x65,
                              0x74, 0x65, 0x73, 0x74, 0x20, 0x6b, 0x65, 0x79 };

/*
const uint8_t  OTA_KEY[32] = {'0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',
                              '8',  '9',  ' ',  't',  'h',  'i',  's',  ' ',
                              'a',  ' ',  's',  'i',  'm',  'p',  'l',  'e',
                              't',  'e',  's',  't',  ' ',  'k',  'e',  'y' };
*/

//const uint8_t  OTA_KEY[33] = "0123456789 this a simpletest key";

const uint32_t OTA_ADDRESS = 0x4320;
const uint32_t OTA_CFG = 0x0f;
const uint32_t OTA_MODE = U_AES_DECRYPT_AUTO;

const char* HTTPUPDATE_USERAGRENT = "ESP32-Updater";
//const char*    HTTPUPDATE_HOST         = "www.yourdomain.com";
const char* HTTPUPDATE_HOST = "192.168.1.2";
const uint16_t HTTPUPDATE_PORT = 80;
const char* HTTPUPDATE_UPDATER_URI = "/firmware/updater.php";                          //uri to 'updater.php'
const char* HTTPUPDATE_DIRECT_URI = "/firmware/HTTP_Client_AES_OTA_Update-v1.1.xbin";  //uri to image file

const char* HTTPUPDATE_USER = NULL;  //use NULL if no authentication needed
//const char*    HTTPUPDATE_USER       = "user";
const char* HTTPUPDATE_PASSWORD = "password";

const char* HTTPUPDATE_BRAND = "21";                         /* Brand ID */
const char* HTTPUPDATE_MODEL = "HTTP_Client_AES_OTA_Update"; /* Project name */
const char* HTTPUPDATE_FIRMWARE = "0.9";                     /* Firmware version */

//==========================================================================
//==========================================================================
String urlEncode(const String& url, const char* safeChars = "-_.~") {
  String encoded = "";
  char temp[4];

  for (int i = 0; i < url.length(); i++) {
    temp[0] = url.charAt(i);
    if (temp[0] == 32) {  //space
      encoded.concat('+');
    } else if ((temp[0] >= 48 && temp[0] <= 57)        /*0-9*/
               || (temp[0] >= 65 && temp[0] <= 90)     /*A-Z*/
               || (temp[0] >= 97 && temp[0] <= 122)    /*a-z*/
               || (strchr(safeChars, temp[0]) != NULL) /* "=&-_.~" */
    ) {
      encoded.concat(temp[0]);
    } else {  //character needs encoding
      snprintf(temp, 4, "%%%02X", temp[0]);
      encoded.concat(temp);
    }
  }
  return encoded;
}

//==========================================================================
bool addQuery(String* query, const String name, const String value) {
  if (name.length() && value.length()) {
    if (query->length() < 3) {
      *query = "?";
    } else {
      query->concat('&');
    }
    query->concat(urlEncode(name));
    query->concat('=');
    query->concat(urlEncode(value));
    return true;
  }
  return false;
}

//==========================================================================
//==========================================================================
void printProgress(size_t progress, const size_t& size) {
  static int last_progress = -1;
  if (size > 0) {
    progress = (progress * 100) / size;
    progress = (progress > 100 ? 100 : progress);  //0-100
    if (progress != last_progress) {
      Serial.printf("Progress: %d%%\n", progress);
      last_progress = progress;
    }
  }
}

//==========================================================================
bool http_downloadUpdate(HTTPClient& http, uint32_t size = 0) {
  size = (size == 0 ? http.getSize() : size);
  if (size == 0) {
    return false;
  }
  NetworkClient* client = http.getStreamPtr();

  if (!Update.begin(size, U_FLASH)) {
    Serial.printf("Update.begin failed! (%s)\n", Update.errorString());
    return false;
  }

  if (!Update.setupCrypt(OTA_KEY, OTA_ADDRESS, OTA_CFG, OTA_MODE)) {
    Serial.println("Update.setupCrypt failed!");
  }

  if (Update.writeStream(*client) != size) {
    Serial.printf("Update.writeStream failed! (%s)\n", Update.errorString());
    return false;
  }

  if (!Update.end()) {
    Serial.printf("Update.end failed! (%s)\n", Update.errorString());
    return false;
  }
  return true;
}

//==========================================================================
int http_sendRequest(HTTPClient& http) {

  //set request Headers to be sent to server
  http.useHTTP10(true);  // use HTTP/1.0 for update since the update handler not support any transfer Encoding
  http.setTimeout(8000);
  http.addHeader("Cache-Control", "no-cache");

  //set own name for HTTPclient user-agent
  http.setUserAgent(HTTPUPDATE_USERAGRENT);

  int code = http.GET();  //send the GET request to HTTP server
  int len = http.getSize();

  if (code == HTTP_CODE_OK) {
    return (len > 0 ? len : 0);  //return 0 or length of image to download
  } else if (code < 0) {
    Serial.printf("Error: %s\n", http.errorToString(code).c_str());
    return code;  //error code should be minus between -1 to -11
  } else {
    Serial.printf("Error: HTTP Server response code %i\n", code);
    return -code;  //return code should be minus between -100 to -511
  }
}

//==========================================================================
/* http_updater sends a GET request to 'update.php' on web server */
bool http_updater(const String& host, const uint16_t& port, String uri, const bool& download, const char* user = NULL, const char* password = NULL) {
  //add GET query params to be sent to server (are used by server 'updater.php' code to determine what action to take)
  String query = "";
  addQuery(&query, "cmd", (download ? "download" : "check"));  //action command

  //setup HTTPclient to be ready to connect & send a request to HTTP server
  HTTPClient http;
  NetworkClient client;
  uri.concat(query);  //GET query added to end of uri path
  if (!http.begin(client, host, port, uri)) {
    return false;  //httpclient setup error
  }
  Serial.printf("Sending HTTP request 'http://%s:%i%s'\n", host.c_str(), port, uri.c_str());

  //set basic authorization, if needed for webpage access
  if (user != NULL && password != NULL) {
    http.setAuthorization(user, password);  //set basic Authorization to server, if needed be gain access
  }

  //add unique Headers to be sent to server used by server 'update.php' code to determine there a suitable firmware update image available
  http.addHeader("Brand-Code", HTTPUPDATE_BRAND);
  http.addHeader("Model", HTTPUPDATE_MODEL);
  http.addHeader("Firmware", HTTPUPDATE_FIRMWARE);

  //set headers to look for to get returned values in servers http response to our http request
  const char* headerkeys[] = { "update", "version" };  //server returns update 0=no update found, 1=update found, version=version of update found
  size_t headerkeyssize = sizeof(headerkeys) / sizeof(char*);
  http.collectHeaders(headerkeys, headerkeyssize);

  //connect & send HTTP request to server
  int size = http_sendRequest(http);

  //is there an image to download
  if (size > 0 || (!download && size == 0)) {
    if (!http.header("update") || http.header("update").toInt() == 0) {
      Serial.println("No Firmware available");
    } else if (!http.header("version") || http.header("version").toFloat() <= String(HTTPUPDATE_FIRMWARE).toFloat()) {
      Serial.println("Firmware is upto Date");
    } else {
      //image avaliabe to download & update
      if (!download) {
        Serial.printf("Found V%s Firmware\n", http.header("version").c_str());
      } else {
        Serial.printf("Downloading & Installing V%s Firmware\n", http.header("version").c_str());
      }
      if (!download || http_downloadUpdate(http)) {
        http.end();  //end connection
        return true;
      }
    }
  }

  http.end();  //end connection
  return false;
}

//==========================================================================
/* this downloads Firmware image file directly from web server */
bool http_direct(const String& host, const uint16_t& port, const String& uri, const char* user = NULL, const char* password = NULL) {
  //setup HTTPclient to be ready to connect & send a request to HTTP server
  HTTPClient http;
  NetworkClient client;
  if (!http.begin(client, host, port, uri)) {
    return false;  //httpclient setup error
  }
  Serial.printf("Sending HTTP request 'http://%s:%i%s'\n", host.c_str(), port, uri.c_str());

  //set basic authorization, if needed for webpage access
  if (user != NULL && password != NULL) {
    http.setAuthorization(user, password);  //set basic Authorization to server, if needed be gain access
  }

  //connect & send HTTP request to server
  int size = http_sendRequest(http);

  //is there an image to download
  if (size > 0) {
    if (http_downloadUpdate(http)) {
      http.end();
      return true;  //end connection
    }
  } else {
    Serial.println("Image File not found");
  }

  http.end();  //end connection
  return false;
}

//==========================================================================
//==========================================================================

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.printf("Booting %s V%s\n", HTTPUPDATE_MODEL, HTTPUPDATE_FIRMWARE);

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi failed, retrying.");
  }
  int i = 0;
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.print(".");
    if ((++i % 100) == 0) {
      Serial.println();
    }
    delay(100);
  }
  Serial.printf("Connected to Wifi\nLocal IP: %s\n", WiFi.localIP().toString().c_str());

  Update.onProgress(printProgress);

  Serial.println("Checking with Server, if New Firmware available");
  if (http_updater(HTTPUPDATE_HOST, HTTPUPDATE_PORT, HTTPUPDATE_UPDATER_URI, 0, HTTPUPDATE_USER, HTTPUPDATE_PASSWORD)) {    //check for new firmware
    if (http_updater(HTTPUPDATE_HOST, HTTPUPDATE_PORT, HTTPUPDATE_UPDATER_URI, 1, HTTPUPDATE_USER, HTTPUPDATE_PASSWORD)) {  //update to new firmware
      Serial.println("Firmware Update Successful, rebooting");
      ESP.restart();
    }
  }

  Serial.println("Checking Server for Firmware Image File to Download & Install");
  if (http_direct(HTTPUPDATE_HOST, HTTPUPDATE_PORT, HTTPUPDATE_DIRECT_URI, HTTPUPDATE_USER, HTTPUPDATE_PASSWORD)) {
    Serial.println("Firmware Update Successful, rebooting");
    ESP.restart();
  }
}

void loop() {
}
