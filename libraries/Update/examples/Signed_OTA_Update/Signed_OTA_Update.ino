/*
  Signed OTA Update Example

  This example demonstrates how to perform a secure OTA update with signature verification.
  Only firmware signed with the correct private key will be accepted.

  NOTE: This example requires signature verification support to be enabled.
        This is done automatically via the build_opt.h file in this directory.

  Steps to use this example:
  1. Generate a key pair (see instructions below)
  2. Include the public key in this sketch (see public_key.h)
  3. Build and upload this sketch to your ESP32
  4. Build your new firmware binary
  5. Sign the binary with the private key (see instructions below)
  6. Upload the signed firmware via OTA (HTTP/HTTPS server)

  Generating keys:
  ------------------
  RSA (recommended for maximum compatibility):
    python bin_signing.py --generate-key rsa-2048 --out private_key.pem
    python bin_signing.py --extract-pubkey private_key.pem --out public_key.pem

  ECDSA (smaller keys, faster verification):
    python bin_signing.py --generate-key ecdsa-p256 --out private_key.pem
    python bin_signing.py --extract-pubkey private_key.pem --out public_key.pem

  Signing firmware:
  -----------------
    python bin_signing.py --bin firmware.bin --key private_key.pem --out firmware_signed.bin

  IMPORTANT: Keep your private_key.pem secure! Anyone with access to it can
  sign firmware that will be accepted by your devices.

  Created by lucasssvaz
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <SHA2Builder.h>

// WiFi credentials
const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

// URL to the signed firmware binary
const char *firmwareUrl = "http://your-server.com/firmware_signed.bin";

// Public key for signature verification
// Generated with: python bin_signing.py --extract-pubkey private_key.pem --out public_key.pem
// This will create a public_key.h file that you should include below
#include "public_key.h"

// Uncomment the key type you're using:
#define USE_RSA  // RSA signature verification
//#define USE_ECDSA // ECDSA signature verification

// Uncomment the hash algorithm you're using (must match the one used for signing):
#define USE_SHA256  // SHA-256 (recommended and default)
//#define USE_SHA384  // SHA-384
//#define USE_SHA512  // SHA-512

void performOTAUpdate() {
  HTTPClient http;

  Serial.println("Starting OTA update...");
  Serial.print("Firmware URL: ");
  Serial.println(firmwareUrl);

  http.begin(firmwareUrl);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return;
  }

  int contentLength = http.getSize();
  Serial.printf("Firmware size: %d bytes\n", contentLength);

  if (contentLength <= 0) {
    Serial.println("Invalid content length");
    http.end();
    return;
  }

  // The signed firmware includes the signature (512 bytes padding)
  // The actual firmware size is contentLength - 512
  const size_t signatureSize = 512;
  size_t firmwareSize = contentLength - signatureSize;

  Serial.printf("Actual firmware size: %d bytes\n", firmwareSize);
  Serial.printf("Signature size: %d bytes\n", signatureSize);

  // Select hash algorithm
#ifdef USE_SHA256
  int hashType = HASH_SHA256;
  Serial.println("Using SHA-256 hash");
#elif defined(USE_SHA384)
  int hashType = HASH_SHA384;
  Serial.println("Using SHA-384 hash");
#elif defined(USE_SHA512)
  int hashType = HASH_SHA512;
  Serial.println("Using SHA-512 hash");
#else
#error "Please define a hash algorithm (USE_SHA256, USE_SHA384, or USE_SHA512)"
#endif

  // Create verifier object
#ifdef USE_RSA
  UpdaterRSAVerifier sign(PUBLIC_KEY, PUBLIC_KEY_LEN, hashType);
  Serial.println("Using RSA signature verification");
#elif defined(USE_ECDSA)
  UpdaterECDSAVerifier sign(PUBLIC_KEY, PUBLIC_KEY_LEN, hashType);
  Serial.println("Using ECDSA signature verification");
#else
#error "Please define a signature scheme (USE_RSA or USE_ECDSA)"
#endif

  // Install signature verification BEFORE calling Update.begin()
  if (!Update.installSignature(&sign)) {
    Serial.println("Failed to install signature verification");
    http.end();
    return;
  }
  Serial.println("Signature verification installed");

  // Begin update with the TOTAL size (firmware + signature)
  if (!Update.begin(contentLength)) {
    Serial.printf("Update.begin failed: %s\n", Update.errorString());
    http.end();
    return;
  }

  // Get the stream
  WiFiClient *stream = http.getStreamPtr();

  // Write firmware data
  Serial.println("Writing firmware...");
  size_t written = 0;
  uint8_t buff[1024];
  int progress = 0;

  while (http.connected() && (written < contentLength)) {
    size_t available = stream->available();

    if (available) {
      int bytesRead = stream->readBytes(buff, min(available, sizeof(buff)));

      if (bytesRead > 0) {
        size_t bytesWritten = Update.write(buff, bytesRead);

        if (bytesWritten > 0) {
          written += bytesWritten;

          // Print progress
          int newProgress = (written * 100) / contentLength;
          if (newProgress != progress && newProgress % 10 == 0) {
            progress = newProgress;
            Serial.printf("Progress: %d%%\n", progress);
          }
        } else {
          Serial.printf("Update.write failed: %s\n", Update.errorString());
          break;
        }
      }
    }
    delay(1);
  }

  Serial.printf("Written: %d bytes\n", written);

  // End the update - this will verify the signature
  if (Update.end()) {
    Serial.println("OTA update completed successfully!");
    Serial.println("Signature verified!");

    if (Update.isFinished()) {
      Serial.println("Update successfully completed. Rebooting...");
      delay(1000);
      ESP.restart();
    } else {
      Serial.println("Update not finished? Something went wrong!");
    }
  } else {
    Serial.printf("Update.end failed: %s\n", Update.errorString());

    // Check if it was a signature verification failure
    if (Update.getError() == UPDATE_ERROR_SIGN) {
      Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
      Serial.println("SIGNATURE VERIFICATION FAILED!");
      Serial.println("The firmware was not signed with the");
      Serial.println("correct private key or is corrupted.");
      Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    }
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nSigned OTA Update Example");
  Serial.println("=========================\n");

  // Connect to WiFi
  Serial.printf("Connecting to WiFi: %s\n", ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Wait a bit before starting OTA
  delay(2000);

  // Perform OTA update
  performOTAUpdate();
}

void loop() {
  // Nothing to do here
  delay(1000);
}
