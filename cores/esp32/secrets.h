#pragma once

#define SECRETS_WIFI_SSID_1 "example-SSID1"
#define SECRETS_WIFI_PASSWORD_1 "example-password-1"

#define SECRETS_WIFI_SSID_2 "example-SSID2"
#define SECRETS_WIFI_PASSWORD_2 "example-password-2"

#define SECRETS_WIFI_ARRAY_MAX 3 // Number of entries in the array

char SECRET_WIFI_SSID_ARRAY[SECRETS_WIFI_ARRAY_MAX][32] = {
SECRETS_WIFI_SSID_1,
SECRETS_WIFI_SSID_2,
"example-SSID3"
};

char SECRET_WIFI_PASSWORD_ARRAY[SECRETS_WIFI_ARRAY_MAX][32] = {
SECRETS_WIFI_PASSWORD_1,
SECRETS_WIFI_PASSWORD_2,
"example-password-3"
};