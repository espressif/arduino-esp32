#define ARDUINO_CORE_BUILD
#include "PPP.h"
#if CONFIG_LWIP_PPP_SUPPORT && ARDUINO_HAS_ESP_MODEM
#include "esp32-hal-periman.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include <string>
#include "driver/uart.h"
#include "hal/uart_ll.h"
#include "esp_private/uart_share_hw_ctrl.h"

#define PPP_CMD_MODE_CHECK(x)                                    \
  if (_dce == NULL) {                                            \
    return x;                                                    \
  }                                                              \
  if (_mode == ESP_MODEM_MODE_DATA) {                            \
    log_e("Wrong modem mode. Should be ESP_MODEM_MODE_COMMAND"); \
    return x;                                                    \
  }

typedef struct {
  void *arg;
} PdpContext;
#include "esp_modem_api.h"

// Because of how esp_modem functions are declared, we need to workaround some APIs that take strings as input (output works OK)
// Following APIs work only when called through this interface
extern "C" {
esp_err_t _esp_modem_at(esp_modem_dce_t *dce_wrap, const char *at, char *p_out, int timeout);
esp_err_t _esp_modem_at_raw(esp_modem_dce_t *dce_wrap, const char *cmd, char *p_out, const char *pass, const char *fail, int timeout);
esp_err_t _esp_modem_send_sms(esp_modem_dce_t *dce_wrap, const char *number, const char *message);
esp_err_t _esp_modem_set_pin(esp_modem_dce_t *dce_wrap, const char *pin);
esp_err_t _esp_modem_set_operator(esp_modem_dce_t *dce_wrap, int mode, int format, const char *oper);
esp_err_t _esp_modem_set_network_bands(esp_modem_dce_t *dce_wrap, const char *mode, const int *bands, int size);
};

static PPPClass *_esp_modem = NULL;
static esp_event_handler_instance_t _ppp_ev_instance = NULL;

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
static const char *_ppp_event_name(int32_t event_id) {
  switch (event_id) {
    case NETIF_PPP_ERRORNONE:          return "No error.";
    case NETIF_PPP_ERRORPARAM:         return "Invalid parameter.";
    case NETIF_PPP_ERROROPEN:          return "Unable to open PPP session.";
    case NETIF_PPP_ERRORDEVICE:        return "Invalid I/O device for PPP.";
    case NETIF_PPP_ERRORALLOC:         return "Unable to allocate resources.";
    case NETIF_PPP_ERRORUSER:          return "User interrupt.";
    case NETIF_PPP_ERRORCONNECT:       return "Connection lost.";
    case NETIF_PPP_ERRORAUTHFAIL:      return "Failed authentication challenge.";
    case NETIF_PPP_ERRORPROTOCOL:      return "Failed to meet protocol.";
    case NETIF_PPP_ERRORPEERDEAD:      return "Connection timeout";
    case NETIF_PPP_ERRORIDLETIMEOUT:   return "Idle Timeout";
    case NETIF_PPP_ERRORCONNECTTIME:   return "Max connect time reached";
    case NETIF_PPP_ERRORLOOPBACK:      return "Loopback detected";
    case NETIF_PPP_PHASE_DEAD:         return "Phase Dead";
    case NETIF_PPP_PHASE_MASTER:       return "Phase Master";
    case NETIF_PPP_PHASE_HOLDOFF:      return "Phase Hold Off";
    case NETIF_PPP_PHASE_INITIALIZE:   return "Phase Initialize";
    case NETIF_PPP_PHASE_SERIALCONN:   return "Phase Serial Conn";
    case NETIF_PPP_PHASE_DORMANT:      return "Phase Dormant";
    case NETIF_PPP_PHASE_ESTABLISH:    return "Phase Establish";
    case NETIF_PPP_PHASE_AUTHENTICATE: return "Phase Authenticate";
    case NETIF_PPP_PHASE_CALLBACK:     return "Phase Callback";
    case NETIF_PPP_PHASE_NETWORK:      return "Phase Network";
    case NETIF_PPP_PHASE_RUNNING:      return "Phase Running";
    case NETIF_PPP_PHASE_TERMINATE:    return "Phase Terminate";
    case NETIF_PPP_PHASE_DISCONNECT:   return "Phase Disconnect";
    case NETIF_PPP_CONNECT_FAILED:     return "Connect Failed";
    default:                           break;
  }
  return "UNKNOWN";
}

