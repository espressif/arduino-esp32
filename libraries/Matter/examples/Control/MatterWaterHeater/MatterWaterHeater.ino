/*
 * Matter Water Heater example for Arduino-ESP32.
 *
 * Creates a Matter 1.4 Water Heater endpoint (0x050F).
 */

#include <Matter.h>
#include <MatterEndpoints/MatterWaterHeater.h>

MatterWaterHeater waterHeater;

constexpr float COLD_WATER_TEMP = 20.0f;
constexpr float INITIAL_WATER_TEMP = 20.0f;

constexpr float TANK_VOLUME_LITERS = 100.0f;

unsigned long lastUpdate = 0;

void updateTankPercentage()
{
  const float current = waterHeater.getLocalTemperature();
  const float target = waterHeater.getHeatingSetpoint();

  if (target <= COLD_WATER_TEMP) {
    waterHeater.setTankPercentage(0);
    return;
  }

  float percentage =
    ((current - COLD_WATER_TEMP) /
     (target - COLD_WATER_TEMP)) * 100.0f;

  percentage = constrain(percentage, 0.0f, 100.0f);

  waterHeater.setTankPercentage(
    static_cast<uint8_t>(percentage)
  );
}

void setup()
{
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("Matter Water Heater");
  Serial.println("-------------------");

  if (!waterHeater.begin()) {
    Serial.println(
      "Failed to create Matter Water Heater endpoint"
    );

    while (true) {
      delay(1000);
    }
  }

  /*
   * Describe the physical water heater.
   */
  waterHeater.setHeaterTypes(
    MatterWaterHeater::IMMERSION_ELEMENT_1
  );

  waterHeater.setTankVolume(
    static_cast<uint16_t>(TANK_VOLUME_LITERS)
  );

  waterHeater.setLocalTemperature(
    INITIAL_WATER_TEMP
  );

  waterHeater.setHeatingSetpoint(48.0f);

  waterHeater.setSystemMode(
    MatterWaterHeater::SYSTEM_MODE_HEAT
  );

  waterHeater.setWaterHeaterMode(
    MatterWaterHeater::WATER_HEATER_MODE_MANUAL
  );

  waterHeater.setTankPercentage(0);

  /*
   * Start Matter only after the endpoint has been created.
   */
  Matter.begin();

  Serial.println(
    "Matter Water Heater endpoint created."
  );

  if (!Matter.isDeviceCommissioned()) {

    Serial.println();
    Serial.println(
      "Device is not commissioned."
    );

    Serial.printf(
      "Manual pairing code: %s\n",
      Matter.getManualPairingCode().c_str()
    );

    Serial.printf(
      "QR code URL: %s\n",
      Matter.getOnboardingQRCodeUrl().c_str()
    );
  }
}

void loop()
{
  /*
   * Simulate heating.
   *
   * This is only an example. In a real implementation,
   * local temperature would come from a physical sensor.
   */
  if (millis() - lastUpdate < 5000) {
    return;
  }

  lastUpdate = millis();

  float temperature =
    waterHeater.getLocalTemperature();

  float setpoint =
    waterHeater.getHeatingSetpoint();

  if (waterHeater.getSystemMode() ==
      MatterWaterHeater::SYSTEM_MODE_HEAT) {

    if (temperature < setpoint) {
      temperature += 0.5f;

      if (temperature > setpoint) {
        temperature = setpoint;
      }

      waterHeater.setLocalTemperature(
        temperature
      );

      waterHeater.setHeatDemand(100);
    } else {
      waterHeater.setHeatDemand(0);
    }
  }

  updateTankPercentage();

  Serial.printf(
    "Temperature: %.1f C | "
    "Setpoint: %.1f C | "
    "Tank: %u %% | "
    "Demand: %u %% | "
    "Boost: %u\n",

    waterHeater.getLocalTemperature(),
    waterHeater.getHeatingSetpoint(),
    waterHeater.getTankPercentage(),
    waterHeater.getHeatDemand(),
    waterHeater.getBoostState()
  );
}