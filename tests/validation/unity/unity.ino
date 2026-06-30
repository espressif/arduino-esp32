/**
 * Unity Framework Validation Test
 *
 * Exercises the Unity assertion and control macros available in the ESP32
 * Arduino core build (float + double enabled; 64-bit optional via sdkconfig).
 */

#include <Arduino.h>
#include <math.h>
#include <unity.h>

// Workaround for C++ until bundled Unity picks up esp-idf cast fix
// (Unity*ToPtr returns const void*; C++ rejects implicit conversion to float*/double*).
#undef UNITY_TEST_ASSERT_EACH_EQUAL_FLOAT
#define UNITY_TEST_ASSERT_EACH_EQUAL_FLOAT(expected, actual, num_elements, line, message)                                                   \
  UnityAssertWithinFloatArray(                                                                                                              \
    (UNITY_FLOAT)0, (const UNITY_FLOAT *)UnityFloatToPtr(expected), (const UNITY_FLOAT *)(actual), (UNITY_UINT32)(num_elements), (message), \
    (UNITY_LINE_TYPE)(line), UNITY_ARRAY_TO_VAL                                                                                             \
  )
#undef TEST_ASSERT_EACH_EQUAL_FLOAT
#define TEST_ASSERT_EACH_EQUAL_FLOAT(expected, actual, num_elements) UNITY_TEST_ASSERT_EACH_EQUAL_FLOAT((expected), (actual), (num_elements), __LINE__, NULL)

#undef UNITY_TEST_ASSERT_EACH_EQUAL_DOUBLE
#define UNITY_TEST_ASSERT_EACH_EQUAL_DOUBLE(expected, actual, num_elements, line, message)                                                      \
  UnityAssertWithinDoubleArray(                                                                                                                 \
    (UNITY_DOUBLE)0, (const UNITY_DOUBLE *)UnityDoubleToPtr(expected), (const UNITY_DOUBLE *)(actual), (UNITY_UINT32)(num_elements), (message), \
    (UNITY_LINE_TYPE)(line), UNITY_ARRAY_TO_VAL                                                                                                 \
  )
#undef TEST_ASSERT_EACH_EQUAL_DOUBLE
#define TEST_ASSERT_EACH_EQUAL_DOUBLE(expected, actual, num_elements) UNITY_TEST_ASSERT_EACH_EQUAL_DOUBLE((expected), (actual), (num_elements), __LINE__, NULL)

static int setUpCallCount;
static int tearDownCallCount;

static const int s_int_exp[] = {-3, 0, 7};
static const int s_int_act[] = {-3, 0, 7};
static const int8_t s_int8_exp[] = {-3, 0, 7};
static const int8_t s_int8_act[] = {-3, 0, 7};
static const int16_t s_int16_exp[] = {-300, 0, 700};
static const int16_t s_int16_act[] = {-300, 0, 700};
static const int32_t s_int32_exp[] = {-30000, 0, 70000};
static const int32_t s_int32_act[] = {-30000, 0, 70000};
static const unsigned int s_uint_exp[] = {0, 128, 255};
static const unsigned int s_uint_act[] = {0, 128, 255};
static const uint8_t s_uint8_exp[] = {0, 128, 255};
static const uint8_t s_uint8_act[] = {0, 128, 255};
static const uint16_t s_uint16_exp[] = {0, 1000, 65535};
static const uint16_t s_uint16_act[] = {0, 1000, 65535};
static const uint32_t s_uint32_exp[] = {0, 100000, 4000000000UL};
static const uint32_t s_uint32_act[] = {0, 100000, 4000000000UL};
static const char s_char_exp[] = {'A', 'B', 'C'};
static const char s_char_act[] = {'A', 'B', 'C'};
static const uint8_t s_mem_exp[] = {0xDE, 0xAD, 0xBE, 0xEF};
static const uint8_t s_mem_act[] = {0xDE, 0xAD, 0xBE, 0xEF};
static const float s_float_exp[] = {1.0f, 2.5f, -3.25f};
static const float s_float_act[] = {1.0f, 2.5f, -3.25f};
static const double s_double_exp[] = {1.0, 2.5, -3.25};
static const double s_double_act[] = {1.0, 2.5, -3.25};
static int s_each_int[] = {42, 42, 42};
static const char *s_str_arr_exp[] = {"alpha", "beta", "gamma"};
static const char *s_str_arr_act[] = {"alpha", "beta", "gamma"};
static void *s_ptr_arr[2];

