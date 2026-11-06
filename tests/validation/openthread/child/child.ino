/*
 * OpenThread Validation Test -- Child (multi-DUT)
 *
 * Receives dataset + network info from pytest (from leader), joins the
 * network, and verifies the same network parameters on the child.
 */

#include <Arduino.h>

#if CONFIG_OPENTHREAD_ENABLED

#include <OThread.h>
#include <OThreadCLI.h>
#include <OThreadCLI_Util.h>
#include <unity.h>
#include "../ot_test_helpers.h"

#define OT_TIMEOUT_MS 30000

static String dataset_hex;
static ot_test_netinfo_t expected_netinfo;

void setUp(void) {
  ot_test_drain_cli();
}

void tearDown(void) {
  ot_test_drain_cli();
}

// ==================== Stack / CLI lifecycle ====================

void test_child_stack_running(void) {
  TEST_ASSERT_TRUE(OThread);
  TEST_ASSERT_TRUE(OThreadCLI);
  TEST_ASSERT_NOT_NULL(OThread.getInstance());
}

void test_child_cli_version(void) {
  TEST_ASSERT_TRUE(ot_test_cli_contains("version", "OPENTHREAD"));
}

void test_child_cli_detached_state(void) {
  TEST_ASSERT_TRUE(ot_test_cli_contains("state", "detached"));
}

void test_child_cli_util_exec(void) {
  char buf[OT_TEST_CLI_BUF_SIZE];
  TEST_ASSERT_TRUE(ot_test_run_cli("state", buf, sizeof(buf)));
  TEST_ASSERT_TRUE(strstr(buf, "detached") != nullptr);
}

// ==================== Join network ====================

void test_child_join_network(void) {
  String cmd = "dataset set active " + dataset_hex;
  TEST_ASSERT_TRUE(ot_test_run_cli(cmd.c_str(), nullptr, 0));
  TEST_ASSERT_TRUE(ot_test_run_cli("dataset commit active", nullptr, 0));
  TEST_ASSERT_TRUE(ot_test_run_cli("ifconfig up", nullptr, 0));
  TEST_ASSERT_TRUE(ot_test_run_cli("thread start", nullptr, 0));
  delay(2000);

  bool attached = ot_test_wait_cli_state("child", OT_TIMEOUT_MS) || ot_test_wait_cli_state("router", 5000);
  TEST_ASSERT_TRUE_MESSAGE(attached, "Child failed to attach to network");

  Serial.println("[CHILD] JOINED");
}

// ==================== Post-attach API (must match leader) ====================

void test_child_network_matches_leader(void) {
  ot_test_assert_netinfo_match(expected_netinfo);
}

void test_child_ot_role_attached(void) {
  ot_device_role_t role = OThread.otGetDeviceRole();
  TEST_ASSERT_TRUE(role == OT_ROLE_CHILD || role == OT_ROLE_ROUTER);
  TEST_ASSERT_TRUE(ot_test_cli_contains("state", OThread.otGetStringDeviceRole()));
}

void test_child_ot_network_getters(void) {
  ot_test_assert_network_getters_match_netinfo(expected_netinfo);
}

void test_child_ot_attached_addresses(void) {
  ot_test_assert_attached_addresses();
}

void test_child_ot_current_dataset(void) {
  ot_test_assert_current_dataset_matches_netinfo(expected_netinfo);
}

void test_child_cli_active_dataset_hex(void) {
  String active_hex = ot_test_extract_hex(ot_test_read_cli_response("dataset active -x", 15000));
  ot_test_assert_dataset_hex(active_hex);
  TEST_ASSERT_EQUAL_STRING_MESSAGE(dataset_hex.c_str(), active_hex.c_str(), "Active dataset differs from leader export");
}

void test_child_cli_util_network_fields(void) {
  ot_test_assert_cli_network_fields();
}

void test_child_cli_print_network_info(void) {
  ot_test_assert_cli_print_network_info();
}

// ==================== Setup ====================

static bool wait_for_serial_config(void) {
  bool have_dataset = false;
  bool have_netinfo = false;

  while (!have_dataset || !have_netinfo) {
    if (Serial.available()) {
      String line = Serial.readStringUntil('\n');
      line.trim();
      if (line.startsWith("DATASET ")) {
        dataset_hex = line.substring(8);
        dataset_hex.trim();
        ot_test_assert_dataset_hex(dataset_hex);
        have_dataset = true;
      } else if (ot_test_parse_netinfo_line(line, expected_netinfo)) {
        have_netinfo = true;
      }
    }
    delay(10);
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("[CHILD] Ready");
  TEST_ASSERT_TRUE(wait_for_serial_config());

  OThread.begin(false);
  OThreadCLI.setRxBufferSize(1024);
  OThreadCLI.setTxBufferSize(256);
  OThreadCLI.begin();
  delay(1000);

  UNITY_BEGIN();

  RUN_TEST(test_child_stack_running);
  RUN_TEST(test_child_cli_version);
  RUN_TEST(test_child_cli_detached_state);
  RUN_TEST(test_child_cli_util_exec);
  RUN_TEST(test_child_join_network);
  RUN_TEST(test_child_network_matches_leader);
  RUN_TEST(test_child_ot_role_attached);
  RUN_TEST(test_child_ot_network_getters);
  RUN_TEST(test_child_ot_attached_addresses);
  RUN_TEST(test_child_ot_current_dataset);
  RUN_TEST(test_child_cli_active_dataset_hex);
  RUN_TEST(test_child_cli_util_network_fields);
  RUN_TEST(test_child_cli_print_network_info);

  UNITY_END();

  Serial.println("[CHILD] DONE");
}

void loop() {}

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

  Serial.println("[CHILD] Ready");
  UNITY_BEGIN();
  RUN_TEST(test_ot_not_supported);
  UNITY_END();
  Serial.println("[CHILD] DONE");
}

void loop() {}
#endif