static const char *_ppp_terminal_error_name(esp_modem_terminal_error_t err) {
  switch (err) {
    case ESP_MODEM_TERMINAL_BUFFER_OVERFLOW:         return "Buffer Overflow";
    case ESP_MODEM_TERMINAL_CHECKSUM_ERROR:          return "Checksum Error";
    case ESP_MODEM_TERMINAL_UNEXPECTED_CONTROL_FLOW: return "Unexpected Control Flow";
    case ESP_MODEM_TERMINAL_DEVICE_GONE:             return "Device Gone";
    case ESP_MODEM_TERMINAL_UNKNOWN_ERROR:           return "Unknown Error";
    default:                                         break;
  }
  return "UNKNOWN";
}
#endif

static void _ppp_event_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  if (event_base == NETIF_PPP_STATUS) {
    if (_esp_modem != NULL) {
      _esp_modem->_onPppEvent(event_id, event_data);
    }
  }
}

static void onPppArduinoEvent(arduino_event_id_t event, arduino_event_info_t info) {
  if (event >= ARDUINO_EVENT_PPP_START && event <= ARDUINO_EVENT_PPP_GOT_IP6) {
    _esp_modem->_onPppArduinoEvent(event, info);
  }
}

// PPP Error Callback
static void _ppp_error_cb(esp_modem_terminal_error_t err) {
  log_v("PPP Driver Error %ld: %s", err, _ppp_terminal_error_name(err));
}

// PPP Arduino Events Callback
void PPPClass::_onPppArduinoEvent(arduino_event_id_t event, arduino_event_info_t info) {
  log_v("PPP Arduino Event %ld: %s", event, Network.eventName(event));
  // if(event == ARDUINO_EVENT_PPP_GOT_IP){
  //     if((getStatusBits() & ESP_NETIF_CONNECTED_BIT) == 0){
  //         setStatusBits(ESP_NETIF_CONNECTED_BIT);
  //         arduino_event_t arduino_event;
  //         arduino_event.event_id = ARDUINO_EVENT_PPP_CONNECTED;
  //         Network.postEvent(&arduino_event);
  //     }
  // } else
  if (event == ARDUINO_EVENT_PPP_LOST_IP) {
    if ((getStatusBits() & ESP_NETIF_CONNECTED_BIT) != 0) {
      clearStatusBits(ESP_NETIF_CONNECTED_BIT);
      arduino_event_t arduino_event;
      arduino_event.event_id = ARDUINO_EVENT_PPP_DISCONNECTED;
      Network.postEvent(&arduino_event);
    }
  }
}

// PPP Driver Events Callback
void PPPClass::_onPppEvent(int32_t event, void *event_data) {
  arduino_event_t arduino_event;
  arduino_event.event_id = ARDUINO_EVENT_MAX;

  log_v("PPP Driver Event %ld: %s", event, _ppp_event_name(event));

  if (event == NETIF_PPP_ERRORNONE) {
    if ((getStatusBits() & ESP_NETIF_CONNECTED_BIT) == 0) {
      setStatusBits(ESP_NETIF_CONNECTED_BIT);
      arduino_event_t arduino_event;
      arduino_event.event_id = ARDUINO_EVENT_PPP_CONNECTED;
      Network.postEvent(&arduino_event);
    }
  }

  if (arduino_event.event_id < ARDUINO_EVENT_MAX) {
    Network.postEvent(&arduino_event);
  }
}

esp_modem_dce_t *PPPClass::handle() const {
  return _dce;
}

PPPClass::PPPClass()
  : _dce(NULL), _pin_tx(-1), _pin_rx(-1), _pin_rts(-1), _pin_cts(-1), _flow_ctrl(ESP_MODEM_FLOW_CONTROL_NONE), _pin_rst(-1), _pin_rst_act_low(true),
    _pin_rst_delay(200), _pin(NULL), _apn(NULL), _rx_buffer_size(4096), _tx_buffer_size(512), _mode(ESP_MODEM_MODE_COMMAND), _uart_num(UART_NUM_1),
    _ppp_event_handle(0) {}

