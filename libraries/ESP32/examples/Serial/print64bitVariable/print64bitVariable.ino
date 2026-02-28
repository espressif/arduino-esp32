/*
  Print 64-bit variables using StringUtils.h

  This example shows how to print 64-bit variables using StringUtils.h.
  This is required to maintain compatibility with NEWLIB-nano and PICOLIBC.

  You can use the following functions to print 64-bit variables:
  - buffer = u64_to_str(value, buffer)
  - buffer = i64_to_str(value, buffer)
  - String = u64_to_String(value)
  - String = i64_to_String(value)
  - Serial.print(value)
  - Serial.println(value)

  Note: printf() does not support 64-bit variables in NEWLIB-nano.

*/

#include <Arduino.h>
#include <StringUtils.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  Serial.println("Printing 64-bit variables using StringUtils.h");
  Serial.println("---------------------------------------------");
  uint64_t big_uint64 = UINT64_MAX;
  int64_t negative_big_int64 = INT64_MIN;

  char positive_buffer[21]; // At least 21 bytes are required for uint64_t
  char negative_buffer[22]; // At least 22 bytes are required for int64_t

  u64_to_str(big_uint64, positive_buffer);
  i64_to_str(negative_big_int64, negative_buffer);

  Serial.printf("Using char* to print uint64_t: %s\n", positive_buffer);
  Serial.printf("Using char* to print int64_t: %s\n", negative_buffer);

  Serial.println("---------------------------------------------");

  String positive_string = u64_to_String(big_uint64);
  String negative_string = i64_to_String(negative_big_int64);

  Serial.printf("Using String to print uint64_t: %s\n", positive_string.c_str());
  Serial.printf("Using String to print int64_t: %s\n", negative_string.c_str());

  Serial.println("---------------------------------------------");

  Serial.print("Using Serial.print to print uint64_t: ");
  Serial.println(big_uint64);
  Serial.print("Using Serial.print to print int64_t: ");
  Serial.println(negative_big_int64);

  Serial.println("---------------------------------------------");
}

void loop() {
  delay(1000);
}
