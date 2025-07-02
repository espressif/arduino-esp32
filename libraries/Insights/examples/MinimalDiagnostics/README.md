# Minimal Diagnostics example
- [What to expect in this example](#what-to-expect-in-this-example)
- [Try out the example](#try-out-the-example)
- [Insights Dashboard](#insights-dashboard)

## What to expect in this example?
- This example demonstrates the use of ESP Insights framework in minimal way
- Device will try to connect with the configured Wi-Fi network
- ESP Insights is enabled in this example, so any error/warning logs, crashes will be reported to cloud
- This example collects heap and Wi-Fi metrics every 10 minutes and network variables are collected when they change

## Try out the example

### Facilitate the Auth Key
In this example we will be using the auth key that we downloaded while [setting up ESP Insights account](https://github.com/espressif/esp-insights/tree/main/examples#set-up-esp-insights-account).

Copy Auth Key to the example
```
const char insights_auth_key[] = "<ENTER YOUR AUTH KEY>";
```

### Enter Wi-Fi Credentials
Inside the example sketch, enter your Wi-Fi credentials in `WIFI_SSID` and `WIFI_PASSPHRASE` macros.

### Get the Node ID
- Start the Serial monitor

- Once the device boots, it will connect to the Wi-Fi network, look for logs similar to below and make a note of Node ID.
```
I (4161) esp_insights: =========================================
I (4171) esp_insights: Insights enabled for Node ID 246F2880371C
I (4181) esp_insights: =========================================
```


## Insights Dashboard
Once everything is set up, any diagnostics information reported will show up on the [Insights Dashboard](https://dashboard.insights.espressif.com). Sign in using the your credentials.


### Monitor the device diagnostics
Visit [Nodes](https://dashboard.insights.espressif.com/home/nodes) section on the dashboard and click on the Node ID to monitor device diagnostics information.

> Note: Diagnostics data is reported dynamically or when the buffers are filled to configured threshold. So, it can take some time for the logs to reflect on the dashboard. Moreover, if a large number of logs are generated then data will be sent to cloud but, if it fails(eg reasons: Wi-Fi failure, No internet) then any newer logs will be dropped.