PPPClass::~PPPClass() {}

bool PPPClass::pppDetachBus(void *bus_pointer) {
  PPPClass *bus = (PPPClass *)bus_pointer;
  bus->end();
  return true;
}

void PPPClass::setResetPin(int8_t rst, bool active_low, uint32_t reset_delay) {
  _pin_rst = digitalPinToGPIONumber(rst);
  _pin_rst_act_low = active_low;
  _pin_rst_delay = reset_delay;
}

bool PPPClass::setPins(int8_t tx, int8_t rx, int8_t rts, int8_t cts, esp_modem_flow_ctrl_t flow_ctrl) {
  perimanSetBusDeinit(ESP32_BUS_TYPE_PPP_TX, PPPClass::pppDetachBus);
  perimanSetBusDeinit(ESP32_BUS_TYPE_PPP_RX, PPPClass::pppDetachBus);
  perimanSetBusDeinit(ESP32_BUS_TYPE_PPP_RTS, PPPClass::pppDetachBus);
  perimanSetBusDeinit(ESP32_BUS_TYPE_PPP_CTS, PPPClass::pppDetachBus);

  if (_pin_tx >= 0) {
    if (!perimanClearPinBus(_pin_tx)) {
      return false;
    }
  }
  if (_pin_rx >= 0) {
    if (!perimanClearPinBus(_pin_rx)) {
      return false;
    }
  }
  if (_pin_rts >= 0) {
    if (!perimanClearPinBus(_pin_rts)) {
      return false;
    }
  }
  if (_pin_cts >= 0) {
    if (!perimanClearPinBus(_pin_cts)) {
      return false;
    }
  }

  _flow_ctrl = flow_ctrl;
  _pin_tx = digitalPinToGPIONumber(tx);
  _pin_rx = digitalPinToGPIONumber(rx);
  _pin_rts = digitalPinToGPIONumber(rts);
  _pin_cts = digitalPinToGPIONumber(cts);

  if (_pin_tx >= 0) {
    if (!perimanSetPinBus(_pin_tx, ESP32_BUS_TYPE_PPP_TX, (void *)(this), -1, -1)) {
      return false;
    }
  }
  if (_pin_rx >= 0) {
    if (!perimanSetPinBus(_pin_rx, ESP32_BUS_TYPE_PPP_RX, (void *)(this), -1, -1)) {
      return false;
    }
  }
  if (_pin_rts >= 0) {
    if (!perimanSetPinBus(_pin_rts, ESP32_BUS_TYPE_PPP_RTS, (void *)(this), -1, -1)) {
      return false;
    }
  }
  if (_pin_cts >= 0) {
    if (!perimanSetPinBus(_pin_cts, ESP32_BUS_TYPE_PPP_CTS, (void *)(this), -1, -1)) {
      return false;
    }
  }
  return true;
}

