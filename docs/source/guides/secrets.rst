#######
Secrets
#######

Why?
----

* DRY (Don't Repeat Yourself) - having your passwords in one place is more manageable than changing them manually in each example, or new sketch.

* Secure - safely share the code without worrying about accidentally exposing WiFi credentials.

How it works - your SSIDs and passwords are ``#defined`` as a plain text constants in a header file located in sketch-book folder (after you manually create it). The platform.txt file automatically includes path to the sketch book as a relative path ``{runtime.hardware.path}/../../``. This header file can be included in any sketch and the passwords used in there hidden with the constant name.

.. note::

    You can still use the traditional way of hard-coded WiFi credentials if you want to. Using ``secrets`` is only something extra on top of that.

.. warning::

    Secrets are stored as a plain text in an unencrypted form. They can be stolen if you keep your computer unlocked and accessible. If the entire disc is unencrypted it can be read when extracted from the computer.

Setup:
------
1. Locate your sketch folder and create file secrets.h if it does not exist yet.

    * To locate your sketchbook folder, click in the Arduino IDE on ``File > Preferences`` (or press Ctrl + comma) on the top is the sketchbook path.

2. Edit the secrets.h file and input the WiFi credential you are often using. You can use simply copy & paste the example below and edit the credentials.

3. For examples we are using constants `SECRETS_WIFI_SSID_1` and `SECRETS_WIFI_PASSWORD_1`. You can follow the numbering or create your own constant names, for example `WIFI_SSID_HOME` + `WIFI_PWD_HOME`.

4. For multi Access Point usage you can use an array of credentials you can expand - either add existing constants, or create a plain text.


Example contents:
-----------------

.. code-block:: c++

    #pragma once

    #define SECRETS_WIFI_SSID_1 "example-SSID1"
    #define SECRETS_WIFI_PASSWORD_1 "example-password-1"

    #define SECRETS_WIFI_SSID_2 "example-SSID2"
    #define SECRETS_WIFI_PASSWORD_2 "example-password-2"

    #define SECRETS_WIFI_ARRAY_MAX 3 // Number of entries in the array

    const char SECRET_WIFI_SSID_ARRAY[SECRETS_WIFI_ARRAY_MAX][32] = {
    SECRETS_WIFI_SSID_1,
    SECRETS_WIFI_SSID_2,
    "example-SSID3"
    };

    const char SECRET_WIFI_PASSWORD_ARRAY[SECRETS_WIFI_ARRAY_MAX][32] = {
    SECRETS_WIFI_PASSWORD_1,
    SECRETS_WIFI_PASSWORD_2,
    "example-password-3"
    };


Example usage:
--------------

.. code-block:: c++

    #include <WiFi.h>
    #include "secrets.h"
    const char* ssid     = SECRETS_WIFI_SSID_1;
    const char* password = SECRETS_WIFI_PASSWORD_1;
    //const char* ssid     = "example-SSID1"; // Traditional usage
    //const char* password = "example-password-1"; // Traditional usage
    WiFi.begin(ssid, password);