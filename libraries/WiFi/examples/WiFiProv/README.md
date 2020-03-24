# Provisioning for Arduino

This sketch implements provisioning using various IDF components

# Description

This example allows Arduino user to choose either BLE or SOFTAP as a mode of transport, over which the provisioning related communication is to take place, between the device (to be provisioned) and the client (owner of the device).

## API's introduced for provisioning

### WiFi.onEvent()

Using this API user can register the WIFI Events and Provisioning Events

#### Parameters passed

A function with following signature
* void SysProvEvent( system_event_t * , wifi_prov_event_t * );
 
#### structure [ wifi_prov_event_t ]

* wifi_prov_cb_event_t event;
* void * event_data;

### WiFi.beginProvision()

prototype : beginProvision(scheme prov_scheme, wifi_prov_security_t security, wifi_prov_scheme_event_handler_t scheme_event_handler, char * pop, char * service_name, char * service_key, uint8_t * uuid);

#### Parameters

* prov_scheme : choose the mode of transfer
    * WIFI_PROV_SCHEME_BLE - Using BLE
    * WIFI_PROV_SCHEME_SOFTAP - Using SOFTAP

* security : choose whether security to apply or not
    * WIFI_PROV_SECURITY_1
    * WIFI_PROV_SECURITY_0

* scheme_event_handler : specify the handlers according to the mode choosen
    * BLE :
        - WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
        - WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BLE
        - WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BT
    * SoftAp :
        - WIFI_PROV_EVENT_HANDLER_NONE

* pop : if security choosen is [ WIFI_PROV_SECURITY_1 ] then specify proof of possesion
        if security choosen is [ WIFI_PROV_SECURITY_0 ] then proof of possesion is NULL

* service_name : Specify service name for ESP_Station while provisioning, if it is not specified then default choosen name via SOFTAP is WIFI_XXX and for BLE service it is BLE_XXXX where XXX is the last 3 digits of the boards MAC. 

* service_key : Specify service key while provisioning, if choosen mode of provisioning is BLE then service_key is always NULL

* uuid : user can specify there own 16 bit uuid while provisioning using BLE, if not specified then default value taken is
        - {  0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
             0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02, }

# NOTE

* If none of the parameters are specified in beginProvision then default provisioning takes place using SOFTAP with
    * scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
    * security = WIFI_PROV_SECURITY_1
    * pop = "abcd1234"
    * service_name = "WiFi_XXX" where XXX is the last 3 digit of the mac Address
    * service_key = NULL

# Hardware Requirement

* Enable Debuger : [ Tools -> Core Debug Level ]

# App Required for provisioning

##Gihub link

* Android : (https://github.com/espressif/esp-idf-provisioning-android)
* iOS : (https://github.com/espressif/esp-idf-provisioning-ios)

## This apps are available on playstore

* For SoftAP : ESP SoftAP Prov
* For BLE : ESP BLE Prov

# Example output

## Credentials are available on device

```
[I][WiFiProv.cpp:125] beginProvision(): Aleardy Provisioned, starting Wi-Fi STA
Station Start
[I][WiFiProv.cpp:126] beginProvision(): CONNECTING ACCESS POINT CREDENTIALS : 
Mode: STA
Channel: 1
SSID (9): GIONEE M2
Passphrase (9): 123456789
BSSID set: 0

```
## Provisioning using SOFTAP

```
[I][WiFiProv.cpp:117] beginProvision(): Starting station using SOFTAP
 service_name : WIFI_XXXX
 password : 123456789
 pop : abcd1234

Station Start

Provisioning started
Give Credentials of your access point using " Android app "

Received Wi-Fi credentials
	SSID : GIONEE M2
	Password : 123456789

Connected IP address : 192.168.43.120
Provisioning Successful
Provisioning Ends

```
## Provisioning using BLE

```
[I][WiFiProv.cpp:115] beginProvision(): 
Starting station using BLE
 service_name : BLE_XXXX
 pop : abcd1234

Provisioning started
Give Credentials of your access point using " Android app "

Provisioning Successful

Connected with IP address : 192.168.43.120
Received Wi-Fi credentials
	SSID : GIONEE M2
	Password : 123456789
Provisioning Ends

```
