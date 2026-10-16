# Arduino-ESP32 Zigbee + Wi‑Fi Weather Example

This example shows how to **pause the Zigbee stack** with `Zigbee.stop()`, use **Wi‑Fi** to fetch outdoor **temperature, humidity and pressure** from the free [Open-Meteo](https://open-meteo.com/) API, then **resume Zigbee** with `Zigbee.start()` and report those values on Zigbee endpoints.

It is intended for SoCs that share the radio between Zigbee (802.15.4) and Wi‑Fi (e.g. ESP32-C6): Zigbee must be stopped before Wi‑Fi can use the radio.

The device runs as a **Zigbee end device** (not a router). Pausing Zigbee for Wi‑Fi must not interrupt routing for other mesh devices. Prefer mains power; this radio handoff pattern is a poor fit for long battery sleep.

# Supported Targets

| Supported Targets | ESP32-C6 | ESP32-S31* |
| ----------------- | -------- | ---------- |
|                   | yes      | yes*       |

\* Preview / when Zigbee + Wi‑Fi are enabled for the board.

**Not supported:** ESP32-H2 (Zigbee only, no Wi‑Fi).

## Zigbee endpoints

| Endpoint | Device | Attributes from Open-Meteo |
| -------- | ------ | -------------------------- |
| 1 | `ZigbeeTempSensor` (+ humidity cluster) | `temperature_2m`, `relative_humidity_2m` |
| 2 | `ZigbeePressureSensor` | `pressure_msl` (hPa) |

## How it works

1. **Before Zigbee starts:** connect Wi‑Fi, fetch weather from Open-Meteo, disconnect Wi‑Fi.
2. Use those values as Zigbee attribute **defaults** (`setDefaultValue` / humidity default). If the fetch fails, fallback constants are used.
3. Start Zigbee as an **end device**, join the network, configure reporting, and report the initial weather values.
4. Every **60 seconds**:
   - `Zigbee.stop()` exits the Zigbee mainloop and releases the radio.
   - Connect Wi‑Fi, fetch updated weather, disconnect Wi‑Fi.
   - `Zigbee.start()` re-enters the Zigbee mainloop (network state kept in NVS).
   - Wait until the stack is ready, then update and report temperature, humidity and pressure (with short gaps / retries).
5. BOOT button: short press reports last values; hold ~3 s for factory reset.

## Hardware Required

* ESP32-C6 (or other Zigbee + Wi‑Fi board) as Zigbee **end device** (prefer mains powered)
* A Zigbee coordinator / hub (e.g. Home Assistant ZHA)
* USB cable for power and programming
* Wi‑Fi access point with internet access

## Configure the Project

1. Set Wi‑Fi credentials at the top of the sketch:

```cpp
const char *ssid = "your-ssid";
const char *password = "your-password";
```

2. Optional: change `weatherUrl` latitude and longitude for your location (default: Brno).

### Using Arduino IDE

* Select board: `Tools -> Board` (e.g. ESP32C6 Dev Module)
* Zigbee mode: `Tools -> Zigbee mode: Zigbee ED (end device)`
* Partition scheme: `Tools -> Partition Scheme: Zigbee 8MB with spiffs`
  (Zigbee + Wi‑Fi needs more app flash than the default 4MB Zigbee scheme)
* Flash size: `Tools -> Flash Size: 8MB`
* Select COM port
* Optional: `Tools -> Core Debug Level: Verbose`

**Switching from router (ZCZR) to ED:** erase flash before upload, or hold BOOT ~3 s after join for factory reset, so NVS does not keep the old role.

## Troubleshooting

* **Wi‑Fi never connects:** Check SSID/password; ensure the AP is 2.4 GHz.
* **Weather fetch fails:** Check internet access; Open-Meteo must be reachable on port 80.
* **Sketch too big:** Use the **Zigbee 8MB with spiffs** partition scheme and **8MB** flash size.
* **Wi‑Fi fails after Zigbee with `NO_AP_FOUND`:** After `Zigbee.stop()`, the library disables the IEEE802.15.4 radio so Wi‑Fi can use the shared RF. Reflash with that Zigbee library fix; ensure `Zigbee.stop()` completes before `WiFi.begin()`.
* **`timeout when WiFi un-init` / multi-second stall after resume:** Fully turn Wi‑Fi off (`disconnect` then `WiFi.mode(WIFI_OFF)`) and wait before `Zigbee.start()` so Wi‑Fi finishes PHY cleanup while 802.15.4 is still disabled.
* **`Failed to report attribute: 0xffffffff` after resume:** Stack/APS not ready yet; the example waits for `Zigbee.connected()`, spaces cluster reports, and retries.
* **Device briefly offline on the hub:** Expected while Zigbee is stopped for Wi‑Fi; keep `WIFI_FETCH_INTERVAL_MS` reasonable.
* **Flash / join issues:** Erase flash before upload (`Tools -> Erase All Flash Before Sketch Upload: Enabled`), or call `Zigbee.factoryReset()`.

***Important: Use a good USB cable and a reliable power source.***

## Resources

* [Open-Meteo API](https://open-meteo.com/)
* [ESP32 Forum](https://esp32.com)
* [arduino-esp32](https://github.com/espressif/arduino-esp32)
* [ESP Zigbee SDK](https://docs.espressif.com/projects/esp-zigbee-sdk/)
