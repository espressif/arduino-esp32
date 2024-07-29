# ESP RainMaker Examples

While building any examples for ESP RainMaker, take care of the following:

1. Change the partition scheme that fits your flash size in Arduino IDE to "RainMaker 4MB", "RainMaker 4MB no OTA" or "RainMaker 8MB" (Tools -> Partition Scheme -> RainMaker).
2. Once ESP RainMaker gets started, compulsorily call `WiFi.beginProvision()` which is responsible for user-node mapping.
3. Use the appropriate provisioning scheme as per the board.
    - ESP32 Board: BLE Provisioning
    - ESP32-C3 Board: BLE Provisioning
    - ESP32-S3 Board: BLE Provisioning
    - ESP32-S2 Board: SoftAP Provisioning
    - ESP32-C6 Board: BLE Provisioning
    - ESP32-H2 Board: BLE Provisioning
4. Set debug level to Info (Tools -> Core Debug Level -> Info). This is the recommended debug level but not mandatory to run RainMaker.