bool PPPClass::begin(ppp_modem_model_t model, uint8_t uart_num, int baud_rate) {
  esp_err_t ret = ESP_OK;
  bool pin_ok = false;
  int tries = 0;

  if (_esp_netif != NULL || _dce != NULL) {
    log_w("PPP Already Started");
    return true;
  }

  if (_apn == NULL) {
    log_e("APN is not set. Call 'PPP.setApn()' first");
    return false;
  }

  if (_pin_tx < 0 || _pin_rx < 0) {
    log_e("UART pins not set. Call 'PPP.setPins()' first");
    return false;
  }

  if ((_pin_rts < 0 || _pin_cts < 0) && (_flow_ctrl != ESP_MODEM_FLOW_CONTROL_NONE)) {
    log_e("UART CTS/RTS pins not set, but flow control is enabled!");
    return false;
  }

  _uart_num = uart_num;
  _esp_modem = this;

  Network.begin();

  /* Listen for PPP status events */
  if (_ppp_ev_instance == NULL && esp_event_handler_instance_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &_ppp_event_cb, NULL, &_ppp_ev_instance)) {
    log_e("event_handler_instance_register for NETIF_PPP_STATUS Failed!");
    return false;
  }

  /* Configure the PPP netif */
  esp_netif_config_t cfg = ESP_NETIF_DEFAULT_PPP();
  _esp_netif = esp_netif_new(&cfg);
  if (_esp_netif == NULL) {
    log_e("esp_netif_new failed");
    return false;
  }

  /* Attach to receive IP events */
  initNetif(ESP_NETIF_ID_PPP);

  /* Configure the DTE */
  esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
  dte_config.uart_config.tx_io_num = _pin_tx;
  dte_config.uart_config.rx_io_num = _pin_rx;
  dte_config.uart_config.rts_io_num = _pin_rts;
  dte_config.uart_config.cts_io_num = _pin_cts;
  dte_config.uart_config.flow_control = _flow_ctrl;
  dte_config.uart_config.rx_buffer_size = _rx_buffer_size;
  dte_config.uart_config.tx_buffer_size = _tx_buffer_size;
  dte_config.uart_config.port_num = (uart_port_t)_uart_num;
  dte_config.uart_config.baud_rate = baud_rate;

  /* Configure the DCE */
  esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(_apn);

  /* Reset the Modem */
  if (_pin_rst >= 0) {
    log_v("Resetting the modem");
    if (_pin_rst_act_low) {
      pinMode(_pin_rst, OUTPUT_OPEN_DRAIN);
    } else {
      pinMode(_pin_rst, OUTPUT);
    }
    perimanSetPinBusExtraType(_pin_rst, "PPP_MODEM_RST");
    digitalWrite(_pin_rst, !_pin_rst_act_low);
    delay(_pin_rst_delay);
    digitalWrite(_pin_rst, _pin_rst_act_low);
    delay(100);
  }

  /* Start the DCE */
  _dce = esp_modem_new_dev((esp_modem_dce_device_t)model, &dte_config, &dce_config, _esp_netif);
  if (_dce == NULL) {
    log_e("esp_modem_new_dev failed");
    goto err;
  }

  esp_modem_set_error_cb(_dce, _ppp_error_cb);

  /* Wait for Modem to respond */
  if (_pin_rst >= 0) {
    // wait to be able to talk to the modem
    log_v("Waiting for response from the modem");
    while (esp_modem_sync(_dce) != ESP_OK && tries < 100) {
      tries++;
      delay(500);
    }
    if (tries >= 100) {
      log_e("Failed to wait for communication");
      goto err;
    }
  } else {
    // try to communicate with the modem
    if (esp_modem_sync(_dce) != ESP_OK) {
      log_v("Modem does not respond to AT! Switching to COMMAND mode.");
      esp_modem_set_mode(_dce, ESP_MODEM_MODE_COMMAND);
      if (esp_modem_sync(_dce) != ESP_OK) {
        log_v("Modem does not respond to AT! Switching to CMUX mode.");
        if (esp_modem_set_mode(_dce, ESP_MODEM_MODE_CMUX) != ESP_OK) {
          log_v("Modem failed to switch to CMUX mode!");
        } else {
          log_v("Switching back to COMMAND mode");
          esp_modem_set_mode(_dce, ESP_MODEM_MODE_COMMAND);
        }
      }
      if (esp_modem_sync(_dce) != ESP_OK) {
        log_e("Modem failed to respond to AT!");
        goto err;
      }
    }
  }

  /* enable flow control */
  if (dte_config.uart_config.flow_control == ESP_MODEM_FLOW_CONTROL_HW) {
    ret = esp_modem_set_flow_control(_dce, 2, 2);  //2/2 means HW Flow Control.
    if (ret != ESP_OK) {
      log_e("Failed to set the hardware flow control: [%d] %s", ret, esp_err_to_name(ret));
      goto err;
    }
  }

  /* check if PIN needed */
  if (esp_modem_read_pin(_dce, pin_ok) == ESP_OK && pin_ok == false) {
    if (_pin == NULL || _esp_modem_set_pin(_dce, _pin) != ESP_OK) {
      log_e("PIN verification failed!");
      goto err;
    }
  }

  _ppp_event_handle = Network.onSysEvent(onPppArduinoEvent);

  setStatusBits(ESP_NETIF_STARTED_BIT);
  arduino_event_t arduino_event;
  arduino_event.event_id = ARDUINO_EVENT_PPP_START;
  Network.postEvent(&arduino_event);

  return true;

