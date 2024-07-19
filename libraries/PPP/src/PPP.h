#pragma once

#include "sdkconfig.h"
#if CONFIG_LWIP_PPP_SUPPORT
#include "Network.h"
#include "esp_modem_c_api_types.h"

typedef enum {
  PPP_MODEM_GENERIC = ESP_MODEM_DCE_GENETIC,
  PPP_MODEM_SIM7600 = ESP_MODEM_DCE_SIM7600,
  PPP_MODEM_SIM7070 = ESP_MODEM_DCE_SIM7070,
  PPP_MODEM_SIM7000 = ESP_MODEM_DCE_SIM7000,
  PPP_MODEM_BG96 = ESP_MODEM_DCE_BG96,
  PPP_MODEM_SIM800 = ESP_MODEM_DCE_SIM800,
#if CONFIG_ESP_MODEM_ADD_CUSTOM_MODULE
  PPP_MODEM_CUSTOM = ESP_MODEM_DCE_CUSTOM,
#endif
  PPP_MODEM_MAX
} ppp_modem_model_t;

class PPPClass : public NetworkInterface {
public:
  PPPClass();
  ~PPPClass();

  bool begin(ppp_modem_model_t model, uint8_t uart_num = 1, int baud_rate = 115200);
  void end();

  // Required for connecting to internet
  bool setApn(const char *apn);

  // Required only if the SIM card is protected by PIN
  bool setPin(const char *pin);

  // If the modem supports hardware flow control, it's best to use it
  bool setPins(int8_t tx, int8_t rx, int8_t rts = -1, int8_t cts = -1, esp_modem_flow_ctrl_t flow_ctrl = ESP_MODEM_FLOW_CONTROL_NONE);

  // Using the reset pin of the module ensures that proper communication can be achieved
  void setResetPin(int8_t rst, bool active_low = true, uint32_t reset_delay = 200);

  // Modem DCE APIs
  int RSSI() const;
  int BER() const;
  String IMSI() const;
  String IMEI() const;
  String moduleName() const;    // modem module name
  String operatorName() const;  // network operator name
  int networkMode() const;      // network type (GSM, LTE, etc.)
  int radioState() const;       // 0:minimal, 1:full
  bool attached() const;        // true is attached to network
  bool sync() const;            // true if responds to 'AT'
  int batteryVoltage() const;
  int batteryLevel() const;
  int batteryStatus() const;

  // Switch the communication mode
  bool mode(esp_modem_dce_mode_t m);
  esp_modem_dce_mode_t mode() const {
    return _mode;
  }

  // Change temporary the baud rate of communication
  bool setBaudrate(int baudrate);

  // Sens SMS message to a number
  bool sms(const char *num, const char *message);
  bool sms(String num, String message) {
    return sms(num.c_str(), message.c_str());
  }

  // Send AT command with timeout in milliseconds
  String cmd(const char *at_command, int timeout);
  String cmd(String at_command, int timeout) {
    return cmd(at_command.c_str(), timeout);
  }

  // untested
  bool powerDown();
  bool reset();
  bool storeProfile();

  esp_modem_dce_t *handle() const;

protected:
  size_t printDriverInfo(Print &out) const;

public:
  void _onPppEvent(int32_t event_id, void *event_data);
  void _onPppArduinoEvent(arduino_event_id_t event, arduino_event_info_t info);

private:
  esp_modem_dce_t *_dce;
  int8_t _pin_tx;
  int8_t _pin_rx;
  int8_t _pin_rts;
  int8_t _pin_cts;
  esp_modem_flow_ctrl_t _flow_ctrl;
  int8_t _pin_rst;
  bool _pin_rst_act_low;
  uint32_t _pin_rst_delay;
  const char *_pin;
  const char *_apn;
  int _rx_buffer_size;
  int _tx_buffer_size;
  esp_modem_dce_mode_t _mode;
  uint8_t _uart_num;

  static bool pppDetachBus(void *bus_pointer);
};

extern PPPClass PPP;

#endif /* CONFIG_LWIP_PPP_SUPPORT */
