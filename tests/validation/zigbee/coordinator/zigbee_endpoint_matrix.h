/*
 * Register all Zigbee endpoint classes from Zigbee.h on the coordinator.
 * Pre-begin configuration (min/max, clusters) lives in register_all_zigbee_endpoints().
 * Deep API tests are in zigbee_endpoint_deep_tests.h.
 */

#pragma once

#if CONFIG_ZB_ENABLED

#include <Zigbee.h>

#define ZB_EP_LIGHT           1
#define ZB_EP_DIMMABLE        2
#define ZB_EP_COLOR_DIM       3
#define ZB_EP_SWITCH          4
#define ZB_EP_COLOR_DIMMER_SW 5
#define ZB_EP_OUTLET          6
#define ZB_EP_FAN             7
#define ZB_EP_THERMOSTAT      8
#define ZB_EP_COVERING        9
#define ZB_EP_ANALOG          10
#define ZB_EP_BINARY          11
#define ZB_EP_MULTISTATE      12
#define ZB_EP_TEMP            13
#define ZB_EP_PRESSURE        14
#define ZB_EP_FLOW            15
#define ZB_EP_ILLUMINANCE     16
#define ZB_EP_OCCUPANCY       17
#define ZB_EP_CO2             18
#define ZB_EP_PM25            19
#define ZB_EP_WIND            20
#define ZB_EP_CONTACT         21
#define ZB_EP_VIBRATION       22
#define ZB_EP_DOOR_HANDLE     23
#define ZB_EP_ELECTRICAL      24
#define ZB_EP_GATEWAY         25
#define ZB_EP_RANGE_EXT       26

static ZigbeeLight epLight(ZB_EP_LIGHT);
static ZigbeeDimmableLight epDimmable(ZB_EP_DIMMABLE);
static ZigbeeColorDimmableLight epColorDim(ZB_EP_COLOR_DIM);
static ZigbeeSwitch epSwitch(ZB_EP_SWITCH);
static ZigbeeColorDimmerSwitch epColorDimmerSw(ZB_EP_COLOR_DIMMER_SW);
static ZigbeePowerOutlet epOutlet(ZB_EP_OUTLET);
static ZigbeeFanControl epFan(ZB_EP_FAN);
static ZigbeeThermostat epThermostat(ZB_EP_THERMOSTAT);
static ZigbeeWindowCovering epCovering(ZB_EP_COVERING);
static ZigbeeAnalog epAnalog(ZB_EP_ANALOG);
static ZigbeeBinary epBinary(ZB_EP_BINARY);
static ZigbeeMultistate epMultistate(ZB_EP_MULTISTATE);
static ZigbeeTempSensor epTemp(ZB_EP_TEMP);
static ZigbeePressureSensor epPressure(ZB_EP_PRESSURE);
static ZigbeeFlowSensor epFlow(ZB_EP_FLOW);
static ZigbeeIlluminanceSensor epIlluminance(ZB_EP_ILLUMINANCE);
static ZigbeeOccupancySensor epOccupancy(ZB_EP_OCCUPANCY);
static ZigbeeCarbonDioxideSensor epCo2(ZB_EP_CO2);
static ZigbeePM25Sensor epPm25(ZB_EP_PM25);
static ZigbeeWindSpeedSensor epWind(ZB_EP_WIND);
static ZigbeeContactSwitch epContact(ZB_EP_CONTACT);
static ZigbeeVibrationSensor epVibration(ZB_EP_VIBRATION);
static ZigbeeDoorWindowHandle epDoorHandle(ZB_EP_DOOR_HANDLE);
static ZigbeeElectricalMeasurement epElectrical(ZB_EP_ELECTRICAL);
static ZigbeeGateway epGateway(ZB_EP_GATEWAY);
static ZigbeeRangeExtender epRangeExt(ZB_EP_RANGE_EXT);

