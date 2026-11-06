/*
 * Shared helpers for OpenThread validation sketches (leader / child).
 * Included from leader/ and child/ sketches as ../ot_test_helpers.h.
 */

#pragma once

#include <unity.h>
#include <OThread.h>
#include <OThreadCLI.h>
#include <OThreadCLI_Util.h>
#include <StreamString.h>

#define OT_TEST_CLI_BUF_SIZE   512
#define OT_TEST_EXT_PAN_ID_LEN 8

struct ot_test_netinfo_t {
  String name;
  uint8_t channel;
  uint16_t pan_id;
  uint8_t ext_pan_id[OT_TEST_EXT_PAN_ID_LEN];
  bool valid;
};

inline void ot_test_drain_cli(void) {
  delay(50);
  while (OThreadCLI.available() > 0) {
    OThreadCLI.read();
  }
}

inline bool ot_test_run_cli(const char *cmd, char *out, size_t out_len, uint32_t timeout_ms = 5000) {
  (void)out_len;
  ot_test_drain_cli();
  if (out != nullptr && out_len > 0) {
    out[0] = '\0';
    return otGetRespCmd(cmd, out, timeout_ms) && out[0] != '\0';
  }
  return otGetRespCmd(cmd, nullptr, timeout_ms);
}

inline bool ot_test_cli_contains(const char *cmd, const char *needle) {
  char buf[OT_TEST_CLI_BUF_SIZE];
  if (!ot_test_run_cli(cmd, buf, sizeof(buf))) {
    return false;
  }
  return strstr(buf, needle) != nullptr;
}

inline bool ot_test_wait_cli_state(const char *role_substr, uint32_t timeout_ms) {
  unsigned long deadline = millis() + timeout_ms;
  while (millis() < deadline) {
    if (ot_test_cli_contains("state", role_substr)) {
      return true;
    }
    delay(500);
  }
  return false;
}

inline String ot_test_hex_from_line(const String &line) {
  String hex;
  hex.reserve(line.length());
  for (size_t i = 0; i < line.length(); i++) {
    char c = line.charAt(i);
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
      hex += c;
    }
  }
  return hex;
}

inline String ot_test_extract_hex(const String &input) {
  String hex;
  int start = 0;
  while (start < (int)input.length()) {
    int end = input.indexOf('\n', start);
    if (end < 0) {
      end = input.length();
    }
    String line = input.substring(start, end);
    line.trim();
    if (line.length() > 0 && !line.startsWith("Done") && !line.startsWith("Error")) {
      hex += ot_test_hex_from_line(line);
    }
    if (end >= (int)input.length()) {
      break;
    }
    start = end + 1;
  }
  return hex;
}

inline String ot_test_read_cli_response(const char *cmd, uint32_t timeout_ms) {
  ot_test_drain_cli();
  if (cmd != nullptr) {
    OThreadCLI.println(cmd);
  }

  String response;
  unsigned long deadline = millis() + timeout_ms;
  while (millis() < deadline) {
    if (OThreadCLI.available()) {
      String line = OThreadCLI.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) {
        continue;
      }
      if (line.startsWith("Done")) {
        break;
      }
      if (line.startsWith("Error")) {
        return "";
      }
      if (response.length() > 0) {
        response += '\n';
      }
      response += line;
    }
    delay(10);
  }
  return response;
}

inline ot_test_netinfo_t ot_test_collect_netinfo(void) {
  ot_test_netinfo_t info = {};
  info.name = OThread.getNetworkName();
  info.channel = OThread.getChannel();
  info.pan_id = OThread.getPanId();
  const uint8_t *ext_pan = OThread.getExtendedPanId();
  if (ext_pan != nullptr) {
    memcpy(info.ext_pan_id, ext_pan, OT_TEST_EXT_PAN_ID_LEN);
  }
  info.valid = info.name.length() > 0 && info.channel > 0 && info.pan_id != 0;
  return info;
}

inline void ot_test_print_netinfo(const char *prefix) {
  ot_test_netinfo_t info = ot_test_collect_netinfo();
  char ext_hex[(OT_TEST_EXT_PAN_ID_LEN * 2) + 1];
  for (int i = 0; i < OT_TEST_EXT_PAN_ID_LEN; i++) {
    sprintf(ext_hex + (i * 2), "%02x", info.ext_pan_id[i]);
  }
  Serial.printf("%s NETINFO name=%s channel=%u panid=0x%04x extpanid=%s\n", prefix, info.name.c_str(), info.channel, info.pan_id, ext_hex);
}

inline bool ot_test_parse_netinfo_line(const String &line, ot_test_netinfo_t &out) {
  if (!line.startsWith("NETINFO ")) {
    return false;
  }

  int name_pos = line.indexOf("name=");
  int channel_pos = line.indexOf(" channel=");
  int panid_pos = line.indexOf(" panid=");
  int extpanid_pos = line.indexOf(" extpanid=");
  if (name_pos < 0 || channel_pos < 0 || panid_pos < 0 || extpanid_pos < 0) {
    return false;
  }

  out.name = line.substring(name_pos + 5, channel_pos);
  out.channel = (uint8_t)line.substring(channel_pos + 9, panid_pos).toInt();
  out.pan_id = (uint16_t)strtoul(line.substring(panid_pos + 7, extpanid_pos).c_str(), nullptr, 0);

  String ext_hex = line.substring(extpanid_pos + 10);
  ext_hex.trim();
  if (ext_hex.length() != OT_TEST_EXT_PAN_ID_LEN * 2) {
    return false;
  }
  for (int i = 0; i < OT_TEST_EXT_PAN_ID_LEN; i++) {
    char byte_str[3] = {ext_hex.charAt(i * 2), ext_hex.charAt((i * 2) + 1), '\0'};
    out.ext_pan_id[i] = (uint8_t)strtoul(byte_str, nullptr, 16);
  }

  out.valid = out.name.length() > 0 && out.channel > 0 && out.pan_id != 0;
  return out.valid;
}

