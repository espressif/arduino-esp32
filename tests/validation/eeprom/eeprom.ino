/*
 * EEPROM Validation Test
 *
 * Covers: begin/end lifecycle, basic byte read/write, isDirty flag,
 * commit, all typed write/read helpers (Byte, Char, UChar, Short, UShort,
 * Int, UInt, Long, ULong, Long64, ULong64, Float, Double, Bool),
 * writeString/readString (char* and String overloads), writeBytes/readBytes,
 * template put/get, getDataPtr, and out-of-bounds safety.
 */

#include <Arduino.h>
#include <EEPROM.h>
#include <unity.h>

#define EEPROM_SIZE 128

void setUp(void) {
  EEPROM.begin(EEPROM_SIZE);
}

void tearDown(void) {
  EEPROM.end();
}

// ==================== Lifecycle ====================

void test_begin_length(void) {
  TEST_ASSERT_EQUAL(EEPROM_SIZE, (int)EEPROM.length());
}

// ==================== Basic byte read/write ====================

void test_read_write_byte(void) {
  EEPROM.write(0, 0xAB);
  TEST_ASSERT_EQUAL_HEX8(0xAB, EEPROM.read(0));
}

void test_write_multiple_bytes(void) {
  EEPROM.write(0, 0x11);
  EEPROM.write(1, 0x22);
  EEPROM.write(2, 0x33);
  TEST_ASSERT_EQUAL_HEX8(0x11, EEPROM.read(0));
  TEST_ASSERT_EQUAL_HEX8(0x22, EEPROM.read(1));
  TEST_ASSERT_EQUAL_HEX8(0x33, EEPROM.read(2));
}

// ==================== isDirty / commit ====================

void test_not_dirty_after_begin(void) {
  TEST_ASSERT_FALSE(EEPROM.isDirty());
}

void test_dirty_after_write(void) {
  // XOR current byte to guarantee a different value
  uint8_t val = EEPROM.read(0) ^ 0x01;
  EEPROM.write(0, val);
  TEST_ASSERT_TRUE(EEPROM.isDirty());
}

void test_not_dirty_after_same_value_write(void) {
  // Write a value, commit to clear dirty, then write the same value
  EEPROM.write(5, 0xAA);
  EEPROM.commit();
  TEST_ASSERT_FALSE(EEPROM.isDirty());
  EEPROM.write(5, 0xAA);
  TEST_ASSERT_FALSE(EEPROM.isDirty());
}

void test_commit_clears_dirty(void) {
  EEPROM.write(0, 0x42);
  TEST_ASSERT_TRUE(EEPROM.isDirty());
  TEST_ASSERT_TRUE(EEPROM.commit());
  TEST_ASSERT_FALSE(EEPROM.isDirty());
}

// ==================== Typed write / read ====================

void test_read_write_typed_byte(void) {
  EEPROM.writeByte(0, 200);
  TEST_ASSERT_EQUAL_UINT8(200, EEPROM.readByte(0));
}

void test_read_write_typed_char(void) {
  EEPROM.writeChar(0, -42);
  TEST_ASSERT_EQUAL_INT8(-42, EEPROM.readChar(0));
}

void test_read_write_typed_uchar(void) {
  EEPROM.writeUChar(0, 250);
  TEST_ASSERT_EQUAL_UINT8(250, EEPROM.readUChar(0));
}

void test_read_write_typed_short(void) {
  EEPROM.writeShort(0, -1234);
  TEST_ASSERT_EQUAL_INT16(-1234, EEPROM.readShort(0));
}

void test_read_write_typed_ushort(void) {
  EEPROM.writeUShort(0, 60000);
  TEST_ASSERT_EQUAL_UINT16(60000, EEPROM.readUShort(0));
}

void test_read_write_typed_int(void) {
  EEPROM.writeInt(0, -123456);
  TEST_ASSERT_EQUAL_INT32(-123456, EEPROM.readInt(0));
}

void test_read_write_typed_uint(void) {
  EEPROM.writeUInt(0, 3000000000UL);
  TEST_ASSERT_EQUAL_UINT32(3000000000UL, EEPROM.readUInt(0));
}

void test_read_write_typed_long(void) {
  EEPROM.writeLong(0, -100000);
  TEST_ASSERT_EQUAL_INT32(-100000, EEPROM.readLong(0));
}

void test_read_write_typed_ulong(void) {
  EEPROM.writeULong(0, 2999999999UL);
  TEST_ASSERT_EQUAL_UINT32(2999999999UL, EEPROM.readULong(0));
}

void test_read_write_typed_long64(void) {
  int64_t val = -9000000000LL;
  EEPROM.writeLong64(0, val);
  int64_t result = EEPROM.readLong64(0);
  TEST_ASSERT_EQUAL_MEMORY(&val, &result, sizeof(int64_t));
}

void test_read_write_typed_ulong64(void) {
  uint64_t val = 18000000000ULL;
  EEPROM.writeULong64(0, val);
  uint64_t result = EEPROM.readULong64(0);
  TEST_ASSERT_EQUAL_MEMORY(&val, &result, sizeof(uint64_t));
}

void test_read_write_typed_float(void) {
  float val = 3.14159f;
  EEPROM.writeFloat(0, val);
  TEST_ASSERT_EQUAL_FLOAT(val, EEPROM.readFloat(0));
}

void test_read_write_typed_double(void) {
  double val = 2.718281828459045;
  EEPROM.writeDouble(0, val);
  TEST_ASSERT_EQUAL_DOUBLE(val, EEPROM.readDouble(0));
}