static bool register_all_zigbee_endpoints(void) {
  epLight.setManufacturerAndModel("Espressif", "ZBValLight");
  if (!Zigbee.addEndpoint(&epLight)) {
    return false;
  }

  epDimmable.setManufacturerAndModel("Espressif", "ZBValDim");
  if (!Zigbee.addEndpoint(&epDimmable)) {
    return false;
  }

  epColorDim.setLightColorCapabilities(ZIGBEE_COLOR_CAPABILITY_HUE_SATURATION | ZIGBEE_COLOR_CAPABILITY_X_Y | ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP);
  epColorDim.setManufacturerAndModel("Espressif", "ZBValColor");
  if (!Zigbee.addEndpoint(&epColorDim)) {
    return false;
  }

  epSwitch.setManufacturerAndModel("Espressif", "ZBValSwitch");
  if (!Zigbee.addEndpoint(&epSwitch)) {
    return false;
  }

  epColorDimmerSw.setManufacturerAndModel("Espressif", "ZBValCDSwitch");
  if (!Zigbee.addEndpoint(&epColorDimmerSw)) {
    return false;
  }

  epOutlet.setManufacturerAndModel("Espressif", "ZBValOutlet");
  if (!Zigbee.addEndpoint(&epOutlet)) {
    return false;
  }

  epFan.setManufacturerAndModel("Espressif", "ZBValFan");
  if (!Zigbee.addEndpoint(&epFan)) {
    return false;
  }

  epThermostat.setManufacturerAndModel("Espressif", "ZBValThermo");
  if (!Zigbee.addEndpoint(&epThermostat)) {
    return false;
  }

  epCovering.setManufacturerAndModel("Espressif", "ZBValCover");
  if (!Zigbee.addEndpoint(&epCovering)) {
    return false;
  }

  if (!epAnalog.addAnalogInput() || !epAnalog.addAnalogOutput()) {
    return false;
  }
  epAnalog.setAnalogInputMinMax(0.0f, 100.0f);
  epAnalog.setAnalogOutputMinMax(0.0f, 100.0f);
  epAnalog.setAnalogInputResolution(0.1f);
  epAnalog.setAnalogOutputResolution(0.1f);
  epAnalog.setManufacturerAndModel("Espressif", "ZBValAnalog");
  if (!Zigbee.addEndpoint(&epAnalog)) {
    return false;
  }

  if (!epBinary.addBinaryInput() || !epBinary.addBinaryOutput()) {
    return false;
  }
  epBinary.setBinaryInputApplication(BINARY_INPUT_APPLICATION_TYPE_HVAC_OCCUPANCY);
  epBinary.setBinaryOutputApplication(BINARY_OUTPUT_APPLICATION_TYPE_HVAC_FAN);
  epBinary.setManufacturerAndModel("Espressif", "ZBValBinary");
  if (!Zigbee.addEndpoint(&epBinary)) {
    return false;
  }

  if (!epMultistate.addMultistateInput() || !epMultistate.addMultistateOutput()) {
    return false;
  }
  epMultistate.setMultistateInputApplication(ZB_MULTISTATE_APPLICATION_TYPE_0_INDEX);
  epMultistate.setMultistateOutputApplication(ZB_MULTISTATE_APPLICATION_TYPE_0_INDEX);
  epMultistate.setMultistateInputStates(ZB_MULTISTATE_APPLICATION_TYPE_0_NUM_STATES);
  epMultistate.setMultistateOutputStates(ZB_MULTISTATE_APPLICATION_TYPE_0_NUM_STATES);
  epMultistate.setManufacturerAndModel("Espressif", "ZBValMulti");
  if (!Zigbee.addEndpoint(&epMultistate)) {
    return false;
  }

  epTemp.setMinMaxValue(-40.0f, 85.0f);
  epTemp.setTolerance(0.5f);
  epTemp.addHumiditySensor(0.0f, 100.0f, 0.5f, 45.0f);
  epTemp.setManufacturerAndModel("Espressif", "ZBValTemp");
  if (!Zigbee.addEndpoint(&epTemp)) {
    return false;
  }

  epPressure.setMinMaxValue(900, 1100);
  epPressure.setTolerance(1);
  epPressure.setManufacturerAndModel("Espressif", "ZBValPress");
  if (!Zigbee.addEndpoint(&epPressure)) {
    return false;
  }

  epFlow.setMinMaxValue(0.0f, 100.0f);
  epFlow.setTolerance(0.1f);
  epFlow.setManufacturerAndModel("Espressif", "ZBValFlow");
  if (!Zigbee.addEndpoint(&epFlow)) {
    return false;
  }

  epIlluminance.setMinMaxValue(0, 65535);
  epIlluminance.setTolerance(1);
  epIlluminance.setManufacturerAndModel("Espressif", "ZBValIllum");
  if (!Zigbee.addEndpoint(&epIlluminance)) {
    return false;
  }

  epOccupancy.setManufacturerAndModel("Espressif", "ZBValOcc");
  if (!Zigbee.addEndpoint(&epOccupancy)) {
    return false;
  }

  epCo2.setMinMaxValue(0.0f, 5000.0f);
  epCo2.setTolerance(10.0f);
  epCo2.setManufacturerAndModel("Espressif", "ZBValCO2");
  if (!Zigbee.addEndpoint(&epCo2)) {
    return false;
  }

  epPm25.setMinMaxValue(0.0f, 500.0f);
  epPm25.setTolerance(1.0f);
  epPm25.setManufacturerAndModel("Espressif", "ZBValPM25");
  if (!Zigbee.addEndpoint(&epPm25)) {
    return false;
  }

  epWind.setMinMaxValue(0.0f, 50.0f);
  epWind.setTolerance(0.5f);
  epWind.setManufacturerAndModel("Espressif", "ZBValWind");
  if (!Zigbee.addEndpoint(&epWind)) {
    return false;
  }

  epContact.setManufacturerAndModel("Espressif", "ZBValContact");
  if (!Zigbee.addEndpoint(&epContact)) {
    return false;
  }

  epVibration.setManufacturerAndModel("Espressif", "ZBValVibr");
  if (!Zigbee.addEndpoint(&epVibration)) {
    return false;
  }

  epDoorHandle.setManufacturerAndModel("Espressif", "ZBValDoor");
  if (!Zigbee.addEndpoint(&epDoorHandle)) {
    return false;
  }

  if (!epElectrical.addDCMeasurement(ZIGBEE_DC_MEASUREMENT_TYPE_VOLTAGE) || !epElectrical.addDCMeasurement(ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT)) {
    return false;
  }
  epElectrical.setDCMinMaxValue(ZIGBEE_DC_MEASUREMENT_TYPE_VOLTAGE, 0, 250);
  epElectrical.setDCMinMaxValue(ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT, 0, 100);
  epElectrical.setDCMultiplierDivisor(ZIGBEE_DC_MEASUREMENT_TYPE_VOLTAGE, 1, 10);
  epElectrical.setManufacturerAndModel("Espressif", "ZBValElec");
  if (!Zigbee.addEndpoint(&epElectrical)) {
    return false;
  }

  epGateway.setManufacturerAndModel("Espressif", "ZBValGW");
  if (!Zigbee.addEndpoint(&epGateway)) {
    return false;
  }

  epRangeExt.setManufacturerAndModel("Espressif", "ZBValRange");
  if (!Zigbee.addEndpoint(&epRangeExt)) {
    return false;
  }

  return true;
}

#endif  // CONFIG_ZB_ENABLED