void setUp(void) {
  setUpCallCount++;
}

void tearDown(void) {
  tearDownCallCount++;
}

void suiteSetUp(void) {}

int suiteTearDown(int num_failures) {
  return num_failures;
}

void test_boolean(void) {
  TEST_ASSERT(1);
  TEST_ASSERT_TRUE(1);
  TEST_ASSERT_FALSE(0);
  TEST_ASSERT_UNLESS(0);

  TEST_ASSERT_NULL((void *)NULL);
  TEST_ASSERT_NOT_NULL(&setUpCallCount);

  TEST_ASSERT_EMPTY("");
  TEST_ASSERT_NOT_EMPTY("x");

  TEST_ASSERT_MESSAGE(1, "message variant");
  TEST_ASSERT_TRUE_MESSAGE(1, "true message");
  TEST_ASSERT_FALSE_MESSAGE(0, "false message");
  TEST_ASSERT_NULL_MESSAGE((void *)NULL, "null message");
  TEST_ASSERT_NOT_NULL_MESSAGE(&tearDownCallCount, "not null message");
  TEST_ASSERT_EMPTY_MESSAGE("", "empty message");
  TEST_ASSERT_NOT_EMPTY_MESSAGE("ok", "not empty message");
}

void test_shorthand_int(void) {
  TEST_ASSERT_EQUAL(42, 42);
  TEST_ASSERT_NOT_EQUAL(1, 2);
  TEST_ASSERT_EQUAL_MESSAGE(7, 7, "shorthand equal message");
  TEST_ASSERT_NOT_EQUAL_MESSAGE(3, 4, "shorthand not equal message");
}

void test_integers_equal(void) {
  TEST_ASSERT_EQUAL_INT(42, 42);
  TEST_ASSERT_EQUAL_INT8((int8_t)-8, (int8_t)-8);
  TEST_ASSERT_EQUAL_INT16((int16_t)-1600, (int16_t)-1600);
  TEST_ASSERT_EQUAL_INT32((int32_t)-160000, (int32_t)-160000);
  TEST_ASSERT_EQUAL_UINT((unsigned)42, (unsigned)42);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)200, (uint8_t)200);
  TEST_ASSERT_EQUAL_UINT16((uint16_t)50000, (uint16_t)50000);
  TEST_ASSERT_EQUAL_UINT32((uint32_t)3000000000UL, (uint32_t)3000000000UL);
  TEST_ASSERT_EQUAL_size_t((size_t)99, (size_t)99);
  TEST_ASSERT_EQUAL_HEX(0xABCD, 0xABCD);
  TEST_ASSERT_EQUAL_HEX8(0xAB, 0xAB);
  TEST_ASSERT_EQUAL_HEX16(0xABCD, 0xABCD);
  TEST_ASSERT_EQUAL_HEX32(0xABCDEF01UL, 0xABCDEF01UL);
  TEST_ASSERT_EQUAL_CHAR('Z', 'Z');

  TEST_ASSERT_EQUAL_INT_ARRAY(s_int_exp, s_int_act, 3);
  TEST_ASSERT_EQUAL_INT8_ARRAY(s_int8_exp, s_int8_act, 3);
  TEST_ASSERT_EQUAL_INT16_ARRAY(s_int16_exp, s_int16_act, 3);
  TEST_ASSERT_EQUAL_INT32_ARRAY(s_int32_exp, s_int32_act, 3);
  TEST_ASSERT_EQUAL_UINT_ARRAY(s_uint_exp, s_uint_act, 3);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(s_uint8_exp, s_uint8_act, 3);
  TEST_ASSERT_EQUAL_UINT16_ARRAY(s_uint16_exp, s_uint16_act, 3);
  TEST_ASSERT_EQUAL_UINT32_ARRAY(s_uint32_exp, s_uint32_act, 3);
  TEST_ASSERT_EQUAL_size_t_ARRAY(s_uint32_exp, s_uint32_act, 3);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(s_uint8_exp, s_uint8_act, 3);
  TEST_ASSERT_EQUAL_HEX16_ARRAY(s_uint16_exp, s_uint16_act, 3);
  TEST_ASSERT_EQUAL_HEX32_ARRAY(s_uint32_exp, s_uint32_act, 3);
  TEST_ASSERT_EQUAL_CHAR_ARRAY(s_char_exp, s_char_act, 3);