void test_read_write_typed_bool_true(void) {
  EEPROM.writeBool(0, true);
  TEST_ASSERT_TRUE(EEPROM.readBool(0));
}

void test_read_write_typed_bool_false(void) {
  EEPROM.writeBool(0, false);
  TEST_ASSERT_FALSE(EEPROM.readBool(0));
}

// ==================== String ====================

void test_read_write_string_cstr(void) {
  const char *str = "Hello";
  EEPROM.writeString(0, str);
  char buf[20] = {};
  size_t len = EEPROM.readString(0, buf, sizeof(buf));
  TEST_ASSERT_EQUAL(5, (int)len);
  TEST_ASSERT_EQUAL_STRING("Hello", buf);
}

void test_read_write_string_obj(void) {
  EEPROM.writeString(0, String("World"));
  String result = EEPROM.readString(0);
  TEST_ASSERT_EQUAL_STRING("World", result.c_str());
}

void test_read_write_empty_string(void) {
  EEPROM.writeString(0, "");
  String result = EEPROM.readString(0);
  TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

// ==================== Bytes ====================

void test_read_write_bytes(void) {
  const uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
  EEPROM.writeBytes(0, data, sizeof(data));
  uint8_t out[5] = {};
  size_t len = EEPROM.readBytes(0, out, sizeof(out));
  TEST_ASSERT_EQUAL(5, (int)len);
  TEST_ASSERT_EQUAL_MEMORY(data, out, 5);
}

// ==================== Template put / get ====================

void test_template_put_get_struct(void) {
  struct Payload {
    uint8_t id;
    uint16_t value;
    float fval;
  };

  Payload written = {42, 1000, 2.5f};
  EEPROM.put(0, written);

  Payload read = {};
  EEPROM.get(0, read);

  TEST_ASSERT_EQUAL_UINT8(written.id, read.id);
  TEST_ASSERT_EQUAL_UINT16(written.value, read.value);
  TEST_ASSERT_EQUAL_FLOAT(written.fval, read.fval);
}

// ==================== getDataPtr ====================

void test_get_data_ptr(void) {
  uint8_t *ptr = EEPROM.getDataPtr();
  TEST_ASSERT_NOT_NULL(ptr);
  ptr[0] = 0xDE;
  ptr[1] = 0xAD;
  TEST_ASSERT_EQUAL_HEX8(0xDE, EEPROM.read(0));
  TEST_ASSERT_EQUAL_HEX8(0xAD, EEPROM.read(1));
}

// ==================== Out-of-bounds safety ====================

void test_out_of_bounds_read_returns_zero(void) {
  TEST_ASSERT_EQUAL_HEX8(0, EEPROM.read(EEPROM_SIZE));
  TEST_ASSERT_EQUAL_HEX8(0, EEPROM.read(EEPROM_SIZE + 10));
}

void test_out_of_bounds_write_safe(void) {
  EEPROM.write(0, 0xCA);
  // These must not crash and must not corrupt valid data
  EEPROM.write(EEPROM_SIZE, 0xFF);
  EEPROM.write(EEPROM_SIZE + 5, 0xFF);
  TEST_ASSERT_EQUAL_HEX8(0xCA, EEPROM.read(0));
}

void test_typed_out_of_bounds_returns_default(void) {
  // writeInt at an address that would overflow — should return 0 (no write)
  int32_t before = EEPROM.readInt(EEPROM_SIZE - 2);
  size_t written = EEPROM.writeInt(EEPROM_SIZE - 2, 0x12345678);
  TEST_ASSERT_EQUAL(0, (int)written);
  // Value at that address should be unchanged
  TEST_ASSERT_EQUAL(before, EEPROM.readInt(EEPROM_SIZE - 2));
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  UNITY_BEGIN();

  // Lifecycle
  RUN_TEST(test_begin_length);

  // Basic byte
  RUN_TEST(test_read_write_byte);
  RUN_TEST(test_write_multiple_bytes);

  // isDirty / commit
  RUN_TEST(test_not_dirty_after_begin);
  RUN_TEST(test_dirty_after_write);
  RUN_TEST(test_not_dirty_after_same_value_write);
  RUN_TEST(test_commit_clears_dirty);

  // Typed write/read
  RUN_TEST(test_read_write_typed_byte);
  RUN_TEST(test_read_write_typed_char);
  RUN_TEST(test_read_write_typed_uchar);
  RUN_TEST(test_read_write_typed_short);
  RUN_TEST(test_read_write_typed_ushort);
  RUN_TEST(test_read_write_typed_int);
  RUN_TEST(test_read_write_typed_uint);
  RUN_TEST(test_read_write_typed_long);
  RUN_TEST(test_read_write_typed_ulong);
  RUN_TEST(test_read_write_typed_long64);
  RUN_TEST(test_read_write_typed_ulong64);
  RUN_TEST(test_read_write_typed_float);
  RUN_TEST(test_read_write_typed_double);
  RUN_TEST(test_read_write_typed_bool_true);
  RUN_TEST(test_read_write_typed_bool_false);

  // String
  RUN_TEST(test_read_write_string_cstr);
  RUN_TEST(test_read_write_string_obj);
  RUN_TEST(test_read_write_empty_string);

  // Bytes
  RUN_TEST(test_read_write_bytes);

  // Template put/get
  RUN_TEST(test_template_put_get_struct);

  // getDataPtr
  RUN_TEST(test_get_data_ptr);

  // Out-of-bounds safety
  RUN_TEST(test_out_of_bounds_read_returns_zero);
  RUN_TEST(test_out_of_bounds_write_safe);
  RUN_TEST(test_typed_out_of_bounds_returns_default);

  UNITY_END();
}

void loop() {}
