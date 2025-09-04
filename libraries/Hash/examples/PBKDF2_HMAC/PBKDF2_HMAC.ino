/*
  Usage example for the PBKDF2_HMACBuilder class.

  This example shows how to use the Hash library to hash data using the PBKDF2_HMACBuilder class.
  PBKDF2_HMAC (Password-Based Key Derivation Function 2) is a key derivation function that uses a password and a salt to derive a key.

  The PBKDF2_HMACBuilder class takes for arguments:
  - A HashBuilder object to use for the HMAC (SHA1Builder, SHA2Builder, SHA3Builder, etc.)
  - A password string (default: empty)
  - A salt string (default: empty)
  - The number of iterations (default: 1000)
*/

#include <SHA1Builder.h>
#include <SHA2Builder.h>
#include <PBKDF2_HMACBuilder.h>

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nPBKDF2-HMAC Example");
  Serial.println("===================");

  // Test 1: Basic PBKDF2-HMAC-SHA1
  Serial.println("\n1. PBKDF2-HMAC-SHA1 Test (1 iteration)");
  {
    SHA1Builder sha1;
    PBKDF2_HMACBuilder pbkdf2(&sha1, "password", "salt", 1);

    pbkdf2.begin();
    pbkdf2.calculate();

    Serial.print("Password: ");
    Serial.println("password");
    Serial.print("Salt: ");
    Serial.println("salt");
    Serial.print("Iterations: ");
    Serial.println(1);
    Serial.print("Output (hex): ");
    Serial.println(pbkdf2.toString());

    // Expected: 0c60c80f961f0e71f3a9b524af6012062fe037a6
    String expected = "0c60c80f961f0e71f3a9b524af6012062fe037a6";
    String result = pbkdf2.toString();

    if (result.equalsIgnoreCase(expected)) {
      Serial.println("✓ PASS: Output matches expected value");
    } else {
      Serial.println("✗ FAIL: Output does not match expected value");
      Serial.print("Expected: ");
      Serial.println(expected);
      Serial.print("Got:      ");
      Serial.println(result);
    }
  }

  // Test 2: PBKDF2-HMAC-SHA1 with more iterations
  Serial.println("\n2. PBKDF2-HMAC-SHA1 Test (1000 iterations)");
  {
    SHA1Builder sha1;
    PBKDF2_HMACBuilder pbkdf2(&sha1);

    const char *password = "password";
    const char *salt = "salt";

    pbkdf2.begin();
    pbkdf2.setPassword(password);
    pbkdf2.setSalt(salt);
    pbkdf2.setIterations(1000);
    pbkdf2.calculate();

    Serial.print("Password: ");
    Serial.println(password);
    Serial.print("Salt: ");
    Serial.println(salt);
    Serial.print("Iterations: ");
    Serial.println(1000);
    Serial.print("Output (hex): ");
    Serial.println(pbkdf2.toString());

    // Expected: 6e88be8bad7eae9d9e10aa061224034fed48d03f
    String expected = "6e88be8bad7eae9d9e10aa061224034fed48d03f";
    String result = pbkdf2.toString();

    if (result.equalsIgnoreCase(expected)) {
      Serial.println("✓ PASS: Output matches expected value");
    } else {
      Serial.println("✗ FAIL: Output does not match expected value");
      Serial.print("Expected: ");
      Serial.println(expected);
      Serial.print("Got:      ");
      Serial.println(result);
    }
  }

  // Test 3: PBKDF2-HMAC-SHA256 with different password and salt
  Serial.println("\n3. PBKDF2-HMAC-SHA256 Test");
  {
    SHA256Builder sha256;
    PBKDF2_HMACBuilder pbkdf2(&sha256, "mySecretPassword", "randomSalt123", 100);

    pbkdf2.begin();
    pbkdf2.calculate();

    Serial.print("Password: ");
    Serial.println("mySecretPassword");
    Serial.print("Salt: ");
    Serial.println("randomSalt123");
    Serial.print("Iterations: ");
    Serial.println(100);
    Serial.print("Output (hex): ");
    Serial.println(pbkdf2.toString());

    // Expected: 4ce309e56a37e0a4b9b84b98ed4a94e6c5cd5926cfd3baca3a6dea8c5d7903e8
    String expected = "4ce309e56a37e0a4b9b84b98ed4a94e6c5cd5926cfd3baca3a6dea8c5d7903e8";
    String result = pbkdf2.toString();

    if (result.equalsIgnoreCase(expected)) {
      Serial.println("✓ PASS: Output matches expected value");
    } else {
      Serial.println("✗ FAIL: Output does not match expected value");
      Serial.print("Expected: ");
      Serial.println(expected);
      Serial.print("Got:      ");
      Serial.println(result);
    }
  }

  // Test 4: PBKDF2-HMAC-SHA1 with byte arrays
  Serial.println("\n4. PBKDF2-HMAC-SHA1 Test (byte arrays)");
  {
    SHA1Builder sha1;  // or any other hash algorithm based on HashBuilder
    PBKDF2_HMACBuilder pbkdf2(&sha1);

    uint8_t password[] = {0x70, 0x61, 0x73, 0x73, 0x77, 0x6f, 0x72, 0x64};  // "password" in bytes
    uint8_t salt[] = {0x73, 0x61, 0x6c, 0x74};                              // "salt" in bytes

    pbkdf2.begin();
    pbkdf2.setPassword(password, sizeof(password));
    pbkdf2.setSalt(salt, sizeof(salt));
    pbkdf2.setIterations(1);
    pbkdf2.calculate();

    Serial.print("Password (bytes): ");
    for (int i = 0; i < sizeof(password); i++) {
      Serial.print((char)password[i]);
    }
    Serial.println();
    Serial.print("Salt (bytes): ");
    for (int i = 0; i < sizeof(salt); i++) {
      Serial.print((char)salt[i]);
    }
    Serial.println();
    Serial.print("Iterations: ");
    Serial.println(1);
    Serial.print("Output (hex): ");
    Serial.println(pbkdf2.toString());

    // Expected: 0c60c80f961f0e71f3a9b524af6012062fe037a6 (same as test 1)
    String expected = "0c60c80f961f0e71f3a9b524af6012062fe037a6";
    String result = pbkdf2.toString();

    if (result.equalsIgnoreCase(expected)) {
      Serial.println("✓ PASS: Output matches expected value");
    } else {
      Serial.println("✗ FAIL: Output does not match expected value");
      Serial.print("Expected: ");
      Serial.println(expected);
      Serial.print("Got:      ");
      Serial.println(result);
    }
  }

  Serial.println("\nPBKDF2-HMAC tests completed!");
}

void loop() {
  // Nothing to do in loop
}
