/*
  Usage example for the SHA3Builder class.

  This example shows how to use the SHA3 library to hash data using the SHA3Builder class.
  SHA3 (Secure Hash Algorithm 3) provides different output sizes: SHA3-224, SHA3-256, SHA3-384, and SHA3-512.

  Available constructors:
  - SHA3_224Builder(): 224-bit hash output
  - SHA3_256Builder(): 256-bit hash output
  - SHA3_384Builder(): 384-bit hash output
  - SHA3_512Builder(): 512-bit hash output
  - SHA3Builder(size_t hash_size): Generic class that can be used to create any SHA3 variant implemented
*/

#include <SHA3Builder.h>

// Expected hash values for validation
const char *EXPECTED_HELLO_WORLD_SHA3_256 = "e167f68d6563d75bb25f3aa49c29ef612d41352dc00606de7cbd630bb2665f51";
const char *EXPECTED_HELLO_WORLD_SHA3_512 =
  "3d58a719c6866b0214f96b0a67b37e51a91e233ce0be126a08f35fdf4c043c6126f40139bfbc338d44eb2a03de9f7bb8eff0ac260b3629811e389a5fbee8a894";
const char *EXPECTED_TEST_MESSAGE_SHA3_224 = "27af391bcb3b86f21b73c42c4abbde4791c395dc650243eede85de0c";
const char *EXPECTED_TEST_MESSAGE_SHA3_384 = "adb18f6b164672c566950bfefa48c5a851d48ee184f249a19e723d753b7536fcd048c3443aff7ebe433fce63c81726ea";

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

  // Using SHA3Builder class directly with different hash sizes
  {
    String test_data = "Hello World";
    Serial.println("Test data: " + test_data);

    // Create SHA3-256 (default hash size)
    SHA3Builder sha3_256;
    sha3_256.begin();
    sha3_256.add(test_data);
    sha3_256.calculate();
    String hash_256 = sha3_256.toString();
    validateHash(hash_256, EXPECTED_HELLO_WORLD_SHA3_256, "SHA3-256 validation");

    // Create SHA3-512
    SHA3Builder sha3_512(SHA3_512_HASH_SIZE);
    sha3_512.begin();
    sha3_512.add(test_data);
    sha3_512.calculate();
    String hash_512 = sha3_512.toString();
    validateHash(hash_512, EXPECTED_HELLO_WORLD_SHA3_512, "SHA3-512 validation");
  }

  // Example using SHA3_224Builder and SHA3_384Builder
  // There are other constructors for other hash sizes available:
  // - SHA3_224Builder()
  // - SHA3_256Builder()
  // - SHA3_384Builder()
  // - SHA3_512Builder()
  // - SHA3Builder(size_t hash_size)
  {
    String test_data = "Test message";
    Serial.println("Test data: " + test_data);

    // Create SHA3-224 using specific constructor
    SHA3_224Builder sha3_224;
    sha3_224.begin();
    sha3_224.add(test_data);
    sha3_224.calculate();
    String hash_224 = sha3_224.toString();
    validateHash(hash_224, EXPECTED_TEST_MESSAGE_SHA3_224, "SHA3_224Builder validation");

    // Create SHA3-384 using specific constructor
    SHA3_384Builder sha3_384;
    sha3_384.begin();
    sha3_384.add(test_data);
    sha3_384.calculate();
    String hash_384 = sha3_384.toString();
    validateHash(hash_384, EXPECTED_TEST_MESSAGE_SHA3_384, "SHA3_384Builder validation");
  }

  Serial.println("Done.");
}

void loop() {}
