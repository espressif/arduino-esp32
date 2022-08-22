# OTA Firmware Upgrade for Arduino
This sketch allows Arduino user to perform Over The Air (OTA) firmware upgrade. It uses HTTPS.
 
# API introduced for OTA

## HttpsOTA.begin(const char * url, const char * server_certificate, bool skip_cert_common_name_check) 

Begins the Https OTA update process in another task in the background, requires atleast 16KB of free heap space to use the array that will store the HTTPS packets that are downloaded.

### Parameters
* url : Url to the binary that should be downloaded and flashed onto the 2nd ota data partition
* server_certificate : Root server certificate of the website we are downloading the binary from
* update_network_interface : Underlying network interface we want to download the binary over (Ethernet, WLan, etc.). Ethernet requries using the ETH.h base class
* skip_cert_common_name_check : Wheter the certificate check should be skipped, generally not recommended if the update process needs to be secure, because this makes us vulnerable to man in the middle attacks where an attacker pretends to be the server and we might not download the actual binary we want too, but a binary sent by an attacker instead

The default value provided to update_network_interface is UpdateNetworkInterface::DEFAULT_INTERFACE and skip_cert_common_name_check is true

## HttpsOTA.onHttpEvent(function)

This API exposes HTTP Events to the user

### Parameter
Function passed has following signature 
void HttpEvent (HttpEvent_t * event);

# HttpsOTA.otaStatus()

It tracks the progress of OTA firmware upgrade.
* HTTPS_OTA_IDLE : OTA upgrade have not started yet.
* HTTPS_OTA_UPDATNG : OTA upgarde is in progress.
* HTTPS_OTA_SUCCESS : OTA upgrade is successful.
* HTTPS_OTA_FAIL : OTA upgrade failed.
* HTTPS_OTA_ERR : Error occured while creating xEventGroup().
