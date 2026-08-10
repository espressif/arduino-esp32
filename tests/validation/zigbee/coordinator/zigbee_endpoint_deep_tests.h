/*
 * Deep endpoint API tests for the Zigbee coordinator validation suite.
 * Exercises callbacks, getter round-trips, reporting, and multi-value paths
 * (similar depth to Matter phase-0 smoke tests, but on-device without a hub).
 */

#pragma once

#if CONFIG_ZB_ENABLED

#include <unity.h>
#include "zigbee_endpoint_matrix.h"

// --- callback state shared across tests ---

static volatile bool light_cb_called = false;
static volatile bool light_cb_state = false;

static volatile bool dimmable_cb_called = false;
static volatile bool dimmable_cb_state = false;
static volatile uint8_t dimmable_cb_level = 0;

static volatile bool color_rgb_cb_called = false;
static volatile uint8_t color_rgb_r = 0;
static volatile uint8_t color_rgb_g = 0;
static volatile uint8_t color_rgb_b = 0;

static volatile bool outlet_cb_called = false;
static volatile bool outlet_cb_state = false;

static volatile bool analog_out_cb_called = false;
static volatile float analog_out_cb_value = 0.0f;

static volatile bool binary_out_cb_called = false;
static volatile bool binary_out_cb_state = false;

static volatile bool multistate_out_cb_called = false;
static volatile uint16_t multistate_out_cb_value = 0;

static void onLightCb(bool state) {
  light_cb_called = true;
  light_cb_state = state;
}

static void onDimmableCb(bool state, uint8_t level) {
  dimmable_cb_called = true;
  dimmable_cb_state = state;
  dimmable_cb_level = level;
}

static void onColorRgbCb(bool state, uint8_t red, uint8_t green, uint8_t blue, uint8_t level) {
  (void)state;
  (void)level;
  color_rgb_cb_called = true;
  color_rgb_r = red;
  color_rgb_g = green;
  color_rgb_b = blue;
}

static void onOutletCb(bool state) {
  outlet_cb_called = true;
  outlet_cb_state = state;
}

static void onAnalogOutCb(float value) {
  analog_out_cb_called = true;
  analog_out_cb_value = value;
}

static void onBinaryOutCb(bool value) {
  binary_out_cb_called = true;
  binary_out_cb_state = value;
}

static void onMultistateOutCb(uint16_t value) {
  multistate_out_cb_called = true;
  multistate_out_cb_value = value;
}

// ==================== Lights ====================

static void test_light_onoff_roundtrip(void) {
  TEST_ASSERT_TRUE(epLight.setLight(false));
  TEST_ASSERT_FALSE(epLight.getLightState());

  TEST_ASSERT_TRUE(epLight.setLight(true));
  TEST_ASSERT_TRUE(epLight.getLightState());

  TEST_ASSERT_TRUE(epLight.setLight(false));
  TEST_ASSERT_FALSE(epLight.getLightState());
}

static void test_light_callback_and_restore(void) {
  light_cb_called = false;
  epLight.onLightChange(onLightCb);

  TEST_ASSERT_TRUE(epLight.setLight(true));
  delay(50);
  TEST_ASSERT_TRUE_MESSAGE(light_cb_called, "onLightChange not fired");
  TEST_ASSERT_TRUE_MESSAGE(light_cb_state, "callback state mismatch");

  light_cb_called = false;
  TEST_ASSERT_TRUE(epLight.setLight(false));
  delay(50);
  TEST_ASSERT_TRUE_MESSAGE(light_cb_called, "onLightChange not fired on off");
  TEST_ASSERT_FALSE_MESSAGE(light_cb_state, "callback state should be off");

  epLight.restoreLight();
}

static void test_dimmable_levels(void) {
  epDimmable.onLightChange(onDimmableCb);

  TEST_ASSERT_TRUE(epDimmable.setLight(false, 0));
  TEST_ASSERT_FALSE(epDimmable.getLightState());
  TEST_ASSERT_EQUAL(0, epDimmable.getLightLevel());

  TEST_ASSERT_TRUE(epDimmable.setLight(true, 128));
  TEST_ASSERT_TRUE(epDimmable.getLightState());
  TEST_ASSERT_EQUAL(128, epDimmable.getLightLevel());
  delay(50);
  TEST_ASSERT_TRUE_MESSAGE(dimmable_cb_called, "dimmable callback not fired");
  TEST_ASSERT_EQUAL(128, dimmable_cb_level);

  TEST_ASSERT_TRUE(epDimmable.setLightLevel(255));
  TEST_ASSERT_EQUAL(255, epDimmable.getLightLevel());

  TEST_ASSERT_TRUE(epDimmable.setLightState(false));
  TEST_ASSERT_FALSE(epDimmable.getLightState());
  epDimmable.restoreLight();
}