#ifdef UNITY_SUPPORT_64
  TEST_ASSERT_EQUAL_INT64((int64_t)-9223372036854775807LL, (int64_t)-9223372036854775807LL);
  TEST_ASSERT_EQUAL_UINT64((uint64_t)18446744073709551615ULL, (uint64_t)18446744073709551615ULL);
  TEST_ASSERT_EQUAL_HEX64(0x0123456789ABCDEFULL, 0x0123456789ABCDEFULL);
#endif
}

void test_integers_compare(void) {
  TEST_ASSERT_NOT_EQUAL_INT(1, 2);
  TEST_ASSERT_NOT_EQUAL_INT8((int8_t)1, (int8_t)2);
  TEST_ASSERT_NOT_EQUAL_INT16((int16_t)1, (int16_t)2);
  TEST_ASSERT_NOT_EQUAL_INT32((int32_t)1, (int32_t)2);
  TEST_ASSERT_NOT_EQUAL_UINT(1, 2);
  TEST_ASSERT_NOT_EQUAL_UINT8((uint8_t)1, (uint8_t)2);
  TEST_ASSERT_NOT_EQUAL_UINT16((uint16_t)1, (uint16_t)2);
  TEST_ASSERT_NOT_EQUAL_UINT32((uint32_t)1, (uint32_t)2);
  TEST_ASSERT_NOT_EQUAL_size_t((size_t)1, (size_t)2);
  TEST_ASSERT_NOT_EQUAL_HEX8(0x01, 0x02);
  TEST_ASSERT_NOT_EQUAL_HEX16(0x0100, 0x0200);
  TEST_ASSERT_NOT_EQUAL_HEX32(0x01000000UL, 0x02000000UL);
  TEST_ASSERT_NOT_EQUAL_CHAR('a', 'b');

  TEST_ASSERT_GREATER_THAN(5, 10);
  TEST_ASSERT_GREATER_THAN_INT(5, 10);
  TEST_ASSERT_GREATER_THAN_INT8((int8_t)5, (int8_t)10);
  TEST_ASSERT_GREATER_THAN_INT16((int16_t)5, (int16_t)10);
  TEST_ASSERT_GREATER_THAN_INT32((int32_t)5, (int32_t)10);
  TEST_ASSERT_GREATER_THAN_UINT(5, 10);
  TEST_ASSERT_GREATER_THAN_UINT8((uint8_t)5, (uint8_t)10);
  TEST_ASSERT_GREATER_THAN_UINT16((uint16_t)5, (uint16_t)10);
  TEST_ASSERT_GREATER_THAN_UINT32((uint32_t)5, (uint32_t)10);
  TEST_ASSERT_GREATER_THAN_size_t((size_t)5, (size_t)10);
  TEST_ASSERT_GREATER_THAN_HEX8(0x05, 0x0A);
  TEST_ASSERT_GREATER_THAN_HEX16(0x0500, 0x0A00);
  TEST_ASSERT_GREATER_THAN_HEX32(0x05000000UL, 0x0A000000UL);
  TEST_ASSERT_GREATER_THAN_CHAR('a', 'z');

  TEST_ASSERT_LESS_THAN(10, 5);
  TEST_ASSERT_LESS_THAN_INT(10, 5);
  TEST_ASSERT_LESS_THAN_INT8((int8_t)10, (int8_t)5);
  TEST_ASSERT_LESS_THAN_INT16((int16_t)10, (int16_t)5);
  TEST_ASSERT_LESS_THAN_INT32((int32_t)10, (int32_t)5);
  TEST_ASSERT_LESS_THAN_UINT(10, 5);
  TEST_ASSERT_LESS_THAN_UINT8((uint8_t)10, (uint8_t)5);
  TEST_ASSERT_LESS_THAN_UINT16((uint16_t)10, (uint16_t)5);
  TEST_ASSERT_LESS_THAN_UINT32((uint32_t)10, (uint32_t)5);
  TEST_ASSERT_LESS_THAN_size_t((size_t)10, (size_t)5);
  TEST_ASSERT_LESS_THAN_HEX8(0x0A, 0x05);
  TEST_ASSERT_LESS_THAN_HEX16(0x0A00, 0x0500);
  TEST_ASSERT_LESS_THAN_HEX32(0x0A000000UL, 0x05000000UL);
  TEST_ASSERT_LESS_THAN_CHAR('z', 'a');

  TEST_ASSERT_GREATER_OR_EQUAL(5, 5);
  TEST_ASSERT_GREATER_OR_EQUAL_INT(5, 5);
  TEST_ASSERT_GREATER_OR_EQUAL_INT8((int8_t)5, (int8_t)5);
  TEST_ASSERT_GREATER_OR_EQUAL_INT16((int16_t)5, (int16_t)5);
  TEST_ASSERT_GREATER_OR_EQUAL_INT32((int32_t)5, (int32_t)5);
  TEST_ASSERT_GREATER_OR_EQUAL_UINT(5, 5);
  TEST_ASSERT_GREATER_OR_EQUAL_UINT8((uint8_t)5, (uint8_t)5);
  TEST_ASSERT_GREATER_OR_EQUAL_UINT16((uint16_t)5, (uint16_t)5);
  TEST_ASSERT_GREATER_OR_EQUAL_UINT32((uint32_t)5, (uint32_t)5);
  TEST_ASSERT_GREATER_OR_EQUAL_size_t((size_t)5, (size_t)5);
  TEST_ASSERT_GREATER_OR_EQUAL_HEX8(0x05, 0x05);
  TEST_ASSERT_GREATER_OR_EQUAL_HEX16(0x0500, 0x0500);
  TEST_ASSERT_GREATER_OR_EQUAL_HEX32(0x05000000UL, 0x05000000UL);
  TEST_ASSERT_GREATER_OR_EQUAL_CHAR('m', 'm');

  TEST_ASSERT_LESS_OR_EQUAL(5, 5);
  TEST_ASSERT_LESS_OR_EQUAL_INT(5, 5);
  TEST_ASSERT_LESS_OR_EQUAL_INT8((int8_t)5, (int8_t)5);
  TEST_ASSERT_LESS_OR_EQUAL_INT16((int16_t)5, (int16_t)5);
  TEST_ASSERT_LESS_OR_EQUAL_INT32((int32_t)5, (int32_t)5);
  TEST_ASSERT_LESS_OR_EQUAL_UINT(5, 5);
  TEST_ASSERT_LESS_OR_EQUAL_UINT8((uint8_t)5, (uint8_t)5);
  TEST_ASSERT_LESS_OR_EQUAL_UINT16((uint16_t)5, (uint16_t)5);
  TEST_ASSERT_LESS_OR_EQUAL_UINT32((uint32_t)5, (uint32_t)5);
  TEST_ASSERT_LESS_OR_EQUAL_size_t((size_t)5, (size_t)5);
  TEST_ASSERT_LESS_OR_EQUAL_HEX8(0x05, 0x05);
  TEST_ASSERT_LESS_OR_EQUAL_HEX16(0x0500, 0x0500);
  TEST_ASSERT_LESS_OR_EQUAL_HEX32(0x05000000UL, 0x05000000UL);
  TEST_ASSERT_LESS_OR_EQUAL_CHAR('m', 'm');
}