err:
  PPPClass::pppDetachBus((void *)(this));
  return false;
}

void PPPClass::end(void) {
  if (_esp_modem && _esp_netif && _dce) {

    if ((getStatusBits() & ESP_NETIF_CONNECTED_BIT) != 0) {
      clearStatusBits(ESP_NETIF_CONNECTED_BIT | ESP_NETIF_HAS_IP_BIT | ESP_NETIF_HAS_LOCAL_IP6_BIT | ESP_NETIF_HAS_GLOBAL_IP6_BIT);
      arduino_event_t disconnect_event;
      disconnect_event.event_id = ARDUINO_EVENT_PPP_DISCONNECTED;
      Network.postEvent(&disconnect_event);
    }

    clearStatusBits(
      ESP_NETIF_STARTED_BIT | ESP_NETIF_CONNECTED_BIT | ESP_NETIF_HAS_IP_BIT | ESP_NETIF_HAS_LOCAL_IP6_BIT | ESP_NETIF_HAS_GLOBAL_IP6_BIT
      | ESP_NETIF_HAS_STATIC_IP_BIT
    );
    arduino_event_t arduino_event;
    arduino_event.event_id = ARDUINO_EVENT_PPP_STOP;
    Network.postEvent(&arduino_event);
  }

  destroyNetif();

  if (_ppp_ev_instance != NULL) {
    if (esp_event_handler_unregister(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &_ppp_event_cb) == ESP_OK) {
      _ppp_ev_instance = NULL;
    }
  }
  _esp_modem = NULL;

  Network.removeEvent(_ppp_event_handle);
  _ppp_event_handle = 0;

  if (_dce != NULL) {
    esp_modem_destroy(_dce);
    _dce = NULL;
  }

  int8_t pin = -1;
  if (_pin_tx != -1) {
    pin = _pin_tx;
    _pin_tx = -1;
    perimanClearPinBus(pin);
  }
  if (_pin_rx != -1) {
    pin = _pin_rx;
    _pin_rx = -1;
    perimanClearPinBus(pin);
  }
  if (_pin_rts != -1) {
    pin = _pin_rts;
    _pin_rts = -1;
    perimanClearPinBus(pin);
  }
  if (_pin_cts != -1) {
    pin = _pin_cts;
    _pin_cts = -1;
    perimanClearPinBus(pin);
  }

  _mode = ESP_MODEM_MODE_COMMAND;
}

bool PPPClass::sync() const {
  PPP_CMD_MODE_CHECK(false);

  return esp_modem_sync(_dce) == ESP_OK;
}

bool PPPClass::attached() const {
  PPP_CMD_MODE_CHECK(false);

  int m = 0;
  esp_err_t err = esp_modem_get_network_attachment_state(_dce, m);
  if (err != ESP_OK) {
    // log_e("esp_modem_get_network_attachment_state failed with %d %s", err, esp_err_to_name(err));
    return false;
  }
  return m != 0;
}

bool PPPClass::mode(esp_modem_dce_mode_t m) {
  if (_dce == NULL) {
    return 0;
  }

  if (_mode == m) {
    return true;
  }
  esp_err_t err = esp_modem_set_mode(_dce, m);
  if (err != ESP_OK) {
    log_e("esp_modem_set_mode failed with %d %s", err, esp_err_to_name(err));
    return false;
  }
  _mode = m;
  return true;
}

bool PPPClass::setApn(const char *apn) {
  if (_apn != NULL) {
    free((void *)_apn);
    _apn = NULL;
  }
  if (apn != NULL) {
    _apn = strdup(apn);
    if (_apn == NULL) {
      log_e("Failed to strdup APN");
      return false;
    }
  }
  return true;
}

bool PPPClass::setPin(const char *pin) {
  if (_pin != NULL) {
    free((void *)_pin);
    _pin = NULL;
  }
  if (pin != NULL) {
    for (int i = 0; i < strlen(pin); i++) {
      if (pin[i] < 0x30 || pin[i] > 0x39) {
        log_e("Bad character '%c' in PIN. Should be only digits", pin[i]);
        return false;
      }
    }
    _pin = strdup(pin);
    if (_pin == NULL) {
      log_e("Failed to strdup PIN");
      return false;
    }
  }
  return true;
}