static void test_color_light_rgb_and_level(void) {
  epColorDim.onLightChangeRgb(onColorRgbCb);

  TEST_ASSERT_TRUE(epColorDim.setLightState(true));
  TEST_ASSERT_TRUE(epColorDim.getLightState());

  TEST_ASSERT_TRUE(epColorDim.setLightLevel(200));
  TEST_ASSERT_EQUAL(200, epColorDim.getLightLevel());

  TEST_ASSERT_TRUE(epColorDim.setLightColor(10, 20, 30));
  TEST_ASSERT_EQUAL(10, epColorDim.getLightRed());
  TEST_ASSERT_EQUAL(20, epColorDim.getLightGreen());
  TEST_ASSERT_EQUAL(30, epColorDim.getLightBlue());
  delay(50);
  TEST_ASSERT_TRUE_MESSAGE(color_rgb_cb_called, "RGB callback not fired");

  espRgbColor_t rgb = epColorDim.getLightColor();
  TEST_ASSERT_EQUAL(10, rgb.r);
  TEST_ASSERT_EQUAL(20, rgb.g);
  TEST_ASSERT_EQUAL(30, rgb.b);

  TEST_ASSERT_TRUE(epColorDim.setLightColorTemperature(250));
  TEST_ASSERT_EQUAL(250, epColorDim.getLightColorTemperature());

  TEST_ASSERT_GREATER_THAN(0, epColorDim.getLightColorCapabilities());
  epColorDim.restoreLight();
}

static void test_power_outlet(void) {
  outlet_cb_called = false;
  epOutlet.onPowerOutletChange(onOutletCb);

  TEST_ASSERT_TRUE(epOutlet.setState(true));
  TEST_ASSERT_TRUE(epOutlet.getPowerOutletState());
  delay(50);
  TEST_ASSERT_TRUE_MESSAGE(outlet_cb_called, "outlet callback not fired");
  TEST_ASSERT_TRUE(outlet_cb_state);

  TEST_ASSERT_TRUE(epOutlet.setState(false));
  TEST_ASSERT_FALSE(epOutlet.getPowerOutletState());
  epOutlet.restoreState();
}

// ==================== Actuators ====================

static void test_fan_mode_sequence(void) {
  TEST_ASSERT_TRUE(epFan.setFanModeSequence(FAN_MODE_SEQUENCE_LOW_MED_HIGH_AUTO));
  TEST_ASSERT_EQUAL(FAN_MODE_SEQUENCE_LOW_MED_HIGH_AUTO, epFan.getFanModeSequence());
  TEST_ASSERT_EQUAL(FAN_MODE_OFF, epFan.getFanMode());
}

static void test_window_covering(void) {
  TEST_ASSERT_TRUE(epCovering.setCoveringType(BLIND_LIFT_AND_TILT));
  TEST_ASSERT_TRUE(epCovering.setLimits(0, 100, 0, 90));
  TEST_ASSERT_TRUE(epCovering.setLiftPercentage(0));
  TEST_ASSERT_TRUE(epCovering.setLiftPercentage(50));
  TEST_ASSERT_TRUE(epCovering.setLiftPercentage(100));
  TEST_ASSERT_TRUE(epCovering.setTiltPercentage(25));
  TEST_ASSERT_TRUE(epCovering.setTiltPercentage(75));
}

static void fake_ias_zone_enroll_attrs(uint8_t endpoint) {
  esp_zb_ieee_addr_t fake_cie = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  uint8_t zone_id = 0;
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_set_attribute_val(
    endpoint, ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_IAS_ZONE_IAS_CIE_ADDRESS_ID, &fake_cie, false
  );
  esp_zb_zcl_set_attribute_val(endpoint, ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_IAS_ZONE_ZONEID_ID, &zone_id, false);
  esp_zb_lock_release();
}