void test_integers_within(void) {
  TEST_ASSERT_INT_WITHIN(2, 10, 11);
  TEST_ASSERT_INT8_WITHIN(2, 10, 11);
  TEST_ASSERT_INT16_WITHIN(2, 10, 11);
  TEST_ASSERT_INT32_WITHIN(2, 10, 11);
  TEST_ASSERT_UINT_WITHIN(2, 10, 11);
  TEST_ASSERT_UINT8_WITHIN(2, 10, 11);
  TEST_ASSERT_UINT16_WITHIN(2, 10, 11);
  TEST_ASSERT_UINT32_WITHIN(2, 10, 11);
  TEST_ASSERT_size_t_WITHIN(2, 10, 11);
  TEST_ASSERT_HEX_WITHIN(2, 0x10, 0x11);
  TEST_ASSERT_HEX8_WITHIN(2, 0x10, 0x11);
  TEST_ASSERT_HEX16_WITHIN(2, 0x1000, 0x1001);
  TEST_ASSERT_HEX32_WITHIN(2, 0x10000000UL, 0x10000001UL);
  TEST_ASSERT_CHAR_WITHIN(2, 'A', 'B');

  TEST_ASSERT_INT_ARRAY_WITHIN(1, s_int_exp, s_int_act, 3);
  TEST_ASSERT_INT8_ARRAY_WITHIN(1, s_int8_exp, s_int8_act, 3);
  TEST_ASSERT_INT16_ARRAY_WITHIN(1, s_int16_exp, s_int16_act, 3);
  TEST_ASSERT_INT32_ARRAY_WITHIN(1, s_int32_exp, s_int32_act, 3);
  TEST_ASSERT_UINT_ARRAY_WITHIN(1, s_uint_exp, s_uint_act, 3);
  TEST_ASSERT_UINT8_ARRAY_WITHIN(1, s_uint8_exp, s_uint8_act, 3);
  TEST_ASSERT_UINT16_ARRAY_WITHIN(1, s_uint16_exp, s_uint16_act, 3);
  TEST_ASSERT_UINT32_ARRAY_WITHIN(1, s_uint32_exp, s_uint32_act, 3);
  TEST_ASSERT_size_t_ARRAY_WITHIN(1, s_uint32_exp, s_uint32_act, 3);
  TEST_ASSERT_HEX8_ARRAY_WITHIN(1, s_uint8_exp, s_uint8_act, 3);
  TEST_ASSERT_HEX16_ARRAY_WITHIN(1, s_uint16_exp, s_uint16_act, 3);
  TEST_ASSERT_HEX32_ARRAY_WITHIN(1, s_uint32_exp, s_uint32_act, 3);
  TEST_ASSERT_CHAR_ARRAY_WITHIN(1, s_char_exp, s_char_act, 3);
}

