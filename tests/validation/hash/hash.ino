/*
 * Validation test for Hash library, MD5Builder, HEXBuilder, and HashBuilder.
 * Uses Unity framework with known-answer test vectors from
 * NIST FIPS 180-4, FIPS 202, RFC 1321, and RFC 6070.
 */

#include <Arduino.h>
#include <unity.h>
#include <MD5Builder.h>
#include <SHA1Builder.h>
#include <SHA2Builder.h>
#include <SHA3Builder.h>
#include <PBKDF2_HMACBuilder.h>
#include <StreamString.h>

void setUp(void) {}
void tearDown(void) {}

// ==================== HEXBuilder ====================

void test_hex_bytes2hex_string(void) {
  const uint8_t data[] = {0x48, 0x65, 0x6c, 0x6c, 0x6f};
  String hex = HEXBuilder::bytes2hex(data, sizeof(data));
  TEST_ASSERT_EQUAL_STRING("48656c6c6f", hex.c_str());
}

void test_hex_bytes2hex_buffer(void) {
  const uint8_t data[] = {0x48, 0x65, 0x6c, 0x6c, 0x6f};
  char buf[11];
  size_t n = HEXBuilder::bytes2hex(buf, sizeof(buf), data, sizeof(data));
  TEST_ASSERT_EQUAL_STRING("48656c6c6f", buf);
  TEST_ASSERT_EQUAL(11, (int)n);
}

void test_hex_hex2bytes(void) {
  const uint8_t expected[] = {0x48, 0x65, 0x6c, 0x6c, 0x6f};
  uint8_t out[5];
  HEXBuilder::hex2bytes(out, sizeof(out), "48656c6c6f");
  TEST_ASSERT_EQUAL_MEMORY(expected, out, 5);
}

void test_hex_is_valid(void) {
  TEST_ASSERT_TRUE(HEXBuilder::isHexString("0123456789abcdefABCDEF"));
}

void test_hex_is_invalid(void) {
  TEST_ASSERT_FALSE(HEXBuilder::isHexString("xyz"));
}

void test_hex_roundtrip(void) {
  const uint8_t original[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x42};
  String hex = HEXBuilder::bytes2hex(original, sizeof(original));
  uint8_t restored[5];
  HEXBuilder::hex2bytes(restored, sizeof(restored), hex.c_str());
  TEST_ASSERT_EQUAL_MEMORY(original, restored, 5);
}

void test_hex_empty_bytes2hex(void) {
  const uint8_t data[] = {0x00};
  String hex = HEXBuilder::bytes2hex(data, 0);
  TEST_ASSERT_EQUAL_STRING("", hex.c_str());
}

void test_hex_empty_hex2bytes(void) {
  uint8_t out[1] = {0xFF};
  HEXBuilder::hex2bytes(out, 0, "");
  TEST_ASSERT_EQUAL_HEX8(0xFF, out[0]);
}

void test_hex_is_valid_with_len(void) {
  TEST_ASSERT_TRUE(HEXBuilder::isHexString("abcdefXYZ", 6));
  TEST_ASSERT_FALSE(HEXBuilder::isHexString("abcdefXYZ", 9));
}

void test_hex_hex2bytes_string_overload(void) {
  const uint8_t expected[] = {0xCA, 0xFE};
  uint8_t out[2];
  String hexStr = "cafe";
  HEXBuilder::hex2bytes(out, sizeof(out), hexStr);
  TEST_ASSERT_EQUAL_MEMORY(expected, out, 2);
}

// ==================== MD5 (RFC 1321) ====================

void test_md5_empty(void) {
  MD5Builder h;
  h.begin();
  h.add((const uint8_t *)"", 0);
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("d41d8cd98f00b204e9800998ecf8427e", h.toString().c_str());
}

void test_md5_abc(void) {
  MD5Builder h;
  h.begin();
  h.add("abc");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("900150983cd24fb0d6963f7d28e17f72", h.toString().c_str());
}

