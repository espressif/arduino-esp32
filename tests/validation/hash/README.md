# Hash Validation Test

Validates the Hash library including HEXBuilder, MD5Builder, SHA-1/2/3 builders, and PBKDF2-HMAC using known-answer test vectors from NIST FIPS 180-4, FIPS 202, RFC 1321, and RFC 6070.

## Test Cases

| Test Function | Description |
|---|---|
| `test_hex_bytes2hex_string` | Convert bytes to hex string |
| `test_hex_bytes2hex_buffer` | Convert bytes to hex into a char buffer |
| `test_hex_hex2bytes` | Convert hex string to bytes |
| `test_hex_is_valid` | Validate a valid hex string |
| `test_hex_is_invalid` | Reject an invalid hex string |
| `test_hex_roundtrip` | bytes→hex→bytes roundtrip preserves data |
| `test_hex_empty_bytes2hex` | Empty input produces empty hex string |
| `test_hex_empty_hex2bytes` | Empty hex string leaves output buffer untouched |
| `test_hex_is_valid_with_len` | Partial-length hex validation |
| `test_hex_hex2bytes_string_overload` | hex2bytes with String object overload |
| `test_md5_empty` | MD5 of empty input |
| `test_md5_abc` | MD5 of "abc" |
| `test_md5_message_digest` | MD5 of "message digest" |
| `test_md5_alphabet` | MD5 of full lowercase alphabet |
| `test_md5_hash_size` | MD5 hash size is 16 bytes |
| `test_md5_getbytes` | MD5 raw bytes output |
| `test_md5_getchars` | MD5 hex char output |
| `test_md5_multi_chunk` | MD5 with multi-chunk add ("a" + "bc") |
| `test_md5_add_string` | MD5 with String object input |
| `test_md5_add_hex_string` | MD5 with hex string input |
| `test_md5_add_hex_string_obj` | MD5 with hex String object input |
| `test_md5_reset` | MD5 begin/calculate/begin/calculate reset |
| `test_md5_add_stream` | MD5 from StreamString |
| `test_sha1_empty` | SHA-1 of empty input |
| `test_sha1_abc` | SHA-1 of "abc" |
| `test_sha1_hash_size` | SHA-1 hash size is 20 bytes |
| `test_sha1_multi_chunk` | SHA-1 with multi-chunk add |
| `test_sha224_empty` | SHA-224 of empty input |
| `test_sha224_abc` | SHA-224 of "abc" |
| `test_sha224_hash_size` | SHA-224 hash size is 28 bytes |
| `test_sha256_empty` | SHA-256 of empty input |
| `test_sha256_abc` | SHA-256 of "abc" |
| `test_sha256_nist_two_block` | SHA-256 NIST two-block test vector |
| `test_sha256_hash_size` | SHA-256 hash size is 32 bytes |
| `test_sha256_multi_chunk` | SHA-256 with multi-chunk add |
| `test_sha256_add_hex_string` | SHA-256 with hex string input |
| `test_sha256_getbytes` | SHA-256 raw bytes output |
| `test_sha256_reset` | SHA-256 begin/calculate reset |
| `test_sha256_add_stream` | SHA-256 from StreamString |
| `test_sha384_empty` | SHA-384 of empty input |
| `test_sha384_abc` | SHA-384 of "abc" |
| `test_sha384_hash_size` | SHA-384 hash size is 48 bytes |
| `test_sha384_multi_chunk` | SHA-384 with multi-chunk add |
| `test_sha384_nist_two_block` | SHA-384 NIST two-block test vector |
| `test_sha512_empty` | SHA-512 of empty input |
| `test_sha512_abc` | SHA-512 of "abc" |
| `test_sha512_hash_size` | SHA-512 hash size is 64 bytes |
| `test_sha512_multi_chunk` | SHA-512 with multi-chunk add |
| `test_sha512_nist_two_block` | SHA-512 NIST two-block test vector |
| `test_sha3_224_empty` | SHA3-224 of empty input |
| `test_sha3_224_abc` | SHA3-224 of "abc" |
| `test_sha3_224_hash_size` | SHA3-224 hash size is 28 bytes |
| `test_sha3_256_empty` | SHA3-256 of empty input |
| `test_sha3_256_abc` | SHA3-256 of "abc" |
| `test_sha3_256_hash_size` | SHA3-256 hash size is 32 bytes |
| `test_sha3_256_multi_chunk` | SHA3-256 with multi-chunk add |
| `test_sha3_384_empty` | SHA3-384 of empty input |
| `test_sha3_384_abc` | SHA3-384 of "abc" |
| `test_sha3_384_hash_size` | SHA3-384 hash size is 48 bytes |
| `test_sha3_512_empty` | SHA3-512 of empty input |
| `test_sha3_512_abc` | SHA3-512 of "abc" |
| `test_sha3_512_hash_size` | SHA3-512 hash size is 64 bytes |
| `test_pbkdf2_sha1_c1` | PBKDF2-HMAC-SHA1 with 1 iteration (RFC 6070) |
| `test_pbkdf2_sha1_c2` | PBKDF2-HMAC-SHA1 with 2 iterations |
| `test_pbkdf2_sha256_c1` | PBKDF2-HMAC-SHA256 with 1 iteration |
| `test_pbkdf2_sha1_c4096` | PBKDF2-HMAC-SHA1 with 4096 iterations |
| `test_pbkdf2_setters` | PBKDF2 using setter methods for algorithm/password/salt/iterations |
| `test_sha256_55bytes` | SHA-256 padding boundary at 55 bytes |
| `test_sha256_56bytes` | SHA-256 padding boundary at 56 bytes |
| `test_sha384_112bytes` | SHA-384 padding boundary at 112 bytes |
| `test_sha512_112bytes` | SHA-512 padding boundary at 112 bytes |
| `test_sha256_10000a` | SHA-256 of 10,000 'a' characters (large input) |

## Requirements

- **Hardware**: Any ESP32 variant
- **Wokwi**: Supported
- **QEMU**: Not supported
