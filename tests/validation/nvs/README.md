# NVS / Preferences Validation Test

Validates the `Preferences` library covering all typed put/get operations, string and byte/struct storage, key queries, namespace isolation, read-only mode, default values, and data persistence across reboots.

## Test Cases

| Test Function | Description |
|---|---|
| `test_char` | `putChar` / `getChar` round-trip |
| `test_uchar` | `putUChar` / `getUChar` round-trip |
| `test_short` | `putShort` / `getShort` round-trip |
| `test_ushort` | `putUShort` / `getUShort` round-trip |
| `test_int` | `putInt` / `getInt` round-trip |
| `test_uint` | `putUInt` / `getUInt` round-trip |
| `test_long` | `putLong` / `getLong` round-trip |
| `test_ulong` | `putULong` / `getULong` round-trip |
| `test_long64` | `putLong64` / `getLong64` round-trip |
| `test_ulong64` | `putULong64` / `getULong64` round-trip |
| `test_float` | `putFloat` / `getFloat` round-trip |
| `test_double` | `putDouble` / `getDouble` round-trip |
| `test_bool` | `putBool` / `getBool` for true and false |
| `test_string_object` | `putString` / `getString` with Arduino `String` |
| `test_string_cstr` | `putString` / `getString` with C-string and char buffer |
| `test_bytes` | `putBytes` / `getBytes` raw byte array |
| `test_struct` | `putBytes` / `getBytes` with a struct |
| `test_is_key` | `isKey()` returns false for missing key, true after write |
| `test_get_type` | `getType()` returns correct `PreferenceType` enum |
| `test_free_entries` | `freeEntries()` decreases after writing keys |
| `test_remove` | `remove()` deletes a key, subsequent read returns default |
| `test_clear` | `clear()` removes all keys in namespace |
| `test_namespace_isolation` | Same key name in different namespaces holds independent values |
| `test_readonly` | Read-only mode prevents writes but allows reads |
| `test_defaults` | Missing keys return caller-supplied default values |
| `test_persistence_write` | Write known values before reboot (Phase 1) |
| `test_persistence_verify` | Verify values survive reboot (Phase 2) |

## Requirements

- **Hardware**: Any ESP32 variant
- **Wokwi/QEMU**: QEMU not supported

## Notes

- The test runs in two phases. Phase 1 executes all unit tests and writes persistence data. The pytest script then hard-resets the device and Phase 2 verifies that the persisted values survived the reboot.
- On Wokwi (no serial port for hard reset), Phase 2 is automatically skipped.
- The sketch detects which phase to run by checking whether the persistence namespace already contains data at boot.