void test_md5_message_digest(void) {
  MD5Builder h;
  h.begin();
  h.add("message digest");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("f96b697d7cb7938d525a2f31aaf161d0", h.toString().c_str());
}

void test_md5_alphabet(void) {
  MD5Builder h;
  h.begin();
  h.add("abcdefghijklmnopqrstuvwxyz");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("c3fcd3d76192e4007dfb496cca67e13b", h.toString().c_str());
}

void test_md5_hash_size(void) {
  MD5Builder h;
  TEST_ASSERT_EQUAL(16, (int)h.getHashSize());
}

void test_md5_getbytes(void) {
  MD5Builder h;
  h.begin();
  h.add("abc");
  h.calculate();
  uint8_t got[16];
  h.getBytes(got);
  const uint8_t expected[] = {0x90, 0x01, 0x50, 0x98, 0x3c, 0xd2, 0x4f, 0xb0, 0xd6, 0x96, 0x3f, 0x7d, 0x28, 0xe1, 0x7f, 0x72};
  TEST_ASSERT_EQUAL_MEMORY(expected, got, 16);
}

void test_md5_getchars(void) {
  MD5Builder h;
  h.begin();
  h.add("abc");
  h.calculate();
  char buf[33];
  h.getChars(buf);
  TEST_ASSERT_EQUAL_STRING("900150983cd24fb0d6963f7d28e17f72", buf);
}

void test_md5_multi_chunk(void) {
  MD5Builder h;
  h.begin();
  h.add("a");
  h.add("bc");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("900150983cd24fb0d6963f7d28e17f72", h.toString().c_str());
}

void test_md5_add_string(void) {
  MD5Builder h;
  h.begin();
  h.add(String("abc"));
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("900150983cd24fb0d6963f7d28e17f72", h.toString().c_str());
}

void test_md5_add_hex_string(void) {
  MD5Builder h;
  h.begin();
  h.addHexString("616263");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("900150983cd24fb0d6963f7d28e17f72", h.toString().c_str());
}

void test_md5_add_hex_string_obj(void) {
  MD5Builder h;
  h.begin();
  h.addHexString(String("616263"));
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("900150983cd24fb0d6963f7d28e17f72", h.toString().c_str());
}

void test_md5_reset(void) {
  MD5Builder h;
  h.begin();
  h.add("first");
  h.calculate();
  h.begin();
  h.add("second");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("a9f0e61a137d86aa9db53465e0801612", h.toString().c_str());
}

void test_md5_add_stream(void) {
  StreamString ss;
  ss.print("Hello, World!");
  MD5Builder h;
  h.begin();
  h.addStream(ss, ss.available());
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("65a8e27d8879283831b664bd8b7f0ad4", h.toString().c_str());
}

// ==================== SHA-1 (FIPS 180-4) ====================

void test_sha1_empty(void) {
  SHA1Builder h;
  h.begin();
  h.add((const uint8_t *)"", 0);
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("da39a3ee5e6b4b0d3255bfef95601890afd80709", h.toString().c_str());
}

void test_sha1_abc(void) {
  SHA1Builder h;
  h.begin();
  h.add("abc");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("a9993e364706816aba3e25717850c26c9cd0d89d", h.toString().c_str());
}

void test_sha1_hash_size(void) {
  SHA1Builder h;
  TEST_ASSERT_EQUAL(20, (int)h.getHashSize());
}

void test_sha1_multi_chunk(void) {
  SHA1Builder h;
  h.begin();
  h.add("a");
  h.add("bc");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("a9993e364706816aba3e25717850c26c9cd0d89d", h.toString().c_str());
}

// ==================== SHA-224 (FIPS 180-4) ====================

void test_sha224_empty(void) {
  SHA224Builder h;
  h.begin();
  h.add((const uint8_t *)"", 0);
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f", h.toString().c_str());
}

