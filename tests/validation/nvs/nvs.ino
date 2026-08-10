/*
 * NVS/Preferences Validation Test
 *
 * Covers: all typed put/get (Char, UChar, Short, UShort, Int, UInt, Long,
 * ULong, Long64, ULong64, Float, Double, Bool), String (String + char*),
 * Bytes/struct, isKey, getType, freeEntries, remove, clear,
 * multi-namespace isolation, and persistence across reboots.
 *
 * Persistence test: Python restarts the device after phase 1 and
 * the sketch detects the boot count to run phase 2 verification.
 */

#include <Arduino.h>
#include <Preferences.h>

#include <unity.h>

Preferences prefs;

struct TestData {
  uint8_t id;
  uint16_t value;
};

void setUp(void) {}
void tearDown(void) {}

// ==================== Typed Put/Get ====================

void test_char(void) {
  prefs.begin("test-types", false);
  TEST_ASSERT_TRUE(prefs.putChar("c", 'Z'));
  TEST_ASSERT_EQUAL('Z', prefs.getChar("c", 0));
  prefs.end();
}

void test_uchar(void) {
  prefs.begin("test-types", false);
  TEST_ASSERT_TRUE(prefs.putUChar("uc", 200));
  TEST_ASSERT_EQUAL(200, prefs.getUChar("uc", 0));
  prefs.end();
}

void test_short(void) {
  prefs.begin("test-types", false);
  TEST_ASSERT_TRUE(prefs.putShort("s", -1234));
  TEST_ASSERT_EQUAL(-1234, prefs.getShort("s", 0));
  prefs.end();
}

void test_ushort(void) {
  prefs.begin("test-types", false);
  TEST_ASSERT_TRUE(prefs.putUShort("us", 50000));
  TEST_ASSERT_EQUAL(50000, prefs.getUShort("us", 0));
  prefs.end();
}

void test_int(void) {
  prefs.begin("test-types", false);
  TEST_ASSERT_TRUE(prefs.putInt("i", -100000));
  TEST_ASSERT_EQUAL(-100000, prefs.getInt("i", 0));
  prefs.end();
}

void test_uint(void) {
  prefs.begin("test-types", false);
  TEST_ASSERT_TRUE(prefs.putUInt("ui", 3000000000U));
  TEST_ASSERT_EQUAL(3000000000U, prefs.getUInt("ui", 0));
  prefs.end();
}

void test_long(void) {
  prefs.begin("test-types", false);
  TEST_ASSERT_TRUE(prefs.putLong("l", -999999));
  TEST_ASSERT_EQUAL(-999999, prefs.getLong("l", 0));
  prefs.end();
}

void test_ulong(void) {
  prefs.begin("test-types", false);
  TEST_ASSERT_TRUE(prefs.putULong("ul", 4000000000U));
  TEST_ASSERT_EQUAL(4000000000U, prefs.getULong("ul", 0));
  prefs.end();
}

void test_long64(void) {
  prefs.begin("test-types", false);
  int64_t val = -1234567890123LL;
  TEST_ASSERT_TRUE(prefs.putLong64("l64", val));
  int64_t got = prefs.getLong64("l64", 0);
  TEST_ASSERT_TRUE_MESSAGE(val == got, "Long64 read-back mismatch");
  prefs.end();
}

void test_ulong64(void) {
  prefs.begin("test-types", false);
  uint64_t val = 9876543210123ULL;
  TEST_ASSERT_TRUE(prefs.putULong64("ul64", val));
  uint64_t got = prefs.getULong64("ul64", 0);
  TEST_ASSERT_TRUE_MESSAGE(val == got, "ULong64 read-back mismatch");
  prefs.end();
}

