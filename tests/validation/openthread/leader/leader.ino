/*
 * OpenThread Validation Test -- Leader (multi-DUT)
 *
 * Exercises OThread / OThreadCLI / OThreadCLI_Util APIs, forms a network,
 * exports dataset + network info for the child via pytest.
 */

#include <Arduino.h>

#if CONFIG_OPENTHREAD_ENABLED

#include <OThread.h>
#include <OThreadCLI.h>
#include <OThreadCLI_Util.h>
#include <unity.h>
#include "../ot_test_helpers.h"

#define OT_TIMEOUT_MS 30000

static String leader_dataset_hex;
static ot_test_netinfo_t leader_netinfo;

void setUp(void) {
  ot_test_drain_cli();
}

void tearDown(void) {
  ot_test_drain_cli();
}

// ==================== Stack / CLI lifecycle ====================

void test_leader_stack_running(void) {
  TEST_ASSERT_TRUE(OThread);
  TEST_ASSERT_TRUE(OThreadCLI);
  TEST_ASSERT_NOT_NULL(OThread.getInstance());
}

void test_leader_cli_version(void) {
  TEST_ASSERT_TRUE(ot_test_cli_contains("version", "OPENTHREAD"));
}

void test_leader_cli_detached_state(void) {
  TEST_ASSERT_TRUE(ot_test_cli_contains("state", "detached"));
}

void test_leader_cli_util_exec(void) {
  char buf[OT_TEST_CLI_BUF_SIZE];
  TEST_ASSERT_TRUE(ot_test_run_cli("state", buf, sizeof(buf)));
  TEST_ASSERT_TRUE(strstr(buf, "detached") != nullptr);
}

// ==================== Network formation ====================

void test_leader_form_network(void) {
  TEST_ASSERT_TRUE(ot_test_run_cli("dataset init new", nullptr, 0));
  TEST_ASSERT_TRUE(ot_test_run_cli("dataset commit active", nullptr, 0));
  TEST_ASSERT_TRUE(ot_test_run_cli("ifconfig up", nullptr, 0));
  TEST_ASSERT_TRUE(ot_test_run_cli("thread start", nullptr, 0));
  delay(1000);

  TEST_ASSERT_TRUE_MESSAGE(ot_test_wait_cli_state("leader", OT_TIMEOUT_MS), "Failed to become leader");

  leader_dataset_hex = ot_test_extract_hex(ot_test_read_cli_response("dataset active -x", 15000));
  ot_test_assert_dataset_hex(leader_dataset_hex);

  leader_netinfo = ot_test_collect_netinfo();
  TEST_ASSERT_TRUE(leader_netinfo.valid);
}

// ==================== Post-attach API (leader role) ====================

void test_leader_ot_role(void) {
  TEST_ASSERT_EQUAL(OT_ROLE_LEADER, OThread.otGetDeviceRole());
  TEST_ASSERT_EQUAL_STRING("leader", OThread.otGetStringDeviceRole());
  TEST_ASSERT_TRUE(ot_test_cli_contains("state", "leader"));
}

void test_leader_ot_network_getters(void) {
  ot_test_assert_network_getters_match_netinfo(leader_netinfo);
}

void test_leader_ot_leader_addresses(void) {
  ot_test_assert_attached_addresses();

  IPAddress leader_rloc = OThread.getLeaderRloc();
  TEST_ASSERT_FALSE(leader_rloc == IPAddress());

  IPAddress rloc = OThread.getRloc();
  TEST_ASSERT_FALSE(rloc == IPAddress());
}

void test_leader_ot_current_dataset(void) {
  ot_test_assert_current_dataset_matches_netinfo(leader_netinfo);
}

void test_leader_dataset_class(void) {
  DataSet ds;
  ds.initNew();
  ds.setNetworkName("OT-Validation");
  ds.setChannel(15);
  ds.setPanId(0x1234);

  uint8_t key[16];
  memset(key, 0xAB, sizeof(key));
  ds.setNetworkKey(key);

  uint8_t ext_pan[8];
  memset(ext_pan, 0xCD, sizeof(ext_pan));
  ds.setExtendedPanId(ext_pan);

  TEST_ASSERT_EQUAL_STRING("OT-Validation", ds.getNetworkName());
  TEST_ASSERT_EQUAL(15, ds.getChannel());
  TEST_ASSERT_EQUAL(0x1234, ds.getPanId());
  TEST_ASSERT_NOT_NULL(ds.getNetworkKey());
  TEST_ASSERT_NOT_NULL(ds.getExtendedPanId());
}

void test_leader_cli_util_network_fields(void) {
  ot_test_assert_cli_network_fields();
}

void test_leader_cli_print_network_info(void) {
  ot_test_assert_cli_print_network_info();
}

// ==================== Setup ====================

static void wait_for_go(void) {
  while (true) {
    if (Serial.available()) {
      String cmd = Serial.readStringUntil('\n');
      cmd.trim();
      if (cmd == "GO") {
        return;
      }
    }
    delay(10);
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  OThread.begin(false);
  // Queue sizes must be set before begin(); resizing later breaks ot_cli_loop.
  OThreadCLI.setRxBufferSize(1024);
  OThreadCLI.setTxBufferSize(256);
  OThreadCLI.begin();
  delay(1000);

  Serial.println("[LEADER] Ready");
  wait_for_go();

  UNITY_BEGIN();

  RUN_TEST(test_leader_stack_running);
  RUN_TEST(test_leader_cli_version);
  RUN_TEST(test_leader_cli_detached_state);
  RUN_TEST(test_leader_cli_util_exec);
  RUN_TEST(test_leader_form_network);
  RUN_TEST(test_leader_ot_role);
  RUN_TEST(test_leader_ot_network_getters);
  RUN_TEST(test_leader_ot_leader_addresses);
  RUN_TEST(test_leader_ot_current_dataset);
  RUN_TEST(test_leader_dataset_class);
  RUN_TEST(test_leader_cli_util_network_fields);
  RUN_TEST(test_leader_cli_print_network_info);

  UNITY_END();

  ot_test_drain_cli();

  if (leader_dataset_hex.length() > 0 && leader_netinfo.valid) {
    Serial.print("[LEADER] DATASET=");
    Serial.println(leader_dataset_hex);
    Serial.flush();
    ot_test_print_netinfo("[LEADER]");
    Serial.flush();
    Serial.println("[LEADER] NETWORK_READY");
  } else {
    Serial.println("[LEADER] EXPORT_FAILED");
  }
  Serial.flush();
  Serial.println("[LEADER] DONE");
}

void loop() {
  delay(100);
}

#else  // !CONFIG_OPENTHREAD_ENABLED

#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

void test_ot_not_supported(void) {
  TEST_IGNORE_MESSAGE("OpenThread not enabled in sdkconfig");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("[LEADER] Ready");
  UNITY_BEGIN();
  RUN_TEST(test_ot_not_supported);
  UNITY_END();
  Serial.println("[LEADER] DONE");
}

void loop() {}
#endif
