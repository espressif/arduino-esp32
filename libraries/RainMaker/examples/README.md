# ESP RainMaker Examples

While building any examples for ESP RainMaker, take care of the following:

1. Change partition scheme in Arduino IDE to RainMaker (Tools -> Partition Scheme -> RainMaker).
2. Once ESP RainMaker gets started, compulsorily call `WiFi.beginProvision()` which is responsible for user-node mapping.
3. Use the appropriate provisioning scheme as per the board.
    - ESP32 Board: BLE Provisioning
    - ESP32-C3 Board: BLE Provisioning
    - ESP32-S3 Board: BLE Provisioning
    - ESP32-S2 Board: SoftAP Provisioning
4. Set debug level to Info (Tools -> Core Debug Level -> Info). This is recommended debug level but not mandatory to run RainMaker.