void test_float(void) {
  prefs.begin("test-types", false);
  TEST_ASSERT_TRUE(prefs.putFloat("f", 3.14f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.14f, prefs.getFloat("f", 0.0f));
  prefs.end();
}

void test_double(void) {
  prefs.begin("test-types", false);
  TEST_ASSERT_TRUE(prefs.putDouble("d", 2.718281828));
  TEST_ASSERT_DOUBLE_WITHIN(0.000001, 2.718281828, prefs.getDouble("d", 0.0));
  prefs.end();
}

void test_bool(void) {
  prefs.begin("test-types", false);
  TEST_ASSERT_TRUE(prefs.putBool("b", true));
  TEST_ASSERT_TRUE(prefs.getBool("b", false));
  TEST_ASSERT_TRUE(prefs.putBool("b", false));
  TEST_ASSERT_FALSE(prefs.getBool("b", true));
  prefs.end();
}

// ==================== String ====================

void test_string_object(void) {
  prefs.begin("test-str", false);
  String val = "Hello Preferences!";
  TEST_ASSERT_TRUE(prefs.putString("so", val));
  TEST_ASSERT_EQUAL_STRING(val.c_str(), prefs.getString("so", "").c_str());
  prefs.end();
}

void test_string_cstr(void) {
  prefs.begin("test-str", false);
  const char *val = "C-string test";
  TEST_ASSERT_TRUE(prefs.putString("sc", val));
  char buf[32] = {0};
  size_t len = prefs.getString("sc", buf, sizeof(buf));
  TEST_ASSERT_GREATER_THAN(0, len);
  TEST_ASSERT_EQUAL_STRING(val, buf);
  prefs.end();
}

// ==================== Bytes / Struct ====================

void test_bytes(void) {
  prefs.begin("test-blob", false);
  uint8_t wbuf[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x42};
  TEST_ASSERT_EQUAL(sizeof(wbuf), prefs.putBytes("raw", wbuf, sizeof(wbuf)));
  uint8_t rbuf[8] = {0};
  size_t len = prefs.getBytes("raw", rbuf, sizeof(rbuf));
  TEST_ASSERT_EQUAL(sizeof(wbuf), len);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(wbuf, rbuf, sizeof(wbuf));
  prefs.end();
}

void test_struct(void) {
  prefs.begin("test-blob", false);
  TestData wd = {42, 1234};
  TEST_ASSERT_EQUAL(sizeof(wd), prefs.putBytes("st", &wd, sizeof(wd)));
  TestData rd = {0, 0};
  size_t len = prefs.getBytes("st", &rd, sizeof(rd));
  TEST_ASSERT_EQUAL(sizeof(rd), len);
  TEST_ASSERT_EQUAL(42, rd.id);
  TEST_ASSERT_EQUAL(1234, rd.value);
  prefs.end();
}

// ==================== isKey / getType ====================

void test_is_key(void) {
  prefs.begin("test-key", false);
  prefs.clear();
  TEST_ASSERT_FALSE(prefs.isKey("nokey"));
  prefs.putInt("exists", 99);
  TEST_ASSERT_TRUE(prefs.isKey("exists"));
  prefs.end();
}

void test_get_type(void) {
  prefs.begin("test-key", false);
  prefs.putChar("tc", 'A');
  TEST_ASSERT_EQUAL(PT_I8, prefs.getType("tc"));
  prefs.putUInt("tu", 1);
  TEST_ASSERT_EQUAL(PT_U32, prefs.getType("tu"));
  prefs.putString("ts", "hi");
  TEST_ASSERT_EQUAL(PT_STR, prefs.getType("ts"));
  TEST_ASSERT_EQUAL(PT_INVALID, prefs.getType("nope"));
  prefs.end();
}

// ==================== freeEntries ====================

void test_free_entries(void) {
  prefs.begin("test-free", false);
  prefs.clear();
  size_t before = prefs.freeEntries();
  TEST_ASSERT_GREATER_THAN(0, before);
  prefs.putInt("k1", 1);
  prefs.putInt("k2", 2);
  size_t after = prefs.freeEntries();
  TEST_ASSERT_LESS_THAN(before, after);
  prefs.end();
}

// ==================== remove ====================

void test_remove(void) {
  prefs.begin("test-rm", false);
  prefs.clear();
  prefs.putInt("rmkey", 123);
  TEST_ASSERT_TRUE(prefs.isKey("rmkey"));
  TEST_ASSERT_TRUE(prefs.remove("rmkey"));
  TEST_ASSERT_FALSE(prefs.isKey("rmkey"));
  TEST_ASSERT_EQUAL(0, prefs.getInt("rmkey", 0));
  prefs.end();
}

// ==================== clear ====================

void test_clear(void) {
  prefs.begin("test-clr", false);
  prefs.putInt("a", 1);
  prefs.putInt("b", 2);
  prefs.putString("c", "val");
  TEST_ASSERT_TRUE(prefs.clear());
  TEST_ASSERT_FALSE(prefs.isKey("a"));
  TEST_ASSERT_FALSE(prefs.isKey("b"));
  TEST_ASSERT_FALSE(prefs.isKey("c"));
  prefs.end();
}

// ==================== Namespace isolation ====================

void test_namespace_isolation(void) {
  prefs.begin("ns-alpha", false);
  prefs.clear();
  prefs.putInt("shared", 111);
  prefs.end();

  prefs.begin("ns-beta", false);
  prefs.clear();
  prefs.putInt("shared", 222);
  prefs.end();

  prefs.begin("ns-alpha", false);
  TEST_ASSERT_EQUAL(111, prefs.getInt("shared", 0));
  prefs.end();

  prefs.begin("ns-beta", false);
  TEST_ASSERT_EQUAL(222, prefs.getInt("shared", 0));
  prefs.end();
}

// ==================== Read-only mode ====================

void test_readonly(void) {
  prefs.begin("test-ro", false);
  prefs.putInt("rokey", 77);
  prefs.end();

  prefs.begin("test-ro", true);
  TEST_ASSERT_EQUAL(77, prefs.getInt("rokey", 0));
  TEST_ASSERT_FALSE(prefs.putInt("rokey", 88));
  TEST_ASSERT_EQUAL(77, prefs.getInt("rokey", 0));
  prefs.end();
}

// ==================== Default values for missing keys ====================

void test_defaults(void) {
  prefs.begin("test-def", false);
  prefs.clear();
  TEST_ASSERT_EQUAL(42, prefs.getInt("missing", 42));
  TEST_ASSERT_EQUAL_STRING("default", prefs.getString("missing", "default").c_str());
  TEST_ASSERT_EQUAL(0, prefs.getBytes("missing", NULL, 0));
  prefs.end();
}

// ==================== Persistence across reboot ====================

void test_persistence_write(void) {
  prefs.begin("test-persist", false);
  prefs.clear();
  prefs.putInt("counter", 100);
  prefs.putString("msg", "survived");
  prefs.end();
}

void test_persistence_verify(void) {
  prefs.begin("test-persist", true);
  TEST_ASSERT_EQUAL(100, prefs.getInt("counter", 0));
  TEST_ASSERT_EQUAL_STRING("survived", prefs.getString("msg", "").c_str());
  prefs.end();
}

// ==================== Main ====================

static bool is_persistence_verify_phase() {
  prefs.begin("test-persist", true);
  bool has_data = prefs.isKey("counter");
  prefs.end();
  return has_data;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  UNITY_BEGIN();

  if (is_persistence_verify_phase()) {
    RUN_TEST(test_persistence_verify);
  } else {
    RUN_TEST(test_char);
    RUN_TEST(test_uchar);
    RUN_TEST(test_short);
    RUN_TEST(test_ushort);
    RUN_TEST(test_int);
    RUN_TEST(test_uint);
    RUN_TEST(test_long);
    RUN_TEST(test_ulong);
    RUN_TEST(test_long64);
    RUN_TEST(test_ulong64);
    RUN_TEST(test_float);
    RUN_TEST(test_double);
    RUN_TEST(test_bool);
    RUN_TEST(test_string_object);
    RUN_TEST(test_string_cstr);
    RUN_TEST(test_bytes);
    RUN_TEST(test_struct);
    RUN_TEST(test_is_key);
    RUN_TEST(test_get_type);
    RUN_TEST(test_free_entries);
    RUN_TEST(test_remove);
    RUN_TEST(test_clear);
    RUN_TEST(test_namespace_isolation);
    RUN_TEST(test_readonly);
    RUN_TEST(test_defaults);
    RUN_TEST(test_persistence_write);
  }

  UNITY_END();
}

void loop() {}