void test_bits(void) {
  const uint32_t value = 0xA5A5A5A5UL;
  const uint32_t high_bits = 0xF5000000UL;
  const uint32_t low_bits = 0xA5A50000UL;

  TEST_ASSERT_BITS(0x0F0F0F0FUL, 0x05050505UL, value);
  TEST_ASSERT_BITS_HIGH(0xF0000000UL, high_bits);
  TEST_ASSERT_BITS_LOW(0x0000FFFFUL, low_bits);
  TEST_ASSERT_BIT_HIGH(31, 0x80000000UL);
  TEST_ASSERT_BIT_LOW(0, 0xA5A5A5A4UL);
}

void test_strings_memory_ptr(void) {
  s_ptr_arr[0] = &setUpCallCount;
  s_ptr_arr[1] = &tearDownCallCount;

  TEST_ASSERT_EQUAL_PTR(&setUpCallCount, &setUpCallCount);
  TEST_ASSERT_EQUAL_STRING("hello", "hello");
  TEST_ASSERT_EQUAL_STRING_LEN("hello", "hello", 5);
  TEST_ASSERT_EQUAL_MEMORY(s_mem_exp, s_mem_act, sizeof(s_mem_exp));
  TEST_ASSERT_EQUAL_PTR_ARRAY(s_ptr_arr, s_ptr_arr, 2);
  TEST_ASSERT_EQUAL_STRING_ARRAY(s_str_arr_exp, s_str_arr_act, 3);
  TEST_ASSERT_EQUAL_MEMORY_ARRAY(s_mem_exp, s_mem_act, 2, 2);
}

