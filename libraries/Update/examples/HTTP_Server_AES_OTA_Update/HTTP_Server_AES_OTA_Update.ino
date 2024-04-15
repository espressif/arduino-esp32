/*
An example of how to use Update to upload encrypted and plain image files OTA. This example uses a simple webserver & Wifi connection via AP or STA with mDNS and DNS for simple host URI.

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

#include <WiFi.h>
#include <NetworkClient.h>
#include <SPIFFS.h>
#include <Update.h>
#include <WebServer.h>
#include <ESPmDNS.h>

WebServer httpServer(80);

//with WIFI_MODE_AP defined the ESP32 is a wifi AP, with it undefined ESP32 tries to connect to wifi STA
#define WIFI_MODE_AP

#ifdef WIFI_MODE_AP
#include <DNSServer.h>
DNSServer dnsServer;
#endif

const char* host = "esp32-web";
const char* ssid = "wifi-ssid";
const char* password = "wifi-password";

const uint8_t OTA_KEY[32] = { 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
                              0x38, 0x39, 0x20, 0x74, 0x68, 0x69, 0x73, 0x20,
                              0x61, 0x20, 0x73, 0x69, 0x6d, 0x70, 0x6c, 0x65,
                              0x74, 0x65, 0x73, 0x74, 0x20, 0x6b, 0x65, 0x79 };

/*
const uint8_t OTA_KEY[32] = {'0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',
                             '8',  '9',  ' ',  't',  'h',  'i',  's',  ' ',
                             'a',  ' ',  's',  'i',  'm',  'p',  'l',  'e',
                             't',  'e',  's',  't',  ' ',  'k',  'e',  'y' };
*/

//const uint8_t   OTA_KEY[33] = "0123456789 this a simpletest key";

const uint32_t OTA_ADDRESS = 0x4320;  //OTA_ADDRESS value has no effect when OTA_CFG = 0x00
const uint32_t OTA_CFG = 0x0f;
const uint32_t OTA_MODE = U_AES_DECRYPT_AUTO;

/*=================================================================*/
const char* update_path = "update";

static const char UpdatePage_HTML[] PROGMEM =
  R"(<!DOCTYPE html>
     <html lang='en'>
     <head>
         <title>Image Upload</title>
         <meta charset='utf-8'>
         <meta name='viewport' content='width=device-width,initial-scale=1'/>
     </head>
     <body style='background-color:black;color:#ffff66;text-align: center;font-size:20px;'>
     <form method='POST' action='' enctype='multipart/form-data'>
         Firmware:<br><br>
         <input type='file' accept='.bin,.bin.gz' name='firmware' style='font-size:20px;'><br><br>
         <input type='submit' value='Update' style='font-size:25px; height:50px; width:100px'>
     </form>
     <br><br><br>
     <form method='POST' action='' enctype='multipart/form-data'>
         FileSystem:<br><br>
         <input type='file' accept='.bin,.bin.gz,.image' name='filesystem' style='font-size:20px;'><br><br>
         <input type='submit' value='Update' style='font-size:25px; height:50px; width:100px'>
     </form>
     </body>
     </html>)";

/*=================================================================*/

void printProgress(size_t progress, size_t size) {
  static int last_progress = -1;
  if (size > 0) {
    progress = (progress * 100) / size;
    progress = (progress > 100 ? 100 : progress);  //0-100
    if (progress != last_progress) {
      Serial.printf("\nProgress: %d%%", progress);
      last_progress = progress;
    }
  }
}

void setupHttpUpdateServer() {
  //redirecting not found web pages back to update page
  httpServer.onNotFound([&]() {  //webpage not found
    httpServer.sendHeader("Location", String("../") + String(update_path));
    httpServer.send(302, F("text/html"), "");
  });

  // handler for the update web page
  httpServer.on(String("/") + String(update_path), HTTP_GET, [&]() {
    httpServer.send_P(200, PSTR("text/html"), UpdatePage_HTML);
  });

  // handler for the update page form POST
  httpServer.on(
    String("/") + String(update_path), HTTP_POST, [&]() {
      // handler when file upload finishes
      if (Update.hasError()) {
        httpServer.send(200, F("text/html"), String(F("<META http-equiv=\"refresh\" content=\"5;URL=/\">Update error: ")) + String(Update.errorString()));
      } else {
        httpServer.client().setNoDelay(true);
        httpServer.send(200, PSTR("text/html"), String(F("<META http-equiv=\"refresh\" content=\"15;URL=/\">Update Success! Rebooting...")));
        delay(100);
        httpServer.client().stop();
        ESP.restart();
      }
    },
    [&]() {
      // handler for the file upload, gets the sketch bytes, and writes
      // them through the Update object
      HTTPUpload& upload = httpServer.upload();
      if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Update: %s\n", upload.filename.c_str());
        if (upload.name == "filesystem") {
          if (!Update.begin(SPIFFS.totalBytes(), U_SPIFFS)) {  //start with max available size
            Update.printError(Serial);
          }
        } else {
          uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
          if (!Update.begin(maxSketchSpace, U_FLASH)) {  //start with max available size
            Update.printError(Serial);
          }
        }
      } else if (upload.status == UPLOAD_FILE_ABORTED || Update.hasError()) {
        if (upload.status == UPLOAD_FILE_ABORTED) {
          if (!Update.end(false)) {
            Update.printError(Serial);
          }
          Serial.println("Update was aborted");
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        Serial.printf(".");
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {  //true to set the size to the current progress
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
      }
      delay(0);
    });

  Update.onProgress(printProgress);
}

/*=================================================================*/

void setup(void) {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Booting Sketch...");
  WiFi.mode(WIFI_AP_STA);
#ifdef WIFI_MODE_AP
  WiFi.softAP(ssid, password);
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", WiFi.softAPIP());  //if DNS started with "*" for domain name, it will reply with provided IP to all DNS request
  Serial.printf("Wifi AP started, IP address: %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("You can connect to ESP32 AP use:-\n    ssid: %s\npassword: %s\n\n", ssid, password);
#else
  WiFi.begin(ssid, password);
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
#endif

  if (MDNS.begin(host)) {
    Serial.println("mDNS responder started");
  }

  setupHttpUpdateServer();

  if (Update.setupCrypt(OTA_KEY, OTA_ADDRESS, OTA_CFG, OTA_MODE)) {
    Serial.println("Upload Decryption Ready");
  }

  httpServer.begin();

  MDNS.addService("http", "tcp", 80);
#ifdef WIFI_MODE_AP
  Serial.printf("HTTPUpdateServer ready with Captive DNS!\nOpen http://anyname.xyz/%s in your browser\n", update_path);
#else
  Serial.printf("HTTPUpdateServer ready!\nOpen http://%s.local/%s in your browser\n", host, update_path);
#endif
}

void loop(void) {
  httpServer.handleClient();
#ifdef WIFI_MODE_AP
  dnsServer.processNextRequest();  //DNS captive portal for easy access to this device webserver
#endif
}