static void test_ias_zone_devices(void) {
  // Non-enrolled devices must refuse to set+report
  TEST_ASSERT_FALSE_MESSAGE(epContact.enrolled(), "contact should start un-enrolled");
  TEST_ASSERT_FALSE_MESSAGE(epContact.setClosed(), "setClosed must fail when not enrolled");

  TEST_ASSERT_FALSE_MESSAGE(epVibration.enrolled(), "vibration should start un-enrolled");
  TEST_ASSERT_FALSE_MESSAGE(epVibration.setVibration(true), "setVibration must fail when not enrolled");

  TEST_ASSERT_FALSE_MESSAGE(epDoorHandle.enrolled(), "door handle should start un-enrolled");
  TEST_ASSERT_FALSE_MESSAGE(epDoorHandle.setClosed(), "setClosed must fail when not enrolled");

  // Simulate enrollment via the production reboot-restore path
  fake_ias_zone_enroll_attrs(ZB_EP_CONTACT);
  TEST_ASSERT_TRUE_MESSAGE(epContact.restoreIASZoneEnroll(), "contact restoreIASZoneEnroll failed");
  TEST_ASSERT_TRUE_MESSAGE(epContact.enrolled(), "contact should be enrolled after restore");

  fake_ias_zone_enroll_attrs(ZB_EP_VIBRATION);
  TEST_ASSERT_TRUE_MESSAGE(epVibration.restoreIASZoneEnroll(), "vibration restoreIASZoneEnroll failed");
  TEST_ASSERT_TRUE_MESSAGE(epVibration.enrolled(), "vibration should be enrolled after restore");

  fake_ias_zone_enroll_attrs(ZB_EP_DOOR_HANDLE);
  TEST_ASSERT_TRUE_MESSAGE(epDoorHandle.restoreIASZoneEnroll(), "door handle restoreIASZoneEnroll failed");
  TEST_ASSERT_TRUE_MESSAGE(epDoorHandle.enrolled(), "door handle should be enrolled after restore");

  // Enrolled devices: full set + report path
  TEST_ASSERT_TRUE(epContact.setClosed());
  TEST_ASSERT_TRUE(epContact.setOpen());
  TEST_ASSERT_TRUE(epContact.setClosed());

  TEST_ASSERT_TRUE(epVibration.setVibration(true));
  TEST_ASSERT_TRUE(epVibration.setVibration(false));

  TEST_ASSERT_TRUE(epDoorHandle.setClosed());
  TEST_ASSERT_TRUE(epDoorHandle.setOpen());
  TEST_ASSERT_TRUE(epDoorHandle.setTilted());
  TEST_ASSERT_TRUE(epDoorHandle.setClosed());
}

// ==================== Sensors ====================

static void test_temp_humidity_sensor(void) {
  TEST_ASSERT_TRUE(epTemp.setReporting(1, 300, 0.5f));
  TEST_ASSERT_TRUE(epTemp.setHumidityReporting(1, 300, 1.0f));

  TEST_ASSERT_TRUE(epTemp.setTemperature(20.0f));
  TEST_ASSERT_TRUE(epTemp.setTemperature(25.5f));
  TEST_ASSERT_TRUE(epTemp.reportTemperature());

  TEST_ASSERT_TRUE(epTemp.setHumidity(55.0f));
  TEST_ASSERT_TRUE(epTemp.setHumidity(60.5f));
  TEST_ASSERT_TRUE(epTemp.reportHumidity());
  TEST_ASSERT_TRUE(epTemp.report());
}

static void test_environmental_sensors(void) {
  TEST_ASSERT_TRUE(epPressure.setReporting(1, 300, 1));
  TEST_ASSERT_TRUE(epFlow.setReporting(1, 300, 0.5f));
  TEST_ASSERT_TRUE(epIlluminance.setReporting(1, 300, 10));
  TEST_ASSERT_TRUE(epCo2.setReporting(1, 300, 10));
  TEST_ASSERT_TRUE(epPm25.setReporting(1, 300, 1.0f));
  TEST_ASSERT_TRUE(epWind.setReporting(1, 300, 0.5f));

  TEST_ASSERT_TRUE(epPressure.setPressure(1013));
  TEST_ASSERT_TRUE(epPressure.report());

  TEST_ASSERT_TRUE(epFlow.setFlow(12.5f));
  TEST_ASSERT_TRUE(epFlow.report());

  TEST_ASSERT_TRUE(epIlluminance.setIlluminance(500));
  TEST_ASSERT_TRUE(epIlluminance.setIlluminance(1000));
  TEST_ASSERT_TRUE(epIlluminance.report());

  TEST_ASSERT_TRUE(epOccupancy.setOccupancy(true));
  TEST_ASSERT_TRUE(epOccupancy.setOccupancy(false));
  TEST_ASSERT_TRUE(epOccupancy.report());

  TEST_ASSERT_TRUE(epCo2.setCarbonDioxide(400.0f));
  TEST_ASSERT_TRUE(epCo2.setCarbonDioxide(800.0f));
  TEST_ASSERT_TRUE(epCo2.report());

  TEST_ASSERT_TRUE(epPm25.setPM25(12.5f));
  TEST_ASSERT_TRUE(epPm25.setPM25(25.0f));
  TEST_ASSERT_TRUE(epPm25.report());

  TEST_ASSERT_TRUE(epWind.setWindSpeed(3.5f));
  TEST_ASSERT_TRUE(epWind.setWindSpeed(10.0f));
  TEST_ASSERT_TRUE(epWind.reportWindSpeed());
}