void test_each_equal(void) {
  TEST_ASSERT_EACH_EQUAL_INT(42, s_each_int, 3);
  TEST_ASSERT_EACH_EQUAL_INT8((int8_t)-3, s_int8_act, 1);
  TEST_ASSERT_EACH_EQUAL_INT8((int8_t)0, &s_int8_act[1], 1);
  TEST_ASSERT_EACH_EQUAL_INT8((int8_t)7, &s_int8_act[2], 1);
  TEST_ASSERT_EACH_EQUAL_INT16((int16_t)-300, s_int16_act, 1);
  TEST_ASSERT_EACH_EQUAL_INT32((int32_t)-30000, s_int32_act, 1);
  TEST_ASSERT_EACH_EQUAL_UINT8((uint8_t)0, s_uint8_act, 1);
  TEST_ASSERT_EACH_EQUAL_UINT8((uint8_t)128, &s_uint8_act[1], 1);
  TEST_ASSERT_EACH_EQUAL_UINT16((uint16_t)1000, &s_uint16_act[1], 1);
  TEST_ASSERT_EACH_EQUAL_UINT32((uint32_t)4000000000UL, &s_uint32_act[2], 1);
  TEST_ASSERT_EACH_EQUAL_size_t((size_t)0, s_uint_exp, 1);
  TEST_ASSERT_EACH_EQUAL_HEX8((uint8_t)0, s_uint8_act, 1);
  TEST_ASSERT_EACH_EQUAL_HEX8((uint8_t)255, &s_uint8_act[2], 1);
  TEST_ASSERT_EACH_EQUAL_HEX16((uint16_t)65535, &s_uint16_act[2], 1);
  TEST_ASSERT_EACH_EQUAL_HEX32((uint32_t)0, s_uint32_act, 1);
  TEST_ASSERT_EACH_EQUAL_PTR(&setUpCallCount, s_ptr_arr, 1);
  TEST_ASSERT_EACH_EQUAL_STRING("alpha", s_str_arr_act, 1);
  TEST_ASSERT_EACH_EQUAL_MEMORY(s_mem_exp, s_mem_act, 1, 1);
  TEST_ASSERT_EACH_EQUAL_CHAR('A', s_char_act, 1);
}

#ifndef UNITY_EXCLUDE_FLOAT
void test_float(void) {
  const float f_det = 3.14f;
  const float f_inf = INFINITY;
  const float f_neg_inf = -INFINITY;
  const float f_nan = NAN;

  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, 1.005f);
  TEST_ASSERT_FLOAT_NOT_WITHIN(0.001f, 1.0f, 1.1f);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, 1.0f);
  TEST_ASSERT_NOT_EQUAL_FLOAT(1.0f, 2.0f);
  TEST_ASSERT_FLOAT_ARRAY_WITHIN(0.01f, s_float_exp, s_float_act, 3);
  TEST_ASSERT_EQUAL_FLOAT_ARRAY(s_float_exp, s_float_act, 3);
  TEST_ASSERT_EACH_EQUAL_FLOAT(1.0f, s_float_act, 1);
  TEST_ASSERT_GREATER_THAN_FLOAT(1.0f, 2.0f);
  TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(2.0f, 2.0f);
  TEST_ASSERT_LESS_THAN_FLOAT(2.0f, 1.0f);
  TEST_ASSERT_LESS_OR_EQUAL_FLOAT(1.0f, 1.0f);
  TEST_ASSERT_FLOAT_IS_INF(f_inf);
  TEST_ASSERT_FLOAT_IS_NEG_INF(f_neg_inf);
  TEST_ASSERT_FLOAT_IS_NAN(f_nan);
  TEST_ASSERT_FLOAT_IS_DETERMINATE(f_det);
  TEST_ASSERT_FLOAT_IS_NOT_INF(f_det);
  TEST_ASSERT_FLOAT_IS_NOT_NEG_INF(f_det);
  TEST_ASSERT_FLOAT_IS_NOT_NAN(f_det);
  TEST_ASSERT_FLOAT_IS_NOT_DETERMINATE(f_nan);
}
#endif

