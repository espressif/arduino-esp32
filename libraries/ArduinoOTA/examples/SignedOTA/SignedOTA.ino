/*
 * SignedOTA Example - Secure OTA Updates with Signature Verification
 *
 * This example demonstrates how to perform OTA updates with cryptographic
 * signature verification using ArduinoOTA library.
 *
 * IMPORTANT: This example requires firmware to be signed with bin_signing.py
 *
 * NOTE: Signature verification support is enabled via the build_opt.h file
 *       in this directory.
 *
 * Setup:
 * 1. Generate keys:
 *    python <ARDUINO_ROOT>/tools/bin_signing.py --generate-key rsa-2048 --out private_key.pem
 *    python <ARDUINO_ROOT>/tools/bin_signing.py --extract-pubkey private_key.pem --out public_key.pem
 *
 * 2. Copy public_key.h to this sketch directory
 *
 * 3. Configure WiFi credentials below
 *
 * 4. Upload this sketch to your device
 *
 * 5. Build your firmware and sign it:
 *    arduino-cli compile --fqbn esp32:esp32:esp32 --export-binaries SignedOTA
 *    python <ARDUINO_ROOT>/tools/bin_signing.py --bin build/<file>.bin --key private_key.pem --out firmware_signed.bin
 *
 * 6. Upload signed firmware using espota.py or Arduino IDE (after modifying espota.py to handle signed binaries)
 *    python <ARDUINO_ROOT>/tools/espota.py -i <device-ip> -f firmware_signed.bin
 *
 * For more information, see the Update library's Signed_OTA_Update example
 * and README.md in the Update library folder.
 *
 * Created by lucasssvaz
 */

#include <WiFi.h>
#include <ESPmDNS.h>
#include <NetworkUdp.h>
#include <ArduinoOTA.h>

// Include your public key (generated with bin_signing.py)
#include "public_key.h"

// ==================== CONFIGURATION ====================

// WiFi credentials
const char *ssid = "..........";
const char *password = "..........";

// Optional: Set a password for OTA authentication
// This is in ADDITION to signature verification
// ArduinoOTA password protects the OTA connection
// Signature verification ensures firmware authenticity
const char *ota_password = nullptr;  // Set to nullptr to disable, or "yourpassword" to enable

// Choose hash algorithm (must match what you use with bin_signing.py --hash)
// Uncomment ONE of these:
#define USE_SHA256  // Default, recommended
// #define USE_SHA384
// #define USE_SHA512

// Choose signature algorithm (must match your key type)
// Uncomment ONE of these:
#define USE_RSA  // Recommended (works with rsa-2048, rsa-3072, rsa-4096)
// #define USE_ECDSA  // Works with ecdsa-p256, ecdsa-p384

// =======================================================

uint32_t last_ota_time = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n=================================");
  Serial.println("SignedOTA - Secure OTA Updates");
  Serial.println("=================================\n");
  Serial.println("Booting...");

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  Serial.println("WiFi Connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // ==================== SIGNATURE VERIFICATION SETUP ====================

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
  static UpdaterRSAVerifier sign(PUBLIC_KEY, PUBLIC_KEY_LEN, hashType);
  Serial.println("Using RSA signature verification");
#elif defined(USE_ECDSA)
  static UpdaterECDSAVerifier sign(PUBLIC_KEY, PUBLIC_KEY_LEN, hashType);
  Serial.println("Using ECDSA signature verification");
#else
#error "Please define a signature type (USE_RSA or USE_ECDSA)"
#endif

  // Install signature verification BEFORE ArduinoOTA.begin()
  ArduinoOTA.setSignature(&sign);
  Serial.println("✓ Signature verification enabled");

  // =======================================================================

  // Optional: Set hostname
  // ArduinoOTA.setHostname("myesp32");

  // Optional: Set OTA password (in addition to signature verification)
  if (ota_password != nullptr) {
    ArduinoOTA.setPassword(ota_password);
    Serial.println("✓ OTA password protection enabled");
  }

  // Configure OTA callbacks
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_SPIFFS
        type = "filesystem";
      }
      Serial.println("\n=================================");
      Serial.println("OTA Update Starting: " + type);
      Serial.println("=================================");
      Serial.println("⚠️  Signature will be verified!");
    })
    .onEnd([]() {
      Serial.println("\n=================================");
      Serial.println("✅ OTA Update Complete!");
      Serial.println("✅ Signature Verified!");
      Serial.println("=================================");
      Serial.println("Rebooting...");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      if (millis() - last_ota_time > 500) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
        last_ota_time = millis();
      }
    })
    .onError([](ota_error_t error) {
      Serial.println("\n=================================");
      Serial.println("❌ OTA Update Failed!");
      Serial.println("=================================");
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Authentication Failed");
        Serial.println("Check your OTA password");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
        Serial.println("This could be:");
        Serial.println("- Signature verification setup failed");
        Serial.println("- Not enough space for update");
        Serial.println("- Invalid partition");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
        Serial.println("This could be:");
        Serial.println("- ❌ SIGNATURE VERIFICATION FAILED!");
        Serial.println("- Firmware not signed with correct key");
        Serial.println("- Firmware corrupted during transfer");
        Serial.println("- MD5 checksum mismatch");
      }
      Serial.println("=================================");
    });

  // Start ArduinoOTA service
  ArduinoOTA.begin();

  Serial.println("\n=================================");
  Serial.println("✓ OTA Server Ready");
  Serial.println("=================================");
  Serial.printf("Hostname: %s.local\n", ArduinoOTA.getHostname().c_str());
  Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.println("Port: 3232");
  Serial.println("\n⚠️  Only signed firmware will be accepted!");
  Serial.println("=================================\n");
}

void loop() {
  ArduinoOTA.handle();
}