int PPPClass::RSSI() const {
  PPP_CMD_MODE_CHECK(-1);

  int rssi, ber;
  esp_err_t err = esp_modem_get_signal_quality(_dce, rssi, ber);
  if (err != ESP_OK) {
    log_e("esp_modem_get_signal_quality failed with %d %s", err, esp_err_to_name(err));
    return -1;
  }
  return rssi;
}

int PPPClass::BER() const {
  PPP_CMD_MODE_CHECK(-1);

  int rssi, ber;
  esp_err_t err = esp_modem_get_signal_quality(_dce, rssi, ber);
  if (err != ESP_OK) {
    log_e("esp_modem_get_signal_quality failed with %d %s", err, esp_err_to_name(err));
    return -1;
  }
  return ber;
}

String PPPClass::IMSI() const {
  PPP_CMD_MODE_CHECK(String());

  char imsi[32];
  esp_err_t err = esp_modem_get_imsi(_dce, (std::string &)imsi);
  if (err != ESP_OK) {
    log_e("esp_modem_get_imsi failed with %d %s", err, esp_err_to_name(err));
    return String();
  }

  return String(imsi);
}

String PPPClass::IMEI() const {
  PPP_CMD_MODE_CHECK(String());

  char imei[32];
  esp_err_t err = esp_modem_get_imei(_dce, (std::string &)imei);
  if (err != ESP_OK) {
    log_e("esp_modem_get_imei failed with %d %s", err, esp_err_to_name(err));
    return String();
  }

  return String(imei);
}

String PPPClass::moduleName() const {
  PPP_CMD_MODE_CHECK(String());

  char name[32];
  esp_err_t err = esp_modem_get_module_name(_dce, (std::string &)name);
  if (err != ESP_OK) {
    log_e("esp_modem_get_module_name failed with %d %s", err, esp_err_to_name(err));
    return String();
  }

  return String(name);
}

String PPPClass::operatorName() const {
  PPP_CMD_MODE_CHECK(String());

  char oper[32];
  int act = 0;
  esp_err_t err = esp_modem_get_operator_name(_dce, (std::string &)oper, act);
  if (err != ESP_OK) {
    log_e("esp_modem_get_operator_name failed with %d %s", err, esp_err_to_name(err));
    return String();
  }

  return String(oper);
}

int PPPClass::networkMode() const {
  PPP_CMD_MODE_CHECK(-1);

  int m = 0;
  esp_err_t err = esp_modem_get_network_system_mode(_dce, m);
  if (err != ESP_OK) {
    log_e("esp_modem_get_network_system_mode failed with %d %s", err, esp_err_to_name(err));
    return -1;
  }
  return m;
}

int PPPClass::radioState() const {
  PPP_CMD_MODE_CHECK(-1);

  int m = 0;
  esp_err_t err = esp_modem_get_radio_state(_dce, m);
  if (err != ESP_OK) {
    // log_e("esp_modem_get_radio_state failed with %d %s", err, esp_err_to_name(err));
    return -1;
  }
  return m;
}

bool PPPClass::powerDown() {
  PPP_CMD_MODE_CHECK(false);

  esp_err_t err = esp_modem_power_down(_dce);
  if (err != ESP_OK) {
    log_e("esp_modem_power_down failed with %d %s", err, esp_err_to_name(err));
    return false;
  }
  return true;
}

bool PPPClass::reset() {
  PPP_CMD_MODE_CHECK(false);

  esp_err_t err = esp_modem_reset(_dce);
  if (err != ESP_OK) {
    log_e("esp_modem_reset failed with %d %s", err, esp_err_to_name(err));
    return false;
  }
  return true;
}

bool PPPClass::storeProfile() {
  PPP_CMD_MODE_CHECK(false);

  esp_err_t err = esp_modem_store_profile(_dce);
  if (err != ESP_OK) {
    log_e("esp_modem_store_profile failed with %d %s", err, esp_err_to_name(err));
    return false;
  }
  return true;
}

