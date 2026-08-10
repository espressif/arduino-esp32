# EEPROM Validation Test

Validates the `EEPROM` library covering lifecycle, byte-level read/write, dirty flag and commit behavior, all typed accessors, string and byte array operations, template put/get, raw data pointer access, and out-of-bounds safety.

## Test Cases

| Test Function | Description |
|---|---|
| `test_begin_length` | Verify `EEPROM.length()` matches requested size |
| `test_read_write_byte` | Write and read back a single byte |
| `test_write_multiple_bytes` | Write and read back multiple consecutive bytes |
| `test_not_dirty_after_begin` | `isDirty()` is false immediately after `begin()` |
| `test_dirty_after_write` | `isDirty()` is true after writing a different value |
| `test_not_dirty_after_same_value_write` | Writing the same value does not set dirty flag |
| `test_commit_clears_dirty` | `commit()` clears the dirty flag |
| `test_read_write_typed_byte` | `writeByte` / `readByte` round-trip |
| `test_read_write_typed_char` | `writeChar` / `readChar` round-trip (signed) |
| `test_read_write_typed_uchar` | `writeUChar` / `readUChar` round-trip |
| `test_read_write_typed_short` | `writeShort` / `readShort` round-trip |
| `test_read_write_typed_ushort` | `writeUShort` / `readUShort` round-trip |
| `test_read_write_typed_int` | `writeInt` / `readInt` round-trip |
| `test_read_write_typed_uint` | `writeUInt` / `readUInt` round-trip |
| `test_read_write_typed_long` | `writeLong` / `readLong` round-trip |
| `test_read_write_typed_ulong` | `writeULong` / `readULong` round-trip |
| `test_read_write_typed_long64` | `writeLong64` / `readLong64` round-trip |
| `test_read_write_typed_ulong64` | `writeULong64` / `readULong64` round-trip |
| `test_read_write_typed_float` | `writeFloat` / `readFloat` round-trip |
| `test_read_write_typed_double` | `writeDouble` / `readDouble` round-trip |
| `test_read_write_typed_bool_true` | `writeBool(true)` / `readBool` round-trip |
| `test_read_write_typed_bool_false` | `writeBool(false)` / `readBool` round-trip |
| `test_read_write_string_cstr` | `writeString` / `readString` with C-string |
| `test_read_write_string_obj` | `writeString` / `readString` with Arduino `String` |
| `test_read_write_empty_string` | Write and read an empty string |
| `test_read_write_bytes` | `writeBytes` / `readBytes` raw byte array |
| `test_template_put_get_struct` | `put` / `get` with a struct (template API) |
| `test_get_data_ptr` | Direct memory access via `getDataPtr()` |
| `test_out_of_bounds_read_returns_zero` | Reading beyond EEPROM size returns 0 |
| `test_out_of_bounds_write_safe` | Writing beyond EEPROM size does not crash or corrupt data |
| `test_typed_out_of_bounds_returns_default` | Typed write at overflowing address returns 0 (no write) |

## Requirements

- **Hardware**: Any ESP32 variant
- **Wokwi/QEMU**: Supported