void test_sha224_abc(void) {
  SHA224Builder h;
  h.begin();
  h.add("abc");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7", h.toString().c_str());
}

void test_sha224_hash_size(void) {
  SHA224Builder h;
  TEST_ASSERT_EQUAL(28, (int)h.getHashSize());
}

// ==================== SHA-256 (FIPS 180-4) ====================

void test_sha256_empty(void) {
  SHA256Builder h;
  h.begin();
  h.add((const uint8_t *)"", 0);
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", h.toString().c_str());
}

void test_sha256_abc(void) {
  SHA256Builder h;
  h.begin();
  h.add("abc");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", h.toString().c_str());
}

void test_sha256_nist_two_block(void) {
  SHA256Builder h;
  h.begin();
  h.add("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", h.toString().c_str());
}

void test_sha256_hash_size(void) {
  SHA256Builder h;
  TEST_ASSERT_EQUAL(32, (int)h.getHashSize());
}

void test_sha256_multi_chunk(void) {
  SHA256Builder h;
  h.begin();
  h.add("a");
  h.add("bc");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", h.toString().c_str());
}

void test_sha256_add_hex_string(void) {
  SHA256Builder h;
  h.begin();
  h.addHexString("616263");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", h.toString().c_str());
}

void test_sha256_getbytes(void) {
  SHA256Builder h;
  h.begin();
  h.add("abc");
  h.calculate();
  uint8_t got[32];
  h.getBytes(got);
  const uint8_t expected[] = {0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
                              0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
  TEST_ASSERT_EQUAL_MEMORY(expected, got, 32);
}

void test_sha256_reset(void) {
  SHA256Builder h;
  h.begin();
  h.add("first");
  h.calculate();
  h.begin();
  h.add("second");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("16367aacb67a4a017c8da8ab95682ccb390863780f7114dda0a0e0c55644c7c4", h.toString().c_str());
}

void test_sha256_add_stream(void) {
  StreamString ss;
  ss.print("Hello, World!");
  SHA256Builder h;
  h.begin();
  h.addStream(ss, ss.available());
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("dffd6021bb2bd5b0af676290809ec3a53191dd81c7f70a4b28688a362182986f", h.toString().c_str());
}

// ==================== SHA-384 (FIPS 180-4) ====================

void test_sha384_empty(void) {
  SHA384Builder h;
  h.begin();
  h.add((const uint8_t *)"", 0);
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b", h.toString().c_str());
}

void test_sha384_abc(void) {
  SHA384Builder h;
  h.begin();
  h.add("abc");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7", h.toString().c_str());
}

void test_sha384_hash_size(void) {
  SHA384Builder h;
  TEST_ASSERT_EQUAL(48, (int)h.getHashSize());
}

void test_sha384_multi_chunk(void) {
  SHA384Builder h;
  h.begin();
  h.add("a");
  h.add("bc");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7", h.toString().c_str());
}

void test_sha384_nist_two_block(void) {
  SHA384Builder h;
  h.begin();
  h.add("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("09330c33f71147e83d192fc782cd1b4753111b173b3b05d22fa08086e3b0f712fcc7c71a557e2db966c3e9fa91746039", h.toString().c_str());
}

// ==================== SHA-512 (FIPS 180-4) ====================

void test_sha512_empty(void) {
  SHA512Builder h;
  h.begin();
  h.add((const uint8_t *)"", 0);
  h.calculate();
  TEST_ASSERT_EQUAL_STRING(
    "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e", h.toString().c_str()
  );
}

void test_sha512_abc(void) {
  SHA512Builder h;
  h.begin();
  h.add("abc");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING(
    "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f", h.toString().c_str()
  );
}

void test_sha512_hash_size(void) {
  SHA512Builder h;
  TEST_ASSERT_EQUAL(64, (int)h.getHashSize());
}

void test_sha512_multi_chunk(void) {
  SHA512Builder h;
  h.begin();
  h.add("a");
  h.add("bc");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING(
    "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f", h.toString().c_str()
  );
}

void test_sha512_nist_two_block(void) {
  SHA512Builder h;
  h.begin();
  h.add("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING(
    "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909", h.toString().c_str()
  );
}

// ==================== SHA3-224 (FIPS 202) ====================

void test_sha3_224_empty(void) {
  SHA3_224Builder h;
  h.begin();
  h.add((const uint8_t *)"", 0);
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("6b4e03423667dbb73b6e15454f0eb1abd4597f9a1b078e3f5b5a6bc7", h.toString().c_str());
}

void test_sha3_224_abc(void) {
  SHA3_224Builder h;
  h.begin();
  h.add("abc");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("e642824c3f8cf24ad09234ee7d3c766fc9a3a5168d0c94ad73b46fdf", h.toString().c_str());
}

void test_sha3_224_hash_size(void) {
  SHA3_224Builder h;
  TEST_ASSERT_EQUAL(28, (int)h.getHashSize());
}

// ==================== SHA3-256 (FIPS 202) ====================

void test_sha3_256_empty(void) {
  SHA3_256Builder h;
  h.begin();
  h.add((const uint8_t *)"", 0);
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a", h.toString().c_str());
}

void test_sha3_256_abc(void) {
  SHA3_256Builder h;
  h.begin();
  h.add("abc");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532", h.toString().c_str());
}

void test_sha3_256_hash_size(void) {
  SHA3_256Builder h;
  TEST_ASSERT_EQUAL(32, (int)h.getHashSize());
}

void test_sha3_256_multi_chunk(void) {
  SHA3_256Builder h;
  h.begin();
  h.add("a");
  h.add("bc");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532", h.toString().c_str());
}

// ==================== SHA3-384 (FIPS 202) ====================

void test_sha3_384_empty(void) {
  SHA3_384Builder h;
  h.begin();
  h.add((const uint8_t *)"", 0);
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("0c63a75b845e4f7d01107d852e4c2485c51a50aaaa94fc61995e71bbee983a2ac3713831264adb47fb6bd1e058d5f004", h.toString().c_str());
}

void test_sha3_384_abc(void) {
  SHA3_384Builder h;
  h.begin();
  h.add("abc");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("ec01498288516fc926459f58e2c6ad8df9b473cb0fc08c2596da7cf0e49be4b298d88cea927ac7f539f1edf228376d25", h.toString().c_str());
}

void test_sha3_384_hash_size(void) {
  SHA3_384Builder h;
  TEST_ASSERT_EQUAL(48, (int)h.getHashSize());
}

// ==================== SHA3-512 (FIPS 202) ====================

void test_sha3_512_empty(void) {
  SHA3_512Builder h;
  h.begin();
  h.add((const uint8_t *)"", 0);
  h.calculate();
  TEST_ASSERT_EQUAL_STRING(
    "a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a615b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26", h.toString().c_str()
  );
}

void test_sha3_512_abc(void) {
  SHA3_512Builder h;
  h.begin();
  h.add("abc");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING(
    "b751850b1a57168a5693cd924b6b096e08f621827444f70d884f5d0240d2712e10e116e9192af3c91a7ec57647e3934057340b4cf408d5a56592f8274eec53f0", h.toString().c_str()
  );
}

void test_sha3_512_hash_size(void) {
  SHA3_512Builder h;
  TEST_ASSERT_EQUAL(64, (int)h.getHashSize());
}

// ==================== PBKDF2-HMAC (RFC 6070) ====================

void test_pbkdf2_sha1_c1(void) {
  SHA1Builder sha1;
  PBKDF2_HMACBuilder pbkdf2(&sha1, "password", "salt", 1);
  pbkdf2.begin();
  pbkdf2.calculate();
  TEST_ASSERT_EQUAL_STRING("0c60c80f961f0e71f3a9b524af6012062fe037a6", pbkdf2.toString().c_str());
}

void test_pbkdf2_sha1_c2(void) {
  SHA1Builder sha1;
  PBKDF2_HMACBuilder pbkdf2(&sha1, "password", "salt", 2);
  pbkdf2.begin();
  pbkdf2.calculate();
  TEST_ASSERT_EQUAL_STRING("ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957", pbkdf2.toString().c_str());
}

void test_pbkdf2_sha256_c1(void) {
  SHA256Builder sha256;
  PBKDF2_HMACBuilder pbkdf2(&sha256, "password", "salt", 1);
  pbkdf2.begin();
  pbkdf2.calculate();
  TEST_ASSERT_EQUAL_STRING("120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b", pbkdf2.toString().c_str());
}

void test_pbkdf2_sha1_c4096(void) {
  SHA1Builder sha1;
  PBKDF2_HMACBuilder pbkdf2(&sha1, "password", "salt", 4096);
  pbkdf2.begin();
  pbkdf2.calculate();
  TEST_ASSERT_EQUAL_STRING("4b007901b765489abead49d926f721d065a429c1", pbkdf2.toString().c_str());
}

void test_pbkdf2_setters(void) {
  SHA256Builder sha256;
  PBKDF2_HMACBuilder pbkdf2(NULL);
  pbkdf2.setHashAlgorithm(&sha256);
  pbkdf2.setPassword("passwd");
  pbkdf2.setSalt("NaCl");
  pbkdf2.setIterations(1);
  pbkdf2.begin();
  pbkdf2.calculate();
  TEST_ASSERT_EQUAL_STRING("33044695f8609480da53f5241212296156533fee38da64c2149fcf308c36952c", pbkdf2.toString().c_str());
}

// ==================== SHA-2 padding boundaries ====================

void test_sha256_55bytes(void) {
  SHA256Builder h;
  h.begin();
  h.add("1234567890123456789012345678901234567890123456789012345");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("03c3a70e99ed5eeccd80f73771fcf1ece643d939d9ecc76f25544b0233f708e9", h.toString().c_str());
}

void test_sha256_56bytes(void) {
  SHA256Builder h;
  h.begin();
  h.add("12345678901234567890123456789012345678901234567890123456");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("0be66ce72c2467e793202906000672306661791622e0ca9adf4a8955b2ed189c", h.toString().c_str());
}

void test_sha384_112bytes(void) {
  SHA384Builder h;
  h.begin();
  h.add("1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("d40b983bfc6d141686814e1f5bb8f3a94eab5008fbfd5409ab59e51c7f7376ff17815360ebcbf9d41c80296204f50906", h.toString().c_str());
}

void test_sha512_112bytes(void) {
  SHA512Builder h;
  h.begin();
  h.add("1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012");
  h.calculate();
  TEST_ASSERT_EQUAL_STRING(
    "8f555dc0f0fc073d27f9197922151df3fa777ae84635dfd9485733e42b04b2caa0281f59eff15b4a4c48a18f74002084ecaa9315b0a8a8de2bad630e705e6a4f", h.toString().c_str()
  );
}

// ==================== Large input ====================

void test_sha256_10000a(void) {
  SHA256Builder h;
  h.begin();
  const char block[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  for (int i = 0; i < 100; i++) {
    h.add(block);
  }
  h.calculate();
  TEST_ASSERT_EQUAL_STRING("27dd1f61b867b6a0f6e9d8a41c43231de52107e53ae424de8f847b821db4b711", h.toString().c_str());
}

// ==================== setup ====================

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  UNITY_BEGIN();

  // HEXBuilder
  RUN_TEST(test_hex_bytes2hex_string);
  RUN_TEST(test_hex_bytes2hex_buffer);
  RUN_TEST(test_hex_hex2bytes);
  RUN_TEST(test_hex_is_valid);
  RUN_TEST(test_hex_is_invalid);
  RUN_TEST(test_hex_roundtrip);
  RUN_TEST(test_hex_empty_bytes2hex);
  RUN_TEST(test_hex_empty_hex2bytes);
  RUN_TEST(test_hex_is_valid_with_len);
  RUN_TEST(test_hex_hex2bytes_string_overload);

  // MD5
  RUN_TEST(test_md5_empty);
  RUN_TEST(test_md5_abc);
  RUN_TEST(test_md5_message_digest);
  RUN_TEST(test_md5_alphabet);
  RUN_TEST(test_md5_hash_size);
  RUN_TEST(test_md5_getbytes);
  RUN_TEST(test_md5_getchars);
  RUN_TEST(test_md5_multi_chunk);
  RUN_TEST(test_md5_add_string);
  RUN_TEST(test_md5_add_hex_string);
  RUN_TEST(test_md5_add_hex_string_obj);
  RUN_TEST(test_md5_reset);
  RUN_TEST(test_md5_add_stream);

  // SHA-1
  RUN_TEST(test_sha1_empty);
  RUN_TEST(test_sha1_abc);
  RUN_TEST(test_sha1_hash_size);
  RUN_TEST(test_sha1_multi_chunk);

  // SHA-224
  RUN_TEST(test_sha224_empty);
  RUN_TEST(test_sha224_abc);
  RUN_TEST(test_sha224_hash_size);

  // SHA-256
  RUN_TEST(test_sha256_empty);
  RUN_TEST(test_sha256_abc);
  RUN_TEST(test_sha256_nist_two_block);
  RUN_TEST(test_sha256_hash_size);
  RUN_TEST(test_sha256_multi_chunk);
  RUN_TEST(test_sha256_add_hex_string);
  RUN_TEST(test_sha256_getbytes);
  RUN_TEST(test_sha256_reset);
  RUN_TEST(test_sha256_add_stream);

  // SHA-384
  RUN_TEST(test_sha384_empty);
  RUN_TEST(test_sha384_abc);
  RUN_TEST(test_sha384_hash_size);
  RUN_TEST(test_sha384_multi_chunk);
  RUN_TEST(test_sha384_nist_two_block);

  // SHA-512
  RUN_TEST(test_sha512_empty);
  RUN_TEST(test_sha512_abc);
  RUN_TEST(test_sha512_hash_size);
  RUN_TEST(test_sha512_multi_chunk);
  RUN_TEST(test_sha512_nist_two_block);

  // SHA3-224
  RUN_TEST(test_sha3_224_empty);
  RUN_TEST(test_sha3_224_abc);
  RUN_TEST(test_sha3_224_hash_size);

  // SHA3-256
  RUN_TEST(test_sha3_256_empty);
  RUN_TEST(test_sha3_256_abc);
  RUN_TEST(test_sha3_256_hash_size);
  RUN_TEST(test_sha3_256_multi_chunk);

  // SHA3-384
  RUN_TEST(test_sha3_384_empty);
  RUN_TEST(test_sha3_384_abc);
  RUN_TEST(test_sha3_384_hash_size);

  // SHA3-512
  RUN_TEST(test_sha3_512_empty);
  RUN_TEST(test_sha3_512_abc);
  RUN_TEST(test_sha3_512_hash_size);

  // PBKDF2-HMAC
  RUN_TEST(test_pbkdf2_sha1_c1);
  RUN_TEST(test_pbkdf2_sha1_c2);
  RUN_TEST(test_pbkdf2_sha256_c1);
  RUN_TEST(test_pbkdf2_sha1_c4096);
  RUN_TEST(test_pbkdf2_setters);

  // Padding boundaries
  RUN_TEST(test_sha256_55bytes);
  RUN_TEST(test_sha256_56bytes);
  RUN_TEST(test_sha384_112bytes);
  RUN_TEST(test_sha512_112bytes);

  // Large input
  RUN_TEST(test_sha256_10000a);

  UNITY_END();
}

void loop() {}
