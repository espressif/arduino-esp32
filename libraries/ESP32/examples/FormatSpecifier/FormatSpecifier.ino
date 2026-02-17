/*
 * FormatSpecifier Example
 *
 * Format specifiers can vary depending on the architecture and the compiler.
 *
 * This sketch will print the format specifiers for the different types for
 * the current architecture and compiler.
 */

#include <Arduino.h>
#include <inttypes.h>
#include <type_traits>
#include <stddef.h>

template<typename T> void reportIntegral(const char *name) {
  Serial.printf("%-18s | size: %2u | format: ", name, (unsigned)sizeof(T));

  // Character types
  if constexpr (std::is_same_v<T, char>) {
    Serial.println("char: %c  | numeric: %hhd");
  } else if constexpr (std::is_same_v<T, signed char>) {
    Serial.println("char: %c  | numeric: %hhd");
  } else if constexpr (std::is_same_v<T, unsigned char>) {
    Serial.println("char: %c  | numeric: %hhu");
  }

  // Fixed-width integer types
  else if constexpr (std::is_same_v<T, int8_t>) {
    Serial.println("%" PRId8);
  } else if constexpr (std::is_same_v<T, uint8_t>) {
    Serial.println("%" PRIu8);
  } else if constexpr (std::is_same_v<T, int16_t>) {
    Serial.println("%" PRId16);
  } else if constexpr (std::is_same_v<T, uint16_t>) {
    Serial.println("%" PRIu16);
  } else if constexpr (std::is_same_v<T, int32_t>) {
    Serial.println("%" PRId32);
  } else if constexpr (std::is_same_v<T, uint32_t>) {
    Serial.println("%" PRIu32);
  } else if constexpr (std::is_same_v<T, int64_t>) {
    Serial.println("%" PRId64);
  } else if constexpr (std::is_same_v<T, uint64_t>) {
    Serial.println("%" PRIu64);
  } else if constexpr (std::is_same_v<T, intmax_t>) {
    Serial.println("%" PRIdMAX);
  } else if constexpr (std::is_same_v<T, uintmax_t>) {
    Serial.println("%" PRIuMAX);
  } else if constexpr (std::is_same_v<T, intptr_t>) {
    Serial.println("%" PRIdPTR);
  } else if constexpr (std::is_same_v<T, uintptr_t>) {
    Serial.println("%" PRIuPTR);
  }

  // Native types not covered by <inttypes.h>
  else if constexpr (std::is_same_v<T, short>) {
    Serial.println("%hd");
  } else if constexpr (std::is_same_v<T, unsigned short>) {
    Serial.println("%hu");
  } else if constexpr (std::is_same_v<T, int>) {
    Serial.println("%d");
  } else if constexpr (std::is_same_v<T, unsigned int>) {
    Serial.println("%u");
  } else if constexpr (std::is_same_v<T, long>) {
    Serial.println("%ld");
  } else if constexpr (std::is_same_v<T, unsigned long>) {
    Serial.println("%lu");
  } else if constexpr (std::is_same_v<T, long long>) {
    Serial.println("%lld");
  } else if constexpr (std::is_same_v<T, unsigned long long>) {
    Serial.println("%llu");
  }

  else {
    Serial.println("UNKNOWN");
  }
}

template<typename T> void reportFloat(const char *name) {
  Serial.printf("%-18s | size: %2u | format: ", name, (unsigned)sizeof(T));

#if LDBL_MANT_DIG == DBL_MANT_DIG
  if constexpr (std::is_same_v<T, long double>) {
    Serial.println("%f (same as double)");
  } else if constexpr (std::is_same_v<T, float>) {
    Serial.println("%f");
  } else if constexpr (std::is_same_v<T, double>) {
    Serial.println("%f");
  }
#else
  if constexpr (std::is_same_v<T, long double>) {
    Serial.println("%Lf");
  } else if constexpr (std::is_same_v<T, float>) {
    Serial.println("%f");
  } else if constexpr (std::is_same_v<T, double>) {
    Serial.println("%f");
  }
#endif
}

template<typename T> void reportPointer(const char *name) {
  Serial.printf("%-18s | size: %2u | format: %%p\n", name, (unsigned)sizeof(T));
}

template<typename T> void reportString(const char *name) {
  Serial.printf("%-18s | size: %2u | format: %%s\n", name, (unsigned)sizeof(T));
}

template<typename T> void reportSize(const char *name) {
  Serial.printf("%-18s | size: %2u | format: ", name, (unsigned)sizeof(T));

  if constexpr (std::is_same_v<T, size_t>) {
    Serial.println("%zu");
  } else if constexpr (std::is_same_v<T, ptrdiff_t>) {
    Serial.println("%td");
  } else {
    Serial.println("UNKNOWN");
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("\n===== FORMAT SPECIFIER REPORT =====\n");

  // Boolean
  Serial.printf("%-18s | size: %2u | format: %%d\n", "bool", (unsigned)sizeof(bool));

  // Native integer types
  reportIntegral<char>("char");
  reportIntegral<signed char>("signed char");
  reportIntegral<unsigned char>("unsigned char");

  reportIntegral<short>("short");
  reportIntegral<unsigned short>("unsigned short");

  reportIntegral<int>("int");
  reportIntegral<unsigned int>("unsigned int");

  reportIntegral<long>("long");
  reportIntegral<unsigned long>("unsigned long");

  reportIntegral<long long>("long long");
  reportIntegral<unsigned long long>("unsigned long long");

  // Fixed width types
  reportIntegral<int8_t>("int8_t");
  reportIntegral<uint8_t>("uint8_t");
  reportIntegral<int16_t>("int16_t");
  reportIntegral<uint16_t>("uint16_t");
  reportIntegral<int32_t>("int32_t");
  reportIntegral<uint32_t>("uint32_t");
  reportIntegral<int64_t>("int64_t");
  reportIntegral<uint64_t>("uint64_t");

  reportIntegral<intmax_t>("intmax_t");
  reportIntegral<uintmax_t>("uintmax_t");
  reportIntegral<intptr_t>("intptr_t");
  reportIntegral<uintptr_t>("uintptr_t");

  reportSize<size_t>("size_t");
  reportSize<ptrdiff_t>("ptrdiff_t");

  // Floating point
  reportFloat<float>("float");
  reportFloat<double>("double");
  reportFloat<long double>("long double");

  // Pointer
  reportPointer<void *>("void*");
  reportString<char *>("char*");
  reportString<String>("String");

  Serial.println("\n===== END =====\n");
}

void loop() {
  delay(1000);
}
