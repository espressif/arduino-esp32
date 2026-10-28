#include <Arduino.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

void test_valid_ipv4_string() {
  IPAddress address("192.0.2.1");

  TEST_ASSERT_EQUAL(IPv4, address.type());
  TEST_ASSERT_EQUAL_UINT8(192, address[0]);
  TEST_ASSERT_EQUAL_UINT8(0, address[1]);
  TEST_ASSERT_EQUAL_UINT8(2, address[2]);
  TEST_ASSERT_EQUAL_UINT8(1, address[3]);
}

void test_unscoped_ipv6_string() {
  IPAddress address("2001:db8::1");
  const IPAddress expected(0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1);

  TEST_ASSERT_EQUAL(IPv6, address.type());
  TEST_ASSERT_TRUE(address == expected);
  TEST_ASSERT_EQUAL_UINT8(0, address.zone());
}

void test_scoped_ipv6_string() {
  IPAddress address("fe80::1%2");
  const IPAddress expected(0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1);

  TEST_ASSERT_EQUAL(IPv6, address.type());
  TEST_ASSERT_TRUE(address == expected);
  TEST_ASSERT_EQUAL_UINT8(3, address.zone());
}

void test_reparse_clears_ipv6_zone() {
  IPAddress address("fe80::1%2");

  TEST_ASSERT_TRUE(address.fromString("fe80::2"));
  TEST_ASSERT_EQUAL(IPv6, address.type());
  TEST_ASSERT_EQUAL_UINT8(0, address.zone());
  TEST_ASSERT_EQUAL_UINT8(2, address[15]);
}

void test_invalid_string_constructor_is_initialized() {
  IPAddress invalid_ipv4("192.0.2.999");
  IPAddress invalid_ipv6("2001:db8::zzzz");

  TEST_ASSERT_EQUAL(IPv4, invalid_ipv4.type());
  TEST_ASSERT_TRUE(invalid_ipv4 == IPAddress(0, 0, 0, 0));
  TEST_ASSERT_EQUAL(IPv4, invalid_ipv6.type());
  TEST_ASSERT_TRUE(invalid_ipv6 == IPAddress(0, 0, 0, 0));
}

void test_failed_parse_preserves_address() {
  IPAddress address("fe80::1%2");
  const IPAddress expected(address);

  TEST_ASSERT_FALSE(address.fromString("2001:db8::zzzz"));
  TEST_ASSERT_EQUAL(IPv6, address.type());
  TEST_ASSERT_TRUE(address == expected);
  TEST_ASSERT_EQUAL_UINT8(3, address.zone());
  TEST_ASSERT_FALSE(address.fromString(static_cast<const char *>(nullptr)));
  TEST_ASSERT_TRUE(address == expected);
  TEST_ASSERT_EQUAL_UINT8(3, address.zone());
}

void setup() {
  Serial.begin(115200);

  UNITY_BEGIN();
  RUN_TEST(test_valid_ipv4_string);
  RUN_TEST(test_unscoped_ipv6_string);
  RUN_TEST(test_scoped_ipv6_string);
  RUN_TEST(test_reparse_clears_ipv6_zone);
  RUN_TEST(test_invalid_string_constructor_is_initialized);
  RUN_TEST(test_failed_parse_preserves_address);
  UNITY_END();
}

void loop() {}
