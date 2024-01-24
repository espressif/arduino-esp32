# OTA Firmware Upgrade for Arduino
This sketch allows Arduino user to perform Over The Air (OTA) firmware upgrade. It uses HTTPS.
 
# API introduced for OTA

## HttpsOTA.begin(const char * url, const char * server_certificate, bool skip_cert_common_name_check) 

Main API which starts firmware upgrade

### Parameters
* url : URL for the uploaded firmware image
* server_certificate : Provide the ota server certificate for authentication via HTTPS
* skip_cert_common_name_check : Skip any validation of server certificate CN field 

The default value provided to skip_cert_common_name_check is true

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