inline bool ot_test_ext_pan_equal(const uint8_t *a, const uint8_t *b) {
  for (int i = 0; i < OT_TEST_EXT_PAN_ID_LEN; i++) {
    if (a[i] != b[i]) {
      return false;
    }
  }
  return true;
}

inline void ot_test_assert_netinfo_match(const ot_test_netinfo_t &expected) {
  ot_test_netinfo_t actual = ot_test_collect_netinfo();
  TEST_ASSERT_TRUE_MESSAGE(expected.valid, "Expected network info is invalid");
  TEST_ASSERT_TRUE_MESSAGE(actual.valid, "Actual network info is invalid");
  TEST_ASSERT_EQUAL_STRING_MESSAGE(expected.name.c_str(), actual.name.c_str(), "Network name mismatch");
  TEST_ASSERT_EQUAL_MESSAGE(expected.channel, actual.channel, "Channel mismatch");
  TEST_ASSERT_EQUAL_MESSAGE(expected.pan_id, actual.pan_id, "PAN ID mismatch");
  TEST_ASSERT_TRUE_MESSAGE(ot_test_ext_pan_equal(expected.ext_pan_id, actual.ext_pan_id), "Extended PAN ID mismatch");
}

inline void ot_test_assert_network_getters_match_netinfo(const ot_test_netinfo_t &expected) {
  ot_test_assert_netinfo_match(expected);

  const uint8_t *net_key = OThread.getNetworkKey();
  TEST_ASSERT_NOT_NULL(net_key);

  TEST_ASSERT_TRUE(OThread.getInstance() != nullptr);
}

inline void ot_test_assert_network_getters(void) {
  ot_test_assert_network_getters_match_netinfo(ot_test_collect_netinfo());
}

inline void ot_test_assert_attached_addresses(void) {
  uint16_t rloc16 = OThread.getRloc16();
  TEST_ASSERT_NOT_EQUAL(0, rloc16);

  IPAddress mesh = OThread.getMeshLocalEid();
  TEST_ASSERT_FALSE(mesh == IPAddress());

  const otMeshLocalPrefix *prefix = OThread.getMeshLocalPrefix();
  TEST_ASSERT_NOT_NULL(prefix);

  TEST_ASSERT_GREATER_THAN(0, OThread.getUnicastAddressCount());
  TEST_ASSERT_GREATER_THAN(0, OThread.getMulticastAddressCount());

  std::vector<IPAddress> uni = OThread.getAllUnicastAddresses();
  TEST_ASSERT_GREATER_THAN(0, uni.size());
  std::vector<IPAddress> multi = OThread.getAllMulticastAddresses();
  TEST_ASSERT_GREATER_THAN(0, multi.size());

  OThread.clearAllAddressCache();
  OThread.clearUnicastAddressCache();
  OThread.clearMulticastAddressCache();
}

inline void ot_test_assert_current_dataset_matches_netinfo(const ot_test_netinfo_t &expected) {
  const DataSet &active = OThread.getCurrentDataSet();
  TEST_ASSERT_NOT_NULL(active.getNetworkName());
  TEST_ASSERT_EQUAL_STRING(expected.name.c_str(), active.getNetworkName());
  TEST_ASSERT_EQUAL(expected.channel, active.getChannel());
  TEST_ASSERT_EQUAL(expected.pan_id, active.getPanId());
  TEST_ASSERT_NOT_NULL(active.getExtendedPanId());
  TEST_ASSERT_TRUE(ot_test_ext_pan_equal(expected.ext_pan_id, active.getExtendedPanId()));
  TEST_ASSERT_NOT_NULL(active.getNetworkKey());
}

inline void ot_test_assert_cli_network_fields(void) {
  char buf[OT_TEST_CLI_BUF_SIZE];

  TEST_ASSERT_TRUE(ot_test_run_cli("networkname", buf, sizeof(buf)));
  TEST_ASSERT_NOT_NULL(strstr(buf, OThread.getNetworkName().c_str()));

  TEST_ASSERT_TRUE(ot_test_run_cli("channel", buf, sizeof(buf)));

  TEST_ASSERT_TRUE(ot_test_run_cli("panid", buf, sizeof(buf)));

  TEST_ASSERT_TRUE(ot_test_run_cli("extpanid", buf, sizeof(buf)));
}

inline void ot_test_assert_cli_print_network_info(void) {
  StreamString out;
  TEST_ASSERT_TRUE(otPrintRespCLI("state", out, 5000));
  TEST_ASSERT_TRUE(out.indexOf(OThread.otGetStringDeviceRole()) >= 0);

  out = StreamString();
  OThread.otPrintNetworkInformation(out);
  TEST_ASSERT_TRUE(out.indexOf("Network Name:") >= 0);
  TEST_ASSERT_TRUE(out.indexOf(OThread.getNetworkName()) >= 0);
  TEST_ASSERT_TRUE(out.indexOf("Channel:") >= 0);
  TEST_ASSERT_TRUE(out.indexOf("PAN ID:") >= 0);
}

inline void ot_test_assert_dataset_hex(const String &hex) {
  TEST_ASSERT_TRUE_MESSAGE(hex.length() >= 100, "Dataset hex too short");
  TEST_ASSERT_EQUAL_MESSAGE(0, hex.length() % 2, "Dataset hex length must be even");
}
