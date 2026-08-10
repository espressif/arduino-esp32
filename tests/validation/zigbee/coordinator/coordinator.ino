/*
 * Zigbee Validation Test -- Coordinator (multi-DUT)
 *
 * Registers all Zigbee library endpoint types, runs ZigbeeCore API tests,
 * deep endpoint API tests, then forms a network for the end device to join.
 * ZCL interop (coordinator switch -> end device light) runs in loop() only
 * after pytest sends RUN_INTEROP (once the end device has joined).
 *
 * Zigbee.begin() is called only once per boot.
 *
 * Pin-to-pin: wireless (802.15.4 radio)
 * Runner: two_duts (C6/H2)
 * Requires: CONFIG_ZB_ENABLED=y, ZigbeeMode=zczr, PartitionScheme=zigbee_zczr
 */

#include <Arduino.h>

#if CONFIG_ZB_ENABLED

#include <Zigbee.h>
#include <unity.h>
#include "zigbee_endpoint_matrix.h"
#include "zigbee_endpoint_deep_tests.h"

#define ZB_TIMEOUT_MS      30000
#define ZB_INTEROP_BIND_MS 60000

enum ZbInteropState {
  ZB_INTEROP_IDLE,
  ZB_INTEROP_WAIT_BIND,
  ZB_INTEROP_LIGHT_ON,
  ZB_INTEROP_LIGHT_OFF,
  ZB_INTEROP_DONE
};

static ZbInteropState interop_state = ZB_INTEROP_IDLE;
static unsigned long interop_phase_ms = 0;
static unsigned long interop_bind_start_ms = 0;

static void arm_switch_interop(void) {
  interop_state = ZB_INTEROP_WAIT_BIND;
  interop_bind_start_ms = millis();
  Serial.println("[COORDINATOR] INTEROP_ARMED");
}

static void poll_serial_commands(void) {
  while (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "RUN_INTEROP") {
      arm_switch_interop();
    }
  }
}

void setUp(void) {}
void tearDown(void) {}

// ==================== Configuration (before begin) ====================

void test_coordinator_config(void) {
  Zigbee.setScanDuration(2);
  TEST_ASSERT_EQUAL(2, Zigbee.getScanDuration());

  Zigbee.setRxOnWhenIdle(true);
  TEST_ASSERT_TRUE(Zigbee.getRxOnWhenIdle());

  Zigbee.setDebugMode(true);
  TEST_ASSERT_TRUE(Zigbee.getDebugMode());

  Zigbee.allowMultiEndpointBinding(true);
  TEST_ASSERT_TRUE(Zigbee.allowMultiEndpointBinding());

  Zigbee.setTimeout(ZB_TIMEOUT_MS);
  Zigbee.setPrimaryChannelMask(ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK);

  esp_zb_radio_config_t radio = Zigbee.getRadioConfig();
  TEST_ASSERT_EQUAL(ZB_RADIO_MODE_NATIVE, radio.radio_mode);

  esp_zb_host_config_t host = Zigbee.getHostConfig();
  TEST_ASSERT_EQUAL(ZB_HOST_CONNECTION_MODE_NONE, host.host_connection_mode);
}

// ==================== All endpoint types (before begin) ====================

void test_coordinator_register_all_endpoints(void) {
  TEST_ASSERT_FALSE_MESSAGE(epTemp.setReporting(1, 300, 0.5f), "setReporting must not run before Zigbee.begin()");
  TEST_ASSERT_TRUE_MESSAGE(register_all_zigbee_endpoints(), "Failed to register one or more Zigbee endpoints");
}

// ==================== Stack init ====================

void test_coordinator_begin(void) {
  TEST_ASSERT_TRUE(Zigbee.begin(ZIGBEE_COORDINATOR, true));

  unsigned long start = millis();
  while (!Zigbee.started() && millis() - start < ZB_TIMEOUT_MS) {
    delay(100);
  }
  TEST_ASSERT_TRUE_MESSAGE(Zigbee.started(), "Coordinator failed to start stack");

  TEST_ASSERT_FALSE_MESSAGE(Zigbee.begin(ZIGBEE_COORDINATOR, true), "duplicate begin() must return false");
  TEST_ASSERT_TRUE_MESSAGE(Zigbee.started(), "stack must keep running after rejected begin()");
}

