#include <Arduino.h>
#include <Preferences.h>

struct TestData {
  uint8_t id;
  uint16_t value;
};

Preferences preferences;

void validate_types() {
  assert(preferences.getType("char") == PT_I8);
  assert(preferences.getType("uchar") == PT_U8);
  assert(preferences.getType("short") == PT_I16);
  assert(preferences.getType("ushort") == PT_U16);
  assert(preferences.getType("int") == PT_I32);
  assert(preferences.getType("uint") == PT_U32);
  assert(preferences.getType("long") == PT_I32);
  assert(preferences.getType("ulong") == PT_U32);
  assert(preferences.getType("long64") == PT_I64);
  assert(preferences.getType("ulong64") == PT_U64);
  assert(preferences.getType("float") == PT_BLOB);
  assert(preferences.getType("double") == PT_BLOB);
  assert(preferences.getType("bool") == PT_U8);
  assert(preferences.getType("str") == PT_STR);
  assert(preferences.getType("strLen") == PT_STR);
  assert(preferences.getType("struct") == PT_BLOB);
}

// Function to increment string values
void incrementStringValues(String &val_string, char *val_string_buf, size_t buf_size) {
  // Extract the number from string and increment it
  val_string = "str" + String(val_string.substring(3).toInt() + 1);

  // Extract the number from strLen and increment it
  String strLen_str = String(val_string_buf);
  int strLen_num = strLen_str.substring(6).toInt();
  snprintf(val_string_buf, buf_size, "strLen%d", strLen_num + 1);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

  preferences.begin("my-app", false);

  // Get the preferences value and if not exists, use default parameter
  char val_char = preferences.getChar("char", 'A');
  unsigned char val_uchar = preferences.getUChar("uchar", 0);
  int16_t val_short = preferences.getShort("short", 0);
  uint16_t val_ushort = preferences.getUShort("ushort", 0);
  int32_t val_int = preferences.getInt("int", 0);
  uint32_t val_uint = preferences.getUInt("uint", 0);
  int64_t val_long = preferences.getLong("long", 0);
  uint32_t val_ulong = preferences.getULong("ulong", 0);
  int64_t val_long64 = preferences.getLong64("long64", 0);
  uint64_t val_ulong64 = preferences.getULong64("ulong64", 0);
  float val_float = preferences.getFloat("float", 0.0f);
  double val_double = preferences.getDouble("double", 0.0);
  bool val_bool = preferences.getBool("bool", false);

  // Strings
  String val_string = preferences.getString("str", "str0");
  char val_string_buf[20] = "strLen0";
  preferences.getString("strLen", val_string_buf, sizeof(val_string_buf));

  // Structure data
  TestData test_data = {0, 0};

  size_t struct_size = preferences.getBytes("struct", &test_data, sizeof(test_data));
  if (struct_size == 0) {
    // First time - set initial values using parameter names
    test_data.id = 1;
    test_data.value = 100;
  }

  Serial.printf("Values from Preferences: ");
  Serial.printf("char: %c | uchar: %u | short: %d | ushort: %u | int: %ld | uint: %lu | ", val_char, val_uchar, val_short, val_ushort, val_int, val_uint);
  Serial.printf("long: %lld | ulong: %lu | long64: %lld | ulong64: %llu | ", val_long, val_ulong, val_long64, val_ulong64);
  Serial.printf(
    "float: %.2f | double: %.2f | bool: %s | str: %s | strLen: %s | struct: {id:%u,val:%u}\n", val_float, val_double, val_bool ? "true" : "false",
    val_string.c_str(), val_string_buf, test_data.id, test_data.value
  );

  // Increment the values
  val_char += 1;  // Increment char A -> B
  val_uchar += 1;
  val_short += 1;
  val_ushort += 1;
  val_int += 1;
  val_uint += 1;
  val_long += 1;
  val_ulong += 1;
  val_long64 += 1;
  val_ulong64 += 1;
  val_float += 1.1f;
  val_double += 1.1;
  val_bool = !val_bool;  // Toggle boolean value

  // Increment string values using function
  incrementStringValues(val_string, val_string_buf, sizeof(val_string_buf));

  test_data.id += 1;
  test_data.value += 10;

  // Store the updated values back to Preferences
  preferences.putChar("char", val_char);
  preferences.putUChar("uchar", val_uchar);
  preferences.putShort("short", val_short);
  preferences.putUShort("ushort", val_ushort);
  preferences.putInt("int", val_int);
  preferences.putUInt("uint", val_uint);
  preferences.putLong("long", val_long);
  preferences.putULong("ulong", val_ulong);
  preferences.putLong64("long64", val_long64);
  preferences.putULong64("ulong64", val_ulong64);
  preferences.putFloat("float", val_float);
  preferences.putDouble("double", val_double);
  preferences.putBool("bool", val_bool);
  preferences.putString("str", val_string);
  preferences.putString("strLen", val_string_buf);
  preferences.putBytes("struct", &test_data, sizeof(test_data));

  // Check if the keys exist
  assert(preferences.isKey("char"));
  assert(preferences.isKey("struct"));

  // Validate the types of the keys
  validate_types();

  // Close the Preferences, wait and restart
  preferences.end();
  Serial.flush();
  delay(1000);
  ESP.restart();
}

void loop() {}
