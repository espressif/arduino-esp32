# Smoke Test App

## What to expect in this example?

- This example is expected to exercise the various features of the ESP Insights framework
- As a smoke test, this allows you to validate, by a quick perusal of the ESP Insights dashboard, the functioning of all the high-level features


## End-to-End Tests

### Lifecycle of the test (Hard reset resets the cycle)
* Device boots up and logs errors/warnings/events in random order every 10 seconds
* Every error/warning/event log with "diag_smoke" tag is associated with an incremental counter
* There's a 30/500 probability that device will crash, this is done for verification of crash
* Device will crash only five times and hard reset will reset the counter to 1
* On sixth boot onwards device will not crash and logs errors/warnings/events and adds heap metrics

### Facilitate the Auth Key
In this example we will be using the auth key that we downloaded while [setting up ESP Insights account](https://github.com/espressif/esp-insights/tree/main/examples#set-up-esp-insights-account).

Copy Auth Key to the example
```
const char insights_auth_key[] = "<ENTER YOUR AUTH KEY>";
```

### Enter WiFi Credentials
Inside the example sketch, enter your WiFi credentials in `WIFI_SSID` and `WIFI_PASSPHRASE` macros.

### Setup
* Build and flash the sketch and monitor the console
* Device will eventually crash after some time
* Before every crash you will see below log print
```
E (75826) diag_smoke: [count][7] [crash_count][1] [excvaddr][0x0f] Crashing...
// [count][7]: count associated with the log
// [crash_count][1]: This is the first crash since device boot up, this number will increment as the crash count increases
// [excvaddr][0x0f]: Exception vaddr, will see this in crash verification part below
```
* You'll see five crashes([crash_count][5]) and after that device will not crash and will keep on logging and adding metrics
* Onwards this point keep device running for more than 30 minutes
* Now we are all set to visit the [dashboard](https://dashboard.insights.espressif.com)
* Select the node-id printed on the console, look for the below log. It is printed early when device boots up
```
ESP Insights enabled for Node ID ----- wx3vEoGgJPk7Rn5JvRUFs9
```