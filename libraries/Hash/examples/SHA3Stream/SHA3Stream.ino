/*
  Usage example for the SHA3Builder class with streams.

  This example shows how to use the SHA3 library to hash data from streams using the addStream method.
  This is useful for hashing large files or data that comes from various stream sources like:
  - File streams
  - Network streams
  - Memory streams
  - Custom stream implementations

  Available constructors:
  - SHA3_224Builder(): 224-bit hash output
  - SHA3_256Builder(): 256-bit hash output
  - SHA3_384Builder(): 384-bit hash output
  - SHA3_512Builder(): 512-bit hash output
  - SHA3Builder(size_t hash_size): Generic class that can be used to create any SHA3 variant implemented
*/

#include <SHA3Builder.h>
#include <Stream.h>

// Expected hash values for validation
const char *EXPECTED_STREAM_TEST_SHA3_256 = "7094efc774885c7a785b408c5da86636cb8adc79156c0f162c6fd7e49f4c505e";
const char *EXPECTED_MAX_SHA3_224_FULL = "ad0e69e04a7258d7cab4272a08ac69f8b43f4e45f9c49c9abb0628af";
const char *EXPECTED_MAX_SHA3_224_10 = "9b55096e998cda6b96d3f2828c4ccda8c9964a1ad98989fb8b0fcd26";
const char *EXPECTED_COMBINED_SHA3_256 = "4a32307fe03bf9f600c5d124419985fd4d42c1639e6a23ab044f107c3b95a189";

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

// Custom stream class for demonstration
class TestStream : public Stream {
private:
  String data;
  size_t position;

public:
  TestStream(String input_data) : data(input_data), position(0) {}

  virtual int available() override {
    return data.length() - position;
  }

  virtual int read() override {
    if (position < data.length()) {
      return data.charAt(position++);
    }
    return -1;
  }

  virtual int peek() override {
    if (position < data.length()) {
      return data.charAt(position);
    }
    return -1;
  }

  virtual size_t write(uint8_t) override {
    return 0;  // Read-only stream
  }

  size_t length() {
    return data.length();
  }

  void reset() {
    position = 0;
  }
};

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("\n\nSHA3 Stream Example");
  Serial.println("===================");

  // Example 1: Using addStream with a custom stream
  {
    Serial.println("\n1. Hashing data from a custom stream:");

    const char *test_data = "This is a test message for streaming hash calculation. "
                            "It contains multiple sentences to demonstrate how the "
                            "addStream method processes data in chunks.";

    TestStream stream(test_data);

    SHA3_256Builder sha3_256;
    sha3_256.begin();

    // Hash the entire stream
    // First argument is the stream, second argument is the maximum length to be read from the stream
    sha3_256.addStream(stream, stream.length());  // Reading the entire stream
    sha3_256.calculate();
    String hash_256 = sha3_256.toString();
    validateHash(hash_256, EXPECTED_STREAM_TEST_SHA3_256, "Stream test validation");
  }

  // Example 2: Using addStream with different maximum lengths
  {
    Serial.println("\n2. Comparing different maximum lengths with streams:");

    const char *test_data = "Streaming hash test with different maximum lengths";
    TestStream stream(test_data);

    // SHA3-224 with a hardcoded maximum length
    stream.reset();
    SHA3_224Builder sha3_224_10;
    sha3_224_10.begin();
    sha3_224_10.addStream(stream, 10);  // Passing a hardcoded maximum length to be read from the stream
    sha3_224_10.calculate();
    String hash_224_10 = sha3_224_10.toString();
    validateHash(hash_224_10, EXPECTED_MAX_SHA3_224_10, "SHA3-224 with 10 bytes");

    // SHA3-224 with the full stream
    stream.reset();
    SHA3_224Builder sha3_224_full;
    sha3_224_full.begin();
    sha3_224_full.addStream(stream, stream.length());  // Reading the entire stream
    sha3_224_full.calculate();
    String hash_224_full = sha3_224_full.toString();
    validateHash(hash_224_full, EXPECTED_MAX_SHA3_224_FULL, "SHA3-224 with full stream");
  }

  // Example 3: Combining add() and addStream()
  {
    Serial.println("\n3. Combining add() and addStream():");

    const char *stream_data = "Additional data from stream";
    TestStream stream(stream_data);

    SHA3_256Builder sha3_256;
    sha3_256.begin();

    // Add some data directly
    sha3_256.add("Initial data: ");

    // Add data from stream
    sha3_256.addStream(stream, stream.length());

    // Add more data directly
    sha3_256.add(" : Final data");

    sha3_256.calculate();
    String hash_256 = sha3_256.toString();
    validateHash(hash_256, EXPECTED_COMBINED_SHA3_256, "Combined data validation");
  }

  Serial.println("\nStream example completed!");
}

void loop() {
  // Nothing to do in loop
}
