# Preferences std::string

Stores a name with `std::string` and reads it back. Pass the `std::string` you already have:

```cpp
name = preferences.getString("name", name);
```

The value is a normal Preferences string, so the Arduino `String` and C-string APIs see the same key.

## Supported Targets

Any ESP32 board.

## How to Use Example

Open the Serial Monitor at 115200 baud, then flash the sketch. Each run writes the namespace `std-string`, key `name`.

Expected log:

```
Stored 13 bytes
std::string: arduino-esp32
```

`getString("name")` and `getString("name", "literal")` return an Arduino `String`. Pass a `std::string` to get one back. Use `std::string()` when the fallback is empty, or `std::string("fallback")` for a missing key:

```cpp
std::string loaded = preferences.getString("name", std::string());
std::string missing = preferences.getString("absent", std::string("fallback"));
String asArduino = preferences.getString("name", "");
```
