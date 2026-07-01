/*
 * Zigbee Validation Test -- End Device (multi-DUT)
 *
 * ZigbeeCore, ZigbeeLight, and ZigbeeTempSensor on an end device. Joins the
 * coordinator network after pytest sends START. ZCL interop (coordinator switch
 * -> ED light) runs on RUN_INTEROP.
 *
 * Zigbee.begin() is called only once per boot.
 *
 * Pin-to-pin: wireless (802.15.4 radio)
 * Runner: two_duts (C6/H2 devices)
 * Requires: CONFIG_ZB_ENABLED=y, ZigbeeMode=ed, PartitionScheme=zigbee
 */

#include <Arduino.h>

#if CONFIG_ZB_ENABLED

#include <Zigbee.h>
#include <unity.h>

#define ZB_TIMEOUT_MS      30000
#define ZB_INTEROP_WAIT_MS 60000

#define ZB_EP_LIGHT 1
#define ZB_EP_TEMP  2

static ZigbeeLight zbLight(ZB_EP_LIGHT);
static ZigbeeTempSensor zbTemp(ZB_EP_TEMP);

static volatile bool light_cb_called = false;
static volatile bool light_cb_state = false;
static volatile bool remote_light_on_seen = false;
static volatile bool remote_light_off_seen = false;

static void onLightChange(bool state) {
  light_cb_called = true;
  light_cb_state = state;
}

static void onRemoteLightChange(bool state) {
  if (state) {
    remote_light_on_seen = true;
    Serial.println("[ENDDEVICE] REMOTE_LIGHT_ON");
  } else {
    remote_light_off_seen = true;
    Serial.println("[ENDDEVICE] REMOTE_LIGHT_OFF");
  }
}

static void wait_for_serial_command(const char *expected) {
  String cmd;
  while (cmd != expected) {
    if (Serial.available()) {
      cmd = Serial.readStringUntil('\n');
      cmd.trim();
    }
    delay(10);
  }
}

void setUp(void) {}
void tearDown(void) {}

// ==================== Configuration (before begin) ====================

void test_end_device_config(void) {
  Zigbee.setScanDuration(3);
  TEST_ASSERT_EQUAL(3, Zigbee.getScanDuration());

  Zigbee.setRxOnWhenIdle(true);
  TEST_ASSERT_TRUE(Zigbee.getRxOnWhenIdle());

  Zigbee.setDebugMode(false);
  TEST_ASSERT_FALSE(Zigbee.getDebugMode());

  Zigbee.allowMultiEndpointBinding(false);
  TEST_ASSERT_FALSE(Zigbee.allowMultiEndpointBinding());

  Zigbee.setTimeout(ZB_TIMEOUT_MS);

  esp_zb_radio_config_t radio = Zigbee.getRadioConfig();
  TEST_ASSERT_EQUAL(ZB_RADIO_MODE_NATIVE, radio.radio_mode);
}

// ==================== Endpoints (before begin) ====================

void test_end_device_add_endpoints(void) {
  zbLight.setManufacturerAndModel("Espressif", "ZBValidationED");
  zbLight.onLightChange(onRemoteLightChange);
  TEST_ASSERT_TRUE(Zigbee.addEndpoint(&zbLight));

  zbTemp.setMinMaxValue(-10.0f, 50.0f);
  zbTemp.setTolerance(0.5f);
  zbTemp.setManufacturerAndModel("Espressif", "ZBValEDTemp");
  TEST_ASSERT_TRUE(Zigbee.addEndpoint(&zbTemp));
}

// ==================== Stack init ====================

void test_end_device_begin(void) {
  TEST_ASSERT_TRUE(Zigbee.begin(ZIGBEE_END_DEVICE, true));

  unsigned long start = millis();
  while (!Zigbee.started() && millis() - start < ZB_TIMEOUT_MS) {
    delay(100);
  }
  TEST_ASSERT_TRUE_MESSAGE(Zigbee.started(), "End device failed to start stack");

  TEST_ASSERT_FALSE_MESSAGE(Zigbee.begin(ZIGBEE_END_DEVICE, true), "duplicate begin() must return false");
  TEST_ASSERT_TRUE_MESSAGE(Zigbee.started(), "stack must keep running after rejected begin()");
}