// ==================== Analog / Binary / Multistate ====================

static void test_analog_io(void) {
  epAnalog.onAnalogOutputChange(onAnalogOutCb);

  TEST_ASSERT_TRUE(epAnalog.setAnalogInput(1.5f));
  TEST_ASSERT_TRUE(epAnalog.reportAnalogInput());

  TEST_ASSERT_TRUE(epAnalog.setAnalogOutput(42.5f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 42.5f, epAnalog.getAnalogOutput());
  delay(50);
  TEST_ASSERT_TRUE_MESSAGE(analog_out_cb_called, "analog output callback not fired");
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 42.5f, analog_out_cb_value);

  TEST_ASSERT_TRUE(epAnalog.setAnalogOutput(0.0f));
  TEST_ASSERT_TRUE(epAnalog.reportAnalogOutput());
}

static void test_binary_io(void) {
  epBinary.onBinaryOutputChange(onBinaryOutCb);

  TEST_ASSERT_TRUE(epBinary.setBinaryInput(true));
  TEST_ASSERT_TRUE(epBinary.reportBinaryInput());

  TEST_ASSERT_TRUE(epBinary.setBinaryOutput(true));
  TEST_ASSERT_TRUE(epBinary.getBinaryOutput());
  delay(50);
  TEST_ASSERT_TRUE_MESSAGE(binary_out_cb_called, "binary output callback not fired");
  TEST_ASSERT_TRUE(binary_out_cb_state);

  TEST_ASSERT_TRUE(epBinary.setBinaryOutput(false));
  TEST_ASSERT_FALSE(epBinary.getBinaryOutput());
  TEST_ASSERT_TRUE(epBinary.reportBinaryOutput());
}

static void test_multistate_io(void) {
  epMultistate.onMultistateOutputChange(onMultistateOutCb);

  TEST_ASSERT_EQUAL(ZB_MULTISTATE_APPLICATION_TYPE_0_NUM_STATES, epMultistate.getMultistateOutputStateNamesLength());

  TEST_ASSERT_TRUE(epMultistate.setMultistateInput(1));
  TEST_ASSERT_EQUAL(1, epMultistate.getMultistateInput());
  TEST_ASSERT_TRUE(epMultistate.reportMultistateInput());

  TEST_ASSERT_TRUE(epMultistate.setMultistateOutput(2));
  TEST_ASSERT_EQUAL(2, epMultistate.getMultistateOutput());
  delay(50);
  TEST_ASSERT_TRUE_MESSAGE(multistate_out_cb_called, "multistate output callback not fired");
  TEST_ASSERT_EQUAL(2, multistate_out_cb_value);

  TEST_ASSERT_TRUE(epMultistate.setMultistateOutput(0));
  TEST_ASSERT_EQUAL(0, epMultistate.getMultistateOutput());
  TEST_ASSERT_TRUE(epMultistate.reportMultistateOutput());
}

static void test_electrical_measurement(void) {
  TEST_ASSERT_TRUE(epElectrical.setDCReporting(ZIGBEE_DC_MEASUREMENT_TYPE_VOLTAGE, 1, 300, 1));

  TEST_ASSERT_TRUE(epElectrical.setDCMeasurement(ZIGBEE_DC_MEASUREMENT_TYPE_VOLTAGE, 120));
  TEST_ASSERT_TRUE(epElectrical.setDCMeasurement(ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT, 5));
  TEST_ASSERT_TRUE(epElectrical.reportDC(ZIGBEE_DC_MEASUREMENT_TYPE_VOLTAGE));
  TEST_ASSERT_TRUE(epElectrical.reportDC(ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT));
}

// ==================== Runner ====================

static void run_all_zigbee_endpoint_deep_tests(void) {
  RUN_TEST(test_light_onoff_roundtrip);
  RUN_TEST(test_light_callback_and_restore);
  RUN_TEST(test_dimmable_levels);
  RUN_TEST(test_color_light_rgb_and_level);
  RUN_TEST(test_power_outlet);
  RUN_TEST(test_fan_mode_sequence);
  RUN_TEST(test_window_covering);
  RUN_TEST(test_ias_zone_devices);
  RUN_TEST(test_temp_humidity_sensor);
  RUN_TEST(test_environmental_sensors);
  RUN_TEST(test_analog_io);
  RUN_TEST(test_binary_io);
  RUN_TEST(test_multistate_io);
  RUN_TEST(test_electrical_measurement);
}

#endif  // CONFIG_ZB_ENABLED
