/*
  Usage example for the HMACBuilder class.

  HMAC (Hash-based Message Authentication Code) pairs any HashBuilder
  (SHA1Builder, SHA256Builder, SHA512Builder, SHA3_256Builder, ...) with a secret key.

  Available constructors:
  - HMACBuilder(HashBuilder *hash, size_t blockSize = 0)
    blockSize 0 uses hash->getBlockSize() (SHA-1/2/3 all report one).
    Construction leaves the HMAC unusable if the hash does not report a block size.
*/

#include <Arduino.h>
#include <SHA1Builder.h>
#include <SHA2Builder.h>
#include <HMACBuilder.h>

static bool validateHash(const String &calculated, const char *expected, const String &test_name) {
  bool passed = calculated.equalsIgnoreCase(expected);
  Serial.print(test_name);
  Serial.print(": ");
  Serial.println(passed ? "PASS" : "FAIL");
  Serial.print("  Expected: ");
  Serial.println(expected);
  Serial.print("  Got:      ");
  Serial.println(calculated);
  return passed;
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nHMAC Example");
  Serial.println("============");

  // RFC 4231 test case 2: HMAC-SHA-256, key = "Jefe"
  {
    SHA256Builder sha256;
    HMACBuilder hmac(&sha256);
    hmac.setKey("Jefe");
    hmac.begin();
    hmac.add("what do ya want for nothing?");
    hmac.calculate();
    validateHash(hmac.toString(), "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843", "HMAC-SHA-256 (RFC 4231 #2)");
  }

  // RFC 2202 test case 1: HMAC-SHA-1
  {
    const uint8_t key[20] = {0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b};
    SHA1Builder sha1;
    HMACBuilder hmac(&sha1);
    hmac.setKey(key, sizeof(key));
    hmac.begin();
    hmac.add("Hi There");
    hmac.calculate();
    validateHash(hmac.toString(), "b617318655057264e28bc0b6fb378c8ef146be00", "HMAC-SHA-1 (RFC 2202 #1)");
  }
}

void loop() {}