#ifndef UNITY_EXCLUDE_DOUBLE
void test_double(void) {
  const double d_det = 3.14;
  const double d_inf = INFINITY;
  const double d_neg_inf = -INFINITY;
  const double d_nan = NAN;

  TEST_ASSERT_DOUBLE_WITHIN(0.01, 1.0, 1.005);
  TEST_ASSERT_DOUBLE_NOT_WITHIN(0.001, 1.0, 1.1);
  TEST_ASSERT_EQUAL_DOUBLE(1.0, 1.0);
  TEST_ASSERT_NOT_EQUAL_DOUBLE(1.0, 2.0);
  TEST_ASSERT_DOUBLE_ARRAY_WITHIN(0.01, s_double_exp, s_double_act, 3);
  TEST_ASSERT_EQUAL_DOUBLE_ARRAY(s_double_exp, s_double_act, 3);
  TEST_ASSERT_EACH_EQUAL_DOUBLE(1.0, s_double_act, 1);
  TEST_ASSERT_GREATER_THAN_DOUBLE(1.0, 2.0);
  TEST_ASSERT_GREATER_OR_EQUAL_DOUBLE(2.0, 2.0);
  TEST_ASSERT_LESS_THAN_DOUBLE(2.0, 1.0);
  TEST_ASSERT_LESS_OR_EQUAL_DOUBLE(1.0, 1.0);
  TEST_ASSERT_DOUBLE_IS_INF(d_inf);
  TEST_ASSERT_DOUBLE_IS_NEG_INF(d_neg_inf);
  TEST_ASSERT_DOUBLE_IS_NAN(d_nan);
  TEST_ASSERT_DOUBLE_IS_DETERMINATE(d_det);
  TEST_ASSERT_DOUBLE_IS_NOT_INF(d_det);
  TEST_ASSERT_DOUBLE_IS_NOT_NEG_INF(d_det);
  TEST_ASSERT_DOUBLE_IS_NOT_NAN(d_det);
  TEST_ASSERT_DOUBLE_IS_NOT_DETERMINATE(d_nan);
}
#endif

void test_message_variants(void) {
  TEST_MESSAGE("informational message");
  TEST_ASSERT_EQUAL_INT_MESSAGE(1, 1, "int message");
  TEST_ASSERT_GREATER_THAN_MESSAGE(0, 1, "greater message");
  TEST_ASSERT_INT_WITHIN_MESSAGE(1, 5, 5, "within message");
  TEST_ASSERT_BITS_MESSAGE(0x0F, 0x0A, 0x0A, "bits message");
  TEST_ASSERT_EQUAL_STRING_MESSAGE("ok", "ok", "string message");
  TEST_ASSERT_EQUAL_MEMORY_MESSAGE(s_mem_exp, s_mem_act, 2, "memory message");
#ifndef UNITY_EXCLUDE_FLOAT
  TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.1f, 1.0f, 1.05f, "float message");
#endif
#ifndef UNITY_EXCLUDE_DOUBLE
  TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(0.1, 1.0, 1.05, "double message");
#endif
}

void test_esp_err_helpers(void) {
  TEST_ESP_OK(ESP_OK);
  TEST_ESP_ERR(ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_ARG);
}

void test_hooks(void) {
  TEST_ASSERT_GREATER_THAN(0, setUpCallCount);
  TEST_ASSERT_GREATER_THAN(0, tearDownCallCount);
  TEST_ASSERT_GREATER_OR_EQUAL(UNITY_VERSION_MAJOR, 2);
}

void test_pass_early(void) {
  TEST_ASSERT_TRUE(1);
  TEST_PASS_MESSAGE("early pass");
}

void test_ignore(void) {
  TEST_IGNORE_MESSAGE("ignored on purpose");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

  setUpCallCount = 0;
  tearDownCallCount = 0;

  UNITY_BEGIN();

  RUN_TEST(test_boolean);
  RUN_TEST(test_shorthand_int);
  RUN_TEST(test_integers_equal);
  RUN_TEST(test_integers_compare);
  RUN_TEST(test_integers_within);
  RUN_TEST(test_bits);
  RUN_TEST(test_strings_memory_ptr);
  RUN_TEST(test_each_equal);
#ifndef UNITY_EXCLUDE_FLOAT
  RUN_TEST(test_float);
#endif
#ifndef UNITY_EXCLUDE_DOUBLE
  RUN_TEST(test_double);
#endif
  RUN_TEST(test_message_variants);
  RUN_TEST(test_esp_err_helpers);
  RUN_TEST(test_hooks);
  RUN_TEST(test_pass_early);
  RUN_TEST(test_ignore);

  UNITY_END();
}

void loop() {}
