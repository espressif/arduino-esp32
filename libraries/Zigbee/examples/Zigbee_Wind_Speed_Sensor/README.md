# Zigbee Wind Speed Sensor Integration with HomeAssistant ZHA

This guide provides a workaround for integrating a Zigbee Wind Speed Sensor with HomeAssistant using the ZHA integration. Since the wind speed cluster is not natively supported, we will use the ZHA Toolkit from HACS to read the wind speed attribute and store it in a helper variable.
## Alternative Option: Creating a Custom Quirk

For advanced users, a more robust solution is to create a custom quirk for your Zigbee Wind Speed Sensor. This approach involves writing a custom device handler that directly supports the wind speed cluster, providing a more seamless integration with HomeAssistant.

Creating a custom quirk can be complex and requires familiarity with Python and the Zigbee protocol. However, it offers greater flexibility and control over your device's behavior.

For more information and guidance on creating custom quirks, visit the [ZHA Device Handlers repository](https://github.com/zigpy/zha-device-handlers/).

## Prerequisites

- HomeAssistant installed and running
- Zigbee Wind Speed Sensor paired with HomeAssistant ZHA
- HACS (Home Assistant Community Store) installed. For more information, visit [HACS](https://hacs.xyz)

## Steps

### 1. Install ZHA Toolkit

1. Open HomeAssistant.
2. Navigate to HACS > Integrations.
3. Search for "ZHA Toolkit - Service for advanced Zigbee Usage" and install it. For more information, visit the [ZHA Toolkit repository](https://github.com/mdeweerd/zha-toolkit).
4. Restart HomeAssistant to apply changes.

### 2. Create a Helper Variable

1. Go to Configuration -> Devices & Services -> Helpers.
2. Click on "Add Helper" and select "Number".
3. Name the helper (e.g., `wind_speed`), set the minimum and maximum values, and save it.

### 3. Create an Automation

1. Go to Configuration > Automations & Scenes.
2. Click on "Add Automation" and choose "Start with an empty automation".
3. Set a name for the automation (e.g., `Read Wind Speed`).
4. Add a trigger:
    - Trigger Type: Time Pattern
    - Every: 30 seconds
5. Add an action (Then do):
    - Action Type: ZHA Toolkit: Read Attribute
    - Setup the action:
      ```yaml
        action: zha_toolkit.attr_read
        metadata: {}
        data:
            ieee: f0:f5:bd:ff:fe:0e:61:30 #set device IEEE address
            endpoint: 10 #set windspeed device endpoint
            cluster: 1035 #use this windspeed cluster
            attribute: 0 #read measurement value
            state_id: input_number.wind_speed #save to created helper variable
            state_value_template: value/100 #use correct value format (convert u16 to float)
      ```
6. Save the automation.

## Conclusion

By following these steps, you can successfully integrate your Zigbee Wind Speed Sensor with HomeAssistant using the ZHA integration and ZHA Toolkit. The wind speed readings will be updated every 30 seconds and stored in the helper variable for use in your HomeAssistant setup.
The helper variable `wind_speed` is now an entity in HomeAssistant. You can use this entity to display the wind speed on your dashboard or in other automations.
