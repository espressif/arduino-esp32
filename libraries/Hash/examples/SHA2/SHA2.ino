/*
  Usage example for the SHA2Builder class.

  This example shows how to use the SHA2 library to hash data using the SHA2Builder class.
  SHA2 (Secure Hash Algorithm 2) provides different output sizes: SHA-224, SHA-256, SHA-384, and SHA-512.

  Available constructors:
  - SHA224Builder(): 224-bit hash output
  - SHA256Builder(): 256-bit hash output
  - SHA384Builder(): 384-bit hash output
  - SHA512Builder(): 512-bit hash output
  - SHA2Builder(size_t hash_size): Generic class that can be used to create any SHA2 variant implemented
*/

#include <SHA2Builder.h>

// Expected hash values for validation
const char *EXPECTED_HELLO_WORLD_SHA256 = "a591a6d40bf420404a011733cfb7b190d62c65bf0bcda32b57b277d9ad9f146e";
const char *EXPECTED_HELLO_WORLD_SHA512 =
  "2c74fd17edafd80e8447b0d46741ee243b7eb74dd2149a0ab1b9246fb30382f27e853d8585719e0e67cbda0daa8f51671064615d645ae27acb15bfb1447f459b";
const char *EXPECTED_TEST_MESSAGE_SHA224 = "155b033d801d4dd59b783d76ac3059053c00b2c28340a5a36a427a76";
const char *EXPECTED_TEST_MESSAGE_SHA384 = "efd336618cbc96551936e5897e6af391d2480513ff8d4fc744e34462edb3111477d2b889c4d5e80e23b5f9d1b636fbd7";

// Validation function
bool validateHash(const String &calculated, const char *expected, const String &test_name) {
  bool passed = (calculated == expected);
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

  Serial.println("\n\n\nStart.");

  // Using SHA2Builder class directly with different hash sizes
  {
    String test_data = "Hello World";
    Serial.println("Test data: " + test_data);

    // Create SHA-256 (default hash size)
    SHA2Builder sha2_256;
    sha2_256.begin();
    sha2_256.add(test_data);
    sha2_256.calculate();
    String hash_256 = sha2_256.toString();
    validateHash(hash_256, EXPECTED_HELLO_WORLD_SHA256, "SHA-256 validation");

    // Create SHA-512
    SHA2Builder sha2_512(SHA2_512_HASH_SIZE);
    sha2_512.begin();
    sha2_512.add(test_data);
    sha2_512.calculate();
    String hash_512 = sha2_512.toString();
    validateHash(hash_512, EXPECTED_HELLO_WORLD_SHA512, "SHA-512 validation");
  }

  // Example using SHA224Builder and SHA384Builder
  // There are other constructors for other hash sizes available:
  // - SHA224Builder()
  // - SHA256Builder()
  // - SHA384Builder()
  // - SHA512Builder()
  // - SHA2Builder(size_t hash_size)
  {
    String test_data = "Test message";
    Serial.println("Test data: " + test_data);

    // Create SHA-224 using specific constructor
    SHA224Builder sha2_224;
    sha2_224.begin();
    sha2_224.add(test_data);
    sha2_224.calculate();
    String hash_224 = sha2_224.toString();
    validateHash(hash_224, EXPECTED_TEST_MESSAGE_SHA224, "SHA224Builder validation");

    // Create SHA-384 using specific constructor
    SHA384Builder sha2_384;
    sha2_384.begin();
    sha2_384.add(test_data);
    sha2_384.calculate();
    String hash_384 = sha2_384.toString();
    validateHash(hash_384, EXPECTED_TEST_MESSAGE_SHA384, "SHA384Builder validation");
  }

  Serial.println("Done.");
}

void loop() {}
