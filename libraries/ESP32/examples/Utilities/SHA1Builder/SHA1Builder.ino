#include <SHA1Builder.h>

// Occasionally it is useful to compare a password that the user
// has entered to a build in string. However this means that the
// password ends up `in the clear' in the firmware and in your
// source code.
//
// SHA1Builder helps you obfuscate this (This is not much more secure.
// SHA1 is past its retirement age and long obsolete/insecure, but it helps
// a little) by letting you create an (unsalted!) SHA1 of the data the
// user entered; and then compare this to an SHA1 string that you have put
// in your code.

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  Serial.println("\n\n\nStart.");

  // Check if a password obfuscated in an SHA1 actually
  // matches the original string.
  //
  // echo -n "Hello World" | openssl sha1
  {
    String sha1_str = "0a4d55a8d778e5022fab701977c5d840bbc486d0";
    String password = "Hello World";

    SHA1Builder sha;

    sha.begin();
    sha.add(password);
    sha.calculate();

    String result = sha.toString();

    if (!sha1_str.equalsIgnoreCase(result)) {
      Serial.println("Odd - failing SHA1 on String");
    } else {
      Serial.println("OK!");
    }
  }

  // Check that this also work if we add the password not as
  // a normal string - but as a string with the HEX values.
  {
    String passwordAsHex = "48656c6c6f20576f726c64";
    String sha1_str = "0a4d55a8d778e5022fab701977c5d840bbc486d0";

    SHA1Builder sha;

    sha.begin();
    sha.addHexString(passwordAsHex);
    sha.calculate();

    String result = sha.toString();

    if (!sha1_str.equalsIgnoreCase(result)) {
      Serial.println("Odd - failing SHA1 on hex string");
      Serial.println(sha1_str);
      Serial.println(result);
    } else {
      Serial.println("OK!");
    }
  }

  // Check that this also work if we add the password as
  // an unsigned byte array.
  {
    uint8_t password[] = {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x57, 0x6f, 0x72, 0x6c, 0x64};
    String sha1_str = "0a4d55a8d778e5022fab701977c5d840bbc486d0";
    SHA1Builder sha;

    sha.begin();
    sha.add(password, sizeof(password));
    sha.calculate();

    String result = sha.toString();

    if (!sha1_str.equalsIgnoreCase(result)) {
      Serial.println("Odd - failing SHA1 on byte array");
    } else {
      Serial.println("OK!");
    }

    // And also check that we can compare this as pure, raw, bytes
    uint8_t raw[20] = {0x0a, 0x4d, 0x55, 0xa8, 0xd7, 0x78, 0xe5, 0x02, 0x2f, 0xab, 0x70, 0x19, 0x77, 0xc5, 0xd8, 0x40, 0xbb, 0xc4, 0x86, 0xd0};
    uint8_t res[20];
    sha.getBytes(res);
    if (memcmp(raw, res, 20)) {
      Serial.println("Odd - failing SHA1 on byte array when compared as bytes");
    } else {
      Serial.println("OK!");
    }
  }
}

void loop() {}