// ==================== stop / start ====================

void test_end_device_stop_start(void) {
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

// ==================== Role ====================

void test_end_device_role(void) {
  TEST_ASSERT_EQUAL(ZIGBEE_END_DEVICE, Zigbee.getRole());
}

// ==================== Join network ====================

void test_end_device_join_network(void) {
  TEST_ASSERT_TRUE_MESSAGE(Zigbee.started(), "Zigbee stack must already be running");

  unsigned long join_start = millis();
  while (!Zigbee.connected() && millis() - join_start < ZB_TIMEOUT_MS) {
    delay(100);
  }
  TEST_ASSERT_TRUE_MESSAGE(Zigbee.connected(), "End device failed to join network");

  Serial.println("[ENDDEVICE] JOINED");
}

// ==================== Temp sensor (after join) ====================

void test_end_device_temp_sensor(void) {
  TEST_ASSERT_TRUE_MESSAGE(Zigbee.connected(), "must be joined before sensor tests");

  TEST_ASSERT_TRUE(zbTemp.setReporting(1, 300, 0.5f));
  TEST_ASSERT_TRUE(zbTemp.setTemperature(22.5f));
  TEST_ASSERT_TRUE(zbTemp.reportTemperature());
}

// ==================== ZCL interop (coordinator switch -> ED light) ====================

void test_end_device_zcl_interop(void) {
  TEST_ASSERT_TRUE_MESSAGE(Zigbee.connected(), "must be joined before interop");

  remote_light_on_seen = false;
  remote_light_off_seen = false;
  zbLight.onLightChange(onRemoteLightChange);

  wait_for_serial_command("RUN_INTEROP");

  unsigned long interop_start = millis();
  while ((!remote_light_on_seen || !remote_light_off_seen) && millis() - interop_start < ZB_INTEROP_WAIT_MS) {
    delay(50);
  }

  TEST_ASSERT_TRUE_MESSAGE(remote_light_on_seen, "coordinator did not turn light on");
  TEST_ASSERT_TRUE_MESSAGE(remote_light_off_seen, "coordinator did not turn light off");

  Serial.println("[ENDDEVICE] SWITCH_INTEROP_DONE");
}

// ==================== ZigbeeLight (local API) ====================

void test_end_device_light(void) {
  TEST_ASSERT_TRUE_MESSAGE(Zigbee.connected(), "must be joined before light tests");

  light_cb_called = false;
  zbLight.onLightChange(onLightChange);

  TEST_ASSERT_TRUE(zbLight.setLight(true));
  TEST_ASSERT_TRUE(zbLight.getLightState());
  delay(50);
  TEST_ASSERT_TRUE_MESSAGE(light_cb_called, "onLightChange callback not fired");
  TEST_ASSERT_TRUE_MESSAGE(light_cb_state, "callback state mismatch");

  TEST_ASSERT_TRUE(zbLight.setLight(false));
  TEST_ASSERT_FALSE(zbLight.getLightState());

  TEST_ASSERT_TRUE(zbLight.setLight(true));
  TEST_ASSERT_TRUE(zbLight.setLight(false));
  zbLight.restoreLight();

  zbLight.onLightChange(onRemoteLightChange);
}

// ==================== Setup ====================

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("[ENDDEVICE] Ready");

  wait_for_serial_command("START");

  UNITY_BEGIN();

  RUN_TEST(test_end_device_config);
  RUN_TEST(test_end_device_add_endpoints);
  RUN_TEST(test_end_device_begin);
  RUN_TEST(test_end_device_stop_start);
  RUN_TEST(test_end_device_role);
  RUN_TEST(test_end_device_join_network);
  RUN_TEST(test_end_device_temp_sensor);
  RUN_TEST(test_end_device_zcl_interop);
  RUN_TEST(test_end_device_light);

  UNITY_END();

  Serial.println("[ENDDEVICE] DONE");
}

void loop() {}

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

  Serial.println("[ENDDEVICE] Ready");
  UNITY_BEGIN();
  RUN_TEST(test_zigbee_not_supported);
  UNITY_END();
  Serial.println("[ENDDEVICE] DONE");
}

void loop() {}
#endif
