/*
 * Validation harness for signed OTA (RSA-PSS / ECDSA-DER).
 *
 * All keys are generated at test runtime by the pytest harness.
 * The public key PEM for each case is received over serial — nothing is hardcoded.
 *
 * Serial protocol (per case, after WiFi + server base setup):
 *   DUT  -> "READY_FOR_CASE"
 *   Host -> case_number          (e.g. "1")
 *   DUT  -> "SEND_KEY_TYPE"
 *   Host -> "RSA" | "ECDSA"
 *   DUT  -> "SEND_HASH_TYPE"
 *   Host -> "SHA256" | "SHA384" | "SHA512"
 *   DUT  -> "SEND_FILENAME"
 *   Host -> filename              (e.g. "fw_case1.bin")
 *   DUT  -> "SEND_KEY"
 *   Host -> PEM lines…
 *   Host -> "KEY_END"
 *   DUT  -> "GOT_KEY len=N"
 *   DUT  -> OTA result + "CASE_END N PASS/FAIL"
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <SHA2Builder.h>

#define MAX_PEM_SIZE 1024

String ssid;
String password;
String serverBase;
static UpdaterVerifyClass *prevVerifier = NULL;

static String waitForLine() {
  String s;
  while (true) {
    if (Serial.available()) {
      s = Serial.readStringUntil('\n');
      s.trim();
      if (s.length() > 0) {
        return s;
      }
    }
    delay(10);
  }
}

static void flushSerial() {
  while (Serial.available()) {
    Serial.read();
  }
}

static void readWifiCredentials() {
  Serial.println("Waiting for WiFi credentials...");
  flushSerial();

  Serial.println("Send SSID:");
  while (ssid.length() == 0) {
    if (Serial.available()) {
      ssid = Serial.readStringUntil('\n');
      ssid.trim();
    }
    delay(100);
  }
  flushSerial();

  Serial.println("Send Password:");
  bool received = false;
  unsigned long timeout = millis() + 10000;
  while (!received && millis() < timeout) {
    if (Serial.available()) {
      password = Serial.readStringUntil('\n');
      password.trim();
      received = true;
    }
    delay(100);
  }
  flushSerial();

  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("Password: ");
  Serial.println(password);
}

static void readServerBase() {
  Serial.println("Send HTTP server base URL (e.g. http://192.168.1.10:8765):");
  serverBase = "";
  while (serverBase.length() == 0) {
    if (Serial.available()) {
      serverBase = Serial.readStringUntil('\n');
      serverBase.trim();
    }
    delay(100);
  }
  flushSerial();
  Serial.print("Server base: ");
  Serial.println(serverBase);
}

static void doOTA(int caseNum, bool isRSA, int hashType, const String &url, uint8_t *pemKey, size_t pemKeyLen) {
  Update.abort();

  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("HTTP GET failed: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    Serial.printf("CASE_END %d FAIL\n", caseNum);
    return;
  }

  int contentLength = http.getSize();
  if (contentLength <= 512) {
    Serial.println("Content too small");
    http.end();
    Serial.printf("CASE_END %d FAIL\n", caseNum);
    return;
  }

  Serial.printf("Firmware size: %d bytes\n", contentLength);
  Serial.printf("Actual firmware size: %lu bytes\n", (unsigned long)(contentLength - 512));

  delete prevVerifier;
  prevVerifier = NULL;

  UpdaterVerifyClass *verifier;
  if (isRSA) {
    verifier = new UpdaterRSAVerifier(pemKey, pemKeyLen, hashType);
    Serial.println("Using RSA signature verification");
  } else {
    verifier = new UpdaterECDSAVerifier(pemKey, pemKeyLen, hashType);
    Serial.println("Using ECDSA signature verification");
  }
  prevVerifier = verifier;

  bool pass = false;

  do {
    if (!Update.installSignature(verifier)) {
      Serial.println("Failed to install signature verification");
      break;
    }
    Serial.println("Signature verification installed");

    if (!Update.begin(contentLength)) {
      Serial.printf("Update.begin failed: %s\n", Update.errorString());
      break;
    }

    WiFiClient *stream = http.getStreamPtr();
    Serial.println("Writing firmware...");
    size_t written = 0;
    uint8_t buff[1024];
    bool writeOk = true;

    while (http.connected() && written < (size_t)contentLength) {
      size_t avail = stream->available();
      if (avail) {
        int nr = stream->readBytes(buff, min(avail, sizeof(buff)));
        if (nr > 0) {
          size_t nw = Update.write(buff, nr);
          if (nw > 0) {
            written += nw;
          } else {
            Serial.printf("Update.write failed: %s\n", Update.errorString());
            writeOk = false;
            break;
          }
        }
      }
      delay(1);
    }

    if (!writeOk) {
      break;
    }
    Serial.printf("Written: %lu bytes\n", (unsigned long)written);

    if (Update.end()) {
      Serial.println(isRSA ? "RSA signature verified successfully" : "ECDSA signature verified successfully");
      Serial.println("OTA update completed successfully!");
      pass = true;
    } else {
      Serial.printf("Update.end failed: %s\n", Update.errorString());
      if (Update.getError() == UPDATE_ERROR_SIGN) {
        Serial.println(isRSA ? "RSA signature verification failed" : "ECDSA signature verification failed");
      }
    }
  } while (false);

  Serial.printf("CASE_END %d %s\n", caseNum, pass ? "PASS" : "FAIL");
  http.end();
}

void setup() {
  Serial.setRxBufferSize(2048);
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  Serial.println("Device ready for signed OTA test");

  readWifiCredentials();

  WiFi.disconnect(true);
  delay(100);
  WiFi.begin(ssid.c_str(), password.c_str());

  Serial.println("Connecting to WiFi...");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - start > 120000) {
      Serial.println("\nWiFi connection timeout");
      return;
    }
  }

  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  readServerBase();
}

void loop() {
  Serial.println("READY_FOR_CASE");

  String caseLine = waitForLine();
  int caseNum = caseLine.toInt();
  if (caseNum < 1) {
    Serial.printf("Invalid case: %s\n", caseLine.c_str());
    return;
  }

  Serial.println("SEND_KEY_TYPE");
  String keyType = waitForLine();
  bool isRSA = (keyType == "RSA");
  Serial.printf("Key type: %s\n", keyType.c_str());

  Serial.println("SEND_HASH_TYPE");
  String hashStr = waitForLine();
  int hashType = HASH_SHA256;
  if (hashStr == "SHA384") {
    hashType = HASH_SHA384;
  } else if (hashStr == "SHA512") {
    hashType = HASH_SHA512;
  }
  Serial.printf("Hash: %s\n", hashStr.c_str());

  Serial.println("SEND_FILENAME");
  String filename = waitForLine();
  Serial.printf("File: %s\n", filename.c_str());

  Serial.println("SEND_KEY");
  uint8_t pemBuf[MAX_PEM_SIZE];
  size_t pemLen = 0;
  while (true) {
    if (Serial.available()) {
      String line = Serial.readStringUntil('\n');
      line.trim();
      if (line == "KEY_END") {
        break;
      }
      if (line.length() > 0) {
        size_t ll = line.length();
        if (pemLen + ll + 1 < MAX_PEM_SIZE) {
          memcpy(pemBuf + pemLen, line.c_str(), ll);
          pemLen += ll;
          pemBuf[pemLen++] = '\n';
        }
      }
    }
    delay(10);
  }
  pemBuf[pemLen] = '\0';
  pemLen++;
  Serial.printf("GOT_KEY len=%d\n", (int)pemLen);

  String url = serverBase;
  if (!url.endsWith("/")) {
    url += "/";
  }
  url += filename;

  Serial.printf("Starting OTA case %d URL: %s\n", caseNum, url.c_str());

  doOTA(caseNum, isRSA, hashType, url, pemBuf, pemLen);
  delay(500);
}
