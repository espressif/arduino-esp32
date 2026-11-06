# Hash Validation Test

Validates the Hash library including HEXBuilder, MD5Builder, SHA-1/2/3 builders, HMAC, and PBKDF2-HMAC using known-answer test vectors from NIST FIPS 180-4, FIPS 202, RFC 1321, RFC 2104/2202/4231, RFC 6070, and Python 3 `hmac.new(..., hashlib.<alg>)`.

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
| `test_pbkdf2_sha512_c1` | PBKDF2-HMAC-SHA512 with 1 iteration (128-byte HMAC block) |
| `test_pbkdf2_sha384_c1` | PBKDF2-HMAC-SHA384 with 1 iteration |
| `test_pbkdf2_sha3_256_c1` | PBKDF2-HMAC-SHA3-256 with 1 iteration |
| `test_hmac_sha1_rfc2202` | HMAC-SHA-1 RFC 2202 test case 1 |
| `test_hmac_sha256_rfc4231_1` | HMAC-SHA-256 RFC 4231 test case 1 |
| `test_hmac_sha256_rfc4231_2` | HMAC-SHA-256 RFC 4231 test case 2 (string key) |
| `test_hmac_sha256_rfc4231_6` | HMAC-SHA-256 RFC 4231 test case 6 (key longer than block) |
| `test_hmac_sha512_rfc4231_1` | HMAC-SHA-512 RFC 4231 test case 1 (128-byte block) |
| `test_hmac_sha256_multi_chunk` | HMAC-SHA-256 with data added in two chunks |
| `test_hmac_sha256_add_stream` | HMAC-SHA-256 from StreamString |
| `test_hmac_sha3_256` | HMAC-SHA3-256 using the hash rate from `getBlockSize()` |
| `test_hmac_sha3_224` | HMAC-SHA3-224 (144-byte rate, the HMAC pad maximum) |
| `test_hmac_unknown_block_size` | HMAC refuses a hash that does not report a block size |
| `test_hash_block_sizes` | `getBlockSize()` for MD5, SHA-1/2/3 |
| `test_hmac_md5` | HMAC-MD5 RFC 2104 test case 1 |
| `test_hmac_sha224` | HMAC-SHA-224, Python hashlib cross-check |
| `test_hmac_sha384` | HMAC-SHA-384, Python hashlib cross-check |
| `test_hmac_sha3_384` | HMAC-SHA3-384 using the SHA-3 rate |
| `test_hmac_sha3_512` | HMAC-SHA3-512 using the SHA-3 rate |
| `test_hmac_empty_key_empty_msg` | HMAC-SHA-256 of empty key and empty message |
| `test_hmac_empty_message` | HMAC-SHA-256 of key `"key"` and empty message |
| `test_hmac_binary_key_and_data` | HMAC-SHA-256 with binary key and `NUL`/`0xff` data |
| `test_hmac_long_key_sha3_256` | HMAC-SHA3-256 with a 200-byte key (hashed down to the rate) |
| `test_hmac_rfc4231_3` | HMAC-SHA-256 RFC 4231 test case 3 |
| `test_hmac_set_key_overloads` | `setKey(bytes / char* / String)` produce the same MAC |
| `test_hmac_set_hash_algorithm` | Construct empty, then `setHashAlgorithm()` SHA-1 then SHA-256 |
| `test_hmac_explicit_block_size` | Explicit SHA-256 block size accepted; oversized block rejected |
| `test_hmac_getbytes_getchars` | `getBytes()` / `getChars()` match `toString()` |
| `test_hmac_reset` | Second `begin()`/`add()`/`calculate()` on the same object |
| `test_hmac_add_string_and_hex` | `add(String)` and `addHexString()` (char* and String) |
| `test_hmac_key_with_spaces` | HMAC-SHA-256 with spaces in the key |
| `test_hmac_md5_string_key` | HMAC-MD5 RFC 2202 test case 2 |
| `test_hmac_calculate_before_begin` | `calculate()` before `begin()` yields an empty digest |
| `test_hmac_null_key` | `setKey(NULL)` after a successful MAC clears the cached result |
| `test_hmac_add_stream_large_maxlen` | `addStream()` with `size_t` max length still hashes the stream |
| `test_sha256_55bytes` | SHA-256 padding boundary at 55 bytes |
| `test_sha256_56bytes` | SHA-256 padding boundary at 56 bytes |
| `test_sha384_112bytes` | SHA-384 padding boundary at 112 bytes |
| `test_sha512_112bytes` | SHA-512 padding boundary at 112 bytes |
| `test_sha256_10000a` | SHA-256 of 10,000 'a' characters (large input) |

## Requirements

- **Hardware**: Any ESP32 variant
- **Wokwi**: Supported
- **QEMU**: Not supported
