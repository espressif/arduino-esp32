# Provisioning for Arduino

This sketch implements provisioning using various IDF components.

## Description

This example allows Arduino users to choose either BLE or SOFTAP as the mode of transport for provisioning-related communication between the device (to be provisioned) and the client (owner of the device).

## APIs introduced for provisioning

### WiFi.onEvent()

This API can be used to register a function to be called from another
thread for WiFi Events and Provisioning Events.

### WiFi.beginProvision()

```
WiFi.beginProvision(void (*scheme_cb)(), wifi_prov_scheme_event_handler_t scheme_event_handler, wifi_prov_security_t security, char *pop, char *service_name, char *service_key, uint8_t *uuid);
```

#### Parameters passed

- Function pointer: Choose the mode of transfer
    - `provSchemeBLE` - Using BLE
    - `provSchemeSoftAP` - Using SoftAP

- `security`: Choose the security type
    - `WIFI_PROV_SECURITY_1` - Enables secure communication with a secure handshake using key exchange and proof of possession (pop), and encryption/decryption of messages.
    - `WIFI_PROV_SECURITY_0` - Does not provide application-level security, allowing plain text communication.

- `scheme_event_handler`: Specify the handlers according to the chosen mode
    - BLE:
        - `WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM` - Used when the application doesn't need BT and BLE after provisioning is finished.
        - `WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BLE` - Used when the application doesn't need BLE to be active after provisioning is finished.
        - `WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BT` - Used when the application doesn't need BT to be active after provisioning is finished.

    - SoftAP:
        - `WIFI_PROV_EVENT_HANDLER_NONE`

- `pop`: String used for authentication.

- `service_name`: Specify the service name for the device. If not specified, the default chosen name is `PROV_XXX`, where XXX represents the last 3 bytes of the MAC address.

- `service_key`: Specify the service key. If the chosen mode of provisioning is BLE, the `service_key` is always NULL.

- `uuid`: Users can specify their own 128-bit UUID while provisioning using BLE. If not specified, the default value is:

```
{ 0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf, 0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02 }
```

- `reset_provisioned`: Resets previously provisioned data before initializing. Using this prevents problem when the device automatically connects to previously connected WiFi and therefore cannot be found.

**NOTE:** If none of the parameters are specified in `beginProvision`, default provisioning takes place using SoftAP with the following settings:
- `scheme = WIFI_PROV_SCHEME_SOFTAP`
- `scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE`
- `security = WIFI_PROV_SECURITY_1`
- `pop = "abcd1234"`
- `service_name = "PROV_XXX"`
- `service_key = NULL`
- `uuid = NULL`
- `reset_provisioned = false`

## Flashing
This sketch takes up a lot of space for the app and may not be able to flash with default setting on some chips.
If you see Error like this: "Sketch too big"
In Arduino IDE go to: Tools > Partition scheme > chose anything that has more than 1.4MB APP for example `No OTA (2MB APP/2MB SPIFFS)`

## Log Output
- To enable debugging: Go to Tools -> Core Debug Level -> Info.

## Provisioning Tools
[Provisioning Tools](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/provisioning/wifi_provisioning.html#provisioning-tools)

## Example Output

### Provisioning using SoftAP
```
[I][WiFiProv.cpp:117] beginProvision(): Starting AP using SOFTAP
 service_name: PROV_XXX
 password: 123456789
 pop: abcd1234

Provisioning started
Give Credentials of your access point using "Android app"

Received Wi-Fi credentials
    SSID: GIONEE M2
    Password: 123456789

Connected IP address: 192.168.43.120
Provisioning Successful
Provisioning Ends
```

### Provisioning using BLE
```
[I][WiFiProv.cpp:115] beginProvision(): Starting AP using BLE
 service_name: PROV_XXX
 pop: abcd1234

Provisioning started
Give Credentials of your access point using "Android app"

Received Wi-Fi credentials
    SSID: GIONEE M2
    Password: 123456789

Connected IP address: 192.168.43.120
Provisioning Successful
Provisioning Ends
```

### Credentials are available on the device
```
[I][WiFiProv.cpp:146] beginProvision(): Already Provisioned, starting Wi-Fi STA
[I][WiFiProv.cpp:150] beginProvision(): SSID: Wce*****
[I][WiFiProv.cpp:152] beginProvision(): CONNECTING TO THE ACCESS POINT:
Connected IP address: 192.168.43.120
```
