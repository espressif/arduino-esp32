TODO

Why?

    DRY (Don't Repeat Yourself) - having your passwords in one place is more manageable than changing them manually in each example, or new sketch.
    Secure - safely share the code without worrying about accidentally leaving WiFi credentials.

How it works - your SSIDs and passwords are #defined as a plain text constants in a header file. This header file can be included in any sketch and the passwords used in there hidden with the constant name.

Setup:
1. Locate your installation folder and go to the file cores/esp32/secrets.h
2. Edit the secres.h file and input the WiFi credential you are often using.
3. For examples we are using constants `SECRETS_WIFI_SSID_1` and `SECRETS_WIFI_PASSWORD_1`. You can follow the numbering or create your own constant names, for example `WIFI_SSID_HOME` + `WIFI_PWD_HOME`.
4. TODO array