// ==================== stop / start ====================

void test_coordinator_stop_start(void) {
  TEST_ASSERT_TRUE_MESSAGE(Zigbee.started(), "stack must be running");

  Zigbee.stop();
  TEST_ASSERT_FALSE_MESSAGE(Zigbee.started(), "stop() must clear started()");

  Zigbee.start();

  unsigned long start = millis();
  while (!Zigbee.started() && millis() - start < 5000) {
    delay(100);
  }
  TEST_ASSERT_TRUE_MESSAGE(Zigbee.started(), "start() must resume the stack");
}

// ==================== Role / network ====================

void test_coordinator_role(void) {
  TEST_ASSERT_EQUAL(ZIGBEE_COORDINATOR, Zigbee.getRole());
}

void test_coordinator_form_network(void) {
  TEST_ASSERT_TRUE_MESSAGE(Zigbee.started(), "Zigbee stack must already be running");

  Zigbee.openNetwork(180);

  unsigned long start = millis();
  while (!Zigbee.connected() && millis() - start < ZB_TIMEOUT_MS) {
    delay(100);
  }
  TEST_ASSERT_TRUE_MESSAGE(Zigbee.connected(), "Coordinator failed to form network");

  delay(2000);
  Serial.println("[COORDINATOR] NETWORK_READY");
}

// ==================== Deep endpoint APIs (after network up) ====================

void test_coordinator_endpoint_deep_tests(void) {
  TEST_ASSERT_TRUE_MESSAGE(Zigbee.connected(), "network required for attribute updates");
  run_all_zigbee_endpoint_deep_tests();
  Serial.println("[COORDINATOR] API_TESTS_DONE");
}

// ==================== Setup ====================

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("[COORDINATOR] Ready");

  UNITY_BEGIN();

  RUN_TEST(test_coordinator_config);
  RUN_TEST(test_coordinator_register_all_endpoints);
  RUN_TEST(test_coordinator_begin);
  RUN_TEST(test_coordinator_stop_start);
  RUN_TEST(test_coordinator_role);
  RUN_TEST(test_coordinator_form_network);
  RUN_TEST(test_coordinator_endpoint_deep_tests);

  UNITY_END();

  Serial.println("[COORDINATOR] INTEROP_READY");
  Serial.println("[COORDINATOR] DONE");
}

void loop() {
  poll_serial_commands();

  if (interop_state == ZB_INTEROP_IDLE || interop_state == ZB_INTEROP_DONE) {
    delay(50);
    return;
  }

  switch (interop_state) {
    case ZB_INTEROP_WAIT_BIND:
      if (epSwitch.bound()) {
        epSwitch.lightOn();
        interop_phase_ms = millis();
        interop_state = ZB_INTEROP_LIGHT_ON;
      } else if (millis() - interop_bind_start_ms > ZB_INTEROP_BIND_MS) {
        Serial.println("[COORDINATOR] SWITCH_INTEROP_FAILED");
        interop_state = ZB_INTEROP_DONE;
      }
      break;

    case ZB_INTEROP_LIGHT_ON:
      if (millis() - interop_phase_ms >= 2000) {
        epSwitch.lightOff();
        interop_phase_ms = millis();
        interop_state = ZB_INTEROP_LIGHT_OFF;
      }
      break;

    case ZB_INTEROP_LIGHT_OFF:
      if (millis() - interop_phase_ms >= 1000) {
        Serial.println("[COORDINATOR] SWITCH_INTEROP_DONE");
        interop_state = ZB_INTEROP_DONE;
      }
      break;

    default: break;
  }

  delay(50);
}

#else  // !CONFIG_ZB_ENABLED

#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

void test_zigbee_not_supported(void) {
  TEST_IGNORE_MESSAGE("Zigbee not enabled in sdkconfig");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("[COORDINATOR] Ready");
  UNITY_BEGIN();
  RUN_TEST(test_zigbee_not_supported);
  UNITY_END();
  Serial.println("[COORDINATOR] DONE");
}

void loop() {}
#endif