bool PPPClass::setBaudrate(int baudrate) {
  PPP_CMD_MODE_CHECK(false);

  esp_err_t err = esp_modem_set_baud(_dce, baudrate);
  if (err != ESP_OK) {
    log_e("esp_modem_set_baud failed with %d %s", err, esp_err_to_name(err));
    return false;
  }

  uint32_t sclk_freq;
  err = uart_get_sclk_freq(UART_SCLK_DEFAULT, &sclk_freq);
  if (err != ESP_OK) {
    log_e("uart_get_sclk_freq failed with %d %s", err, esp_err_to_name(err));
    return false;
  }

  HP_UART_SRC_CLK_ATOMIC() {
    uart_ll_set_baudrate(UART_LL_GET_HW(_uart_num), (uint32_t)baudrate, sclk_freq);
  }

  return true;
}

int PPPClass::batteryVoltage() const {
  PPP_CMD_MODE_CHECK(-1);

  int volt, bcs, bcl;
  esp_err_t err = esp_modem_get_battery_status(_dce, volt, bcs, bcl);
  if (err != ESP_OK) {
    log_e("esp_modem_get_battery_status failed with %d %s", err, esp_err_to_name(err));
    return -1;
  }
  return volt;
}

int PPPClass::batteryLevel() const {
  PPP_CMD_MODE_CHECK(-1);

  int volt, bcs, bcl;
  esp_err_t err = esp_modem_get_battery_status(_dce, volt, bcs, bcl);
  if (err != ESP_OK) {
    log_e("esp_modem_get_battery_status failed with %d %s", err, esp_err_to_name(err));
    return -1;
  }
  return bcl;
}

int PPPClass::batteryStatus() const {
  PPP_CMD_MODE_CHECK(-1);

  int volt, bcs, bcl;
  esp_err_t err = esp_modem_get_battery_status(_dce, volt, bcs, bcl);
  if (err != ESP_OK) {
    log_e("esp_modem_get_battery_status failed with %d %s", err, esp_err_to_name(err));
    return -1;
  }
  return bcs;
}

bool PPPClass::sms(const char *num, const char *message) {
  PPP_CMD_MODE_CHECK(false);

  for (int i = 0; i < strlen(num); i++) {
    if (num[i] != '+' && num[i] != '#' && num[i] != '*' && (num[i] < 0x30 || num[i] > 0x39)) {
      log_e("Bad character '%c' in SMS Number. Should be only digits and +, # or *", num[i]);
      return false;
    }
  }

  esp_err_t err = esp_modem_sms_txt_mode(_dce, true);
  if (err != ESP_OK) {
    log_e("Setting text mode failed %d %s", err, esp_err_to_name(err));
    return false;
  }

  err = esp_modem_sms_character_set(_dce);
  if (err != ESP_OK) {
    log_e("Setting GSM character set failed %d %s", err, esp_err_to_name(err));
    return false;
  }

  err = _esp_modem_send_sms(_dce, num, message);
  if (err != ESP_OK) {
    log_e("esp_modem_send_sms() failed with %d %s", err, esp_err_to_name(err));
    return false;
  }
  return true;
}

String PPPClass::cmd(const char *at_command, int timeout) {
  PPP_CMD_MODE_CHECK(String());

  char out[128] = {0};
  esp_err_t err = _esp_modem_at(_dce, at_command, out, timeout);
  if (err != ESP_OK) {
    log_e("esp_modem_at failed %d %s", err, esp_err_to_name(err));
    return String();
  }
  return String(out);
}

size_t PPPClass::printDriverInfo(Print &out) const {
  size_t bytes = 0;
  if (_dce == NULL || _mode == ESP_MODEM_MODE_DATA) {
    return bytes;
  }
  if (attached()) {
    bytes += out.print(",");
    bytes += out.print(operatorName());
  }
  bytes += out.print(",RSSI:");
  bytes += out.print(RSSI());
  bytes += out.print(",BER:");
  bytes += out.print(BER());
  return bytes;
}

PPPClass PPP;

#endif /* CONFIG_LWIP_PPP_SUPPORT */
