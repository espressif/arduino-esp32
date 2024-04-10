#define ARDUINO_CORE_BUILD
#include "PPP.h"
#if CONFIG_LWIP_PPP_SUPPORT
#include "esp32-hal-periman.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include <string>

typedef struct { void * arg; } PdpContext;
#include "esp_modem_api.h"

static PPPClass * _esp_modem = NULL;
static esp_event_handler_instance_t _ppp_ev_instance = NULL;

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
static const char * _ppp_event_name(int32_t event_id){
    switch(event_id){
        case NETIF_PPP_ERRORNONE         : return "No error.";
        case NETIF_PPP_ERRORPARAM        : return "Invalid parameter.";
        case NETIF_PPP_ERROROPEN         : return "Unable to open PPP session.";
        case NETIF_PPP_ERRORDEVICE       : return "Invalid I/O device for PPP.";
        case NETIF_PPP_ERRORALLOC        : return "Unable to allocate resources.";
        case NETIF_PPP_ERRORUSER         : return "User interrupt.";
        case NETIF_PPP_ERRORCONNECT      : return "Connection lost.";
        case NETIF_PPP_ERRORAUTHFAIL     : return "Failed authentication challenge.";
        case NETIF_PPP_ERRORPROTOCOL     : return "Failed to meet protocol.";
        case NETIF_PPP_ERRORPEERDEAD     : return "Connection timeout";
        case NETIF_PPP_ERRORIDLETIMEOUT  : return "Idle Timeout";
        case NETIF_PPP_ERRORCONNECTTIME  : return "Max connect time reached";
        case NETIF_PPP_ERRORLOOPBACK     : return "Loopback detected";
        case NETIF_PPP_PHASE_DEAD        : return "Phase Dead";
        case NETIF_PPP_PHASE_MASTER      : return "Phase Master";
        case NETIF_PPP_PHASE_HOLDOFF     : return "Phase Hold Off";
        case NETIF_PPP_PHASE_INITIALIZE  : return "Phase Initialize";
        case NETIF_PPP_PHASE_SERIALCONN  : return "Phase Serial Conn";
        case NETIF_PPP_PHASE_DORMANT     : return "Phase Dormant";
        case NETIF_PPP_PHASE_ESTABLISH   : return "Phase Establish";
        case NETIF_PPP_PHASE_AUTHENTICATE: return "Phase Authenticate";
        case NETIF_PPP_PHASE_CALLBACK    : return "Phase Callback";
        case NETIF_PPP_PHASE_NETWORK     : return "Phase Network";
        case NETIF_PPP_PHASE_RUNNING     : return "Phase Running";
        case NETIF_PPP_PHASE_TERMINATE   : return "Phase Terminate";
        case NETIF_PPP_PHASE_DISCONNECT  : return "Phase Disconnect";
        case NETIF_PPP_CONNECT_FAILED    : return "Connect Failed";
        default: break;
    }
    return "UNKNOWN";
}

static const char * _ppp_terminal_error_name(esp_modem_terminal_error_t err){
    switch(err){
        case ESP_MODEM_TERMINAL_BUFFER_OVERFLOW: return "Buffer Overflow";
        case ESP_MODEM_TERMINAL_CHECKSUM_ERROR: return "Checksum Error";
        case ESP_MODEM_TERMINAL_UNEXPECTED_CONTROL_FLOW: return "Unexpected Control Flow";
        case ESP_MODEM_TERMINAL_DEVICE_GONE: return "Device Gone";
        case ESP_MODEM_TERMINAL_UNKNOWN_ERROR: return "Unknown Error";
        default: break;
    }
    return "UNKNOWN";
}
#endif

static void _ppp_event_cb(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == NETIF_PPP_STATUS){
        if(_esp_modem != NULL){
            _esp_modem->_onPppEvent(event_id, event_data);
        }
    }
}

static void onPppArduinoEvent(arduino_event_id_t event, arduino_event_info_t info)
{
    if(event >= ARDUINO_EVENT_PPP_START && event <= ARDUINO_EVENT_PPP_GOT_IP6){
        _esp_modem->_onPppArduinoEvent(event, info);
    }
}

// PPP Error Callback
static void _ppp_error_cb(esp_modem_terminal_error_t err){
    log_v("PPP Driver Error %ld: %s", err, _ppp_terminal_error_name(err));
}

// PPP Arduino Events Callback
void PPPClass::_onPppArduinoEvent(arduino_event_id_t event, arduino_event_info_t info){
    log_v("PPP Arduino Event %ld: %s", event, Network.eventName(event));
    if(event == ARDUINO_EVENT_PPP_GOT_IP){
        if((getStatusBits() & ESP_NETIF_CONNECTED_BIT) == 0){
            setStatusBits(ESP_NETIF_CONNECTED_BIT);
            arduino_event_t arduino_event;
            arduino_event.event_id = ARDUINO_EVENT_PPP_CONNECTED;
            Network.postEvent(&arduino_event);
        }
    } else 
    if(event == ARDUINO_EVENT_PPP_LOST_IP){
        if((getStatusBits() & ESP_NETIF_CONNECTED_BIT) != 0){
            clearStatusBits(ESP_NETIF_CONNECTED_BIT);
            arduino_event_t arduino_event;
            arduino_event.event_id = ARDUINO_EVENT_PPP_DISCONNECTED;
            Network.postEvent(&arduino_event);
        }
    }
}

// PPP Driver Events Callback
void PPPClass::_onPppEvent(int32_t event_id, void* event_data){
    arduino_event_t arduino_event;
    arduino_event.event_id = ARDUINO_EVENT_MAX;

    log_v("PPP Driver Event %ld: %s", event_id, _ppp_event_name(event_id));
    
    // if (event_id == ETHERNET_EVENT_CONNECTED) {
    //     log_v("%s Connected", desc());
    //     arduino_event.event_id = ARDUINO_EVENT_PPP_CONNECTED;
    //     arduino_event.event_info.eth_connected = handle();
    //     setStatusBits(ESP_NETIF_CONNECTED_BIT);
    // } else if (event_id == ETHERNET_EVENT_DISCONNECTED) {
    //     log_v("%s Disconnected", desc());
    //     arduino_event.event_id = ARDUINO_EVENT_PPP_DISCONNECTED;
    //     clearStatusBits(ESP_NETIF_CONNECTED_BIT | ESP_NETIF_HAS_IP_BIT | ESP_NETIF_HAS_LOCAL_IP6_BIT | ESP_NETIF_HAS_GLOBAL_IP6_BIT);
    // } else if (event_id == ETHERNET_EVENT_START) {
    //     log_v("%s Started", desc());
    //     arduino_event.event_id = ARDUINO_EVENT_PPP_START;
    //     setStatusBits(ESP_NETIF_STARTED_BIT);
    // } else if (event_id == ETHERNET_EVENT_STOP) {
    //     log_v("%s Stopped", desc());
    //     arduino_event.event_id = ARDUINO_EVENT_PPP_STOP;
    //     clearStatusBits(ESP_NETIF_STARTED_BIT | ESP_NETIF_CONNECTED_BIT | ESP_NETIF_HAS_IP_BIT | ESP_NETIF_HAS_LOCAL_IP6_BIT | ESP_NETIF_HAS_GLOBAL_IP6_BIT | ESP_NETIF_HAS_STATIC_IP_BIT);
    // }

    if(arduino_event.event_id < ARDUINO_EVENT_MAX){
        Network.postEvent(&arduino_event);
    }
}

esp_modem_dce_t * PPPClass::handle() const {
    return _dce;
}

PPPClass::PPPClass()
    :_dce(NULL)
    ,_pin_tx(-1)
    ,_pin_rx(-1)
    ,_pin_rts(-1)
    ,_pin_cts(-1)
    ,_flow_ctrl(ESP_MODEM_FLOW_CONTROL_NONE)
    ,_pin_rst(-1)
    ,_pin_rst_act_low(true)
    ,_pin(NULL)
    ,_apn(NULL)
    ,_rx_buffer_size(4096)
    ,_tx_buffer_size(512)
    ,_mode(ESP_MODEM_MODE_COMMAND)
{
}

PPPClass::~PPPClass()
{}

bool PPPClass::pppDetachBus(void * bus_pointer){
    PPPClass *bus = (PPPClass *) bus_pointer;
    bus->end();
    return true;
}

void PPPClass::setResetPin(int8_t rst, bool active_low){
    _pin_rst = digitalPinToGPIONumber(rst);
    _pin_rst_act_low = active_low;
}

bool PPPClass::setPins(int8_t tx, int8_t rx, int8_t rts, int8_t cts, esp_modem_flow_ctrl_t flow_ctrl){
    perimanSetBusDeinit(ESP32_BUS_TYPE_PPP_TX, PPPClass::pppDetachBus);
    perimanSetBusDeinit(ESP32_BUS_TYPE_PPP_RX, PPPClass::pppDetachBus);
    perimanSetBusDeinit(ESP32_BUS_TYPE_PPP_RTS, PPPClass::pppDetachBus);
    perimanSetBusDeinit(ESP32_BUS_TYPE_PPP_CTS, PPPClass::pppDetachBus);

    if(_pin_tx >= 0){
        if(!perimanClearPinBus(_pin_tx)){ return false; }
    }
    if(_pin_rx >= 0){
        if(!perimanClearPinBus(_pin_rx)){ return false; }
    }
    if(_pin_rts >= 0){
        if(!perimanClearPinBus(_pin_rts)){ return false; }
    }
    if(_pin_cts >= 0){
        if(!perimanClearPinBus(_pin_cts)){ return false; }
    }

    _flow_ctrl = flow_ctrl;
    _pin_tx = digitalPinToGPIONumber(tx);
    _pin_rx = digitalPinToGPIONumber(rx);
    _pin_rts = digitalPinToGPIONumber(rts);
    _pin_cts = digitalPinToGPIONumber(cts);

    if(_pin_tx >= 0){
        if(!perimanSetPinBus(_pin_tx, ESP32_BUS_TYPE_PPP_TX, (void *)(this), -1, -1)){ return false; }
    }
    if(_pin_rx >= 0){
        if(!perimanSetPinBus(_pin_rx, ESP32_BUS_TYPE_PPP_RX, (void *)(this), -1, -1)){ return false; }
    }
    if(_pin_rts >= 0){
        if(!perimanSetPinBus(_pin_rts,  ESP32_BUS_TYPE_PPP_RTS, (void *)(this), -1, -1)){ return false; }
    }
    if(_pin_cts >= 0){
        if(!perimanSetPinBus(_pin_cts,  ESP32_BUS_TYPE_PPP_CTS, (void *)(this), -1, -1)){ return false; }
    }
    return true;
}

bool PPPClass::begin(ppp_modem_model_t model){
    esp_err_t ret = ESP_OK;
    bool pin_ok = false;
    int trys = 0;

    if(_esp_netif != NULL || _dce != NULL){
        log_w("PPP Already Started");
        return true;
    }

    if(_apn == NULL){
        log_e("APN is not set. Call 'PPP.setApn()' first");
        return false;
    }

    if(_pin_tx < 0 || _pin_rx < 0){
        log_e("UART pins not set. Call 'PPP.setPins()' first");
        return false;
    }

    if((_pin_rts < 0 || _pin_cts < 0) && (_flow_ctrl != ESP_MODEM_FLOW_CONTROL_NONE)){
        log_e("UART CTS/RTS pins not set, but flow control is enabled!");
        return false;
    }

    Network.begin();
    _esp_modem = this;
    if(_ppp_ev_instance == NULL && esp_event_handler_instance_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &_ppp_event_cb, NULL, &_ppp_ev_instance)){
        log_e("event_handler_instance_register for NETIF_PPP_STATUS Failed!");
        return false;
    }

    /* Configure the PPP netif */
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_PPP();
    _esp_netif = esp_netif_new(&cfg);
    if(_esp_netif == NULL){
        log_e("esp_netif_new failed");
        return false;
    }

    /* attach to receive events */
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

    /* Configure the DCE */
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(_apn);

    /* Reset the Modem */
    if(_pin_rst >= 0){
        log_v("Resetting the modem");
        if(_pin_rst_act_low){
            pinMode(_pin_rst, OUTPUT_OPEN_DRAIN);
        } else {
            pinMode(_pin_rst, OUTPUT);
        }
        digitalWrite(_pin_rst, !_pin_rst_act_low);
        delay(200);
        digitalWrite(_pin_rst, _pin_rst_act_low);
        delay(100);
    }

    /* Start the DCE */
    _dce = esp_modem_new_dev((esp_modem_dce_device_t)model, &dte_config, &dce_config, _esp_netif);
    if(_dce == NULL){
        log_e("esp_modem_new_dev failed");
        goto err;
    }

    esp_modem_set_error_cb(_dce, _ppp_error_cb);

    /* Wait for Modem to respond */
    if(_pin_rst >= 0){
        // wait to be able to talk to the modem
        log_v("Waiting for response from the modem");
        while(esp_modem_sync(_dce) != ESP_OK && trys < 50){
            trys++;
            delay(500);
        }
        if(trys >= 50){
            log_e("Failed to wait for communication");
            goto err;
        }
    } else {
        // try to communicate with the modem
        
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
        if (_pin == NULL || esp_modem_set_pin(_dce, _pin) != ESP_OK) {
            log_e("PIN verification failed!");
            goto err;
        }
    }

    Network.onSysEvent(onPppArduinoEvent);

    setStatusBits(ESP_NETIF_STARTED_BIT);
    arduino_event_t arduino_event;
    arduino_event.event_id = ARDUINO_EVENT_PPP_START;
    Network.postEvent(&arduino_event);

    return true;

err:
    PPPClass::pppDetachBus((void *)(this));
    return false;
}

void PPPClass::end(void)
{
    if(_esp_modem && _esp_netif && _dce){

        if((getStatusBits() & ESP_NETIF_CONNECTED_BIT) != 0){
            clearStatusBits(ESP_NETIF_CONNECTED_BIT | ESP_NETIF_HAS_IP_BIT | ESP_NETIF_HAS_LOCAL_IP6_BIT | ESP_NETIF_HAS_GLOBAL_IP6_BIT);
            arduino_event_t disconnect_event;
            disconnect_event.event_id = ARDUINO_EVENT_PPP_DISCONNECTED;
            Network.postEvent(&disconnect_event);
        }

        clearStatusBits(ESP_NETIF_STARTED_BIT | ESP_NETIF_CONNECTED_BIT | ESP_NETIF_HAS_IP_BIT | ESP_NETIF_HAS_LOCAL_IP6_BIT | ESP_NETIF_HAS_GLOBAL_IP6_BIT | ESP_NETIF_HAS_STATIC_IP_BIT);
        arduino_event_t arduino_event;
        arduino_event.event_id = ARDUINO_EVENT_PPP_STOP;
        Network.postEvent(&arduino_event);
    }

    destroyNetif();

    if(_ppp_ev_instance != NULL){
        if(esp_event_handler_unregister(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &_ppp_event_cb) == ESP_OK){
            _ppp_ev_instance = NULL;
        }
    }
    _esp_modem = NULL;

    Network.removeEvent(onPppArduinoEvent);

    if(_dce != NULL){
        esp_modem_destroy(_dce);
        _dce = NULL;
    }

    if(_pin_tx != -1){
        perimanClearPinBus(_pin_tx);
        _pin_tx = -1;
    }
    if(_pin_rx != -1){
        perimanClearPinBus(_pin_rx);
        _pin_rx = -1;
    }
    if(_pin_rts != -1){
        perimanClearPinBus(_pin_rts);
        _pin_rts = -1;
    }
    if(_pin_cts != -1){
        perimanClearPinBus(_pin_cts);
        _pin_cts = -1;
    }

    _mode = ESP_MODEM_MODE_COMMAND;
}

bool PPPClass::sync() const
{
    if(_dce == NULL){
        return false;
    }

    if(_mode == ESP_MODEM_MODE_DATA){
        log_e("Wrong modem mode. Should be ESP_MODEM_MODE_COMMAND");
        return false;
    }
    return esp_modem_sync(_dce) == ESP_OK;
}

bool PPPClass::attached() const
{
    if(_dce == NULL){
        return false;
    }

    if(_mode == ESP_MODEM_MODE_DATA){
        log_e("Wrong modem mode. Should be ESP_MODEM_MODE_COMMAND");
        return false;
    }

    int m = 0;
    esp_err_t err = esp_modem_get_network_attachment_state(_dce, m);
    if (err != ESP_OK) {
        // log_e("esp_modem_get_network_attachment_state failed with %d %s", err, esp_err_to_name(err));
        return false;
    }
    return m != 0;
}

bool PPPClass::mode(esp_modem_dce_mode_t m){
    if(_dce == NULL){
        return 0;
    }

    if(_mode == m){
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

bool PPPClass::setApn(const char * apn){
    if(_apn != NULL){
        free((void *)_apn);
        _apn = NULL;
    }
    if(apn != NULL){
        _apn = strdup(apn);
        if(_apn == NULL){
            log_e("Failed to strdup APN");
            return false;
        }
    }
    return true;
}

bool PPPClass::setPin(const char * pin){
    if(_pin != NULL){
        free((void *)_pin);
        _pin = NULL;
    }
    if(pin != NULL){
        for(int i=0; i<strlen(pin); i++){
            if(pin[i] < 0x30 || pin[i] > 0x39){
                log_e("Bad character '%c' in PIN. Should be only digits", pin[i]);
                return false;
            }
        }
        _pin = strdup(pin);
        if(_pin == NULL){
            log_e("Failed to strdup PIN");
            return false;
        }
    }
    return true;
}

int PPPClass::RSSI() const
{
    if(_dce == NULL){
        return -1;
    }

    if(_mode == ESP_MODEM_MODE_DATA){
        log_e("Wrong modem mode. Should be ESP_MODEM_MODE_COMMAND");
        return -1;
    }

    int rssi, ber;
    esp_err_t err = esp_modem_get_signal_quality(_dce, rssi, ber);
    if (err != ESP_OK) {
        // log_e("esp_modem_get_signal_quality failed with %d %s", err, esp_err_to_name(err));
        return -1;
    }
    return rssi;
}

int PPPClass::BER() const
{
    if(_dce == NULL){
        return -1;
    }

    if(_mode == ESP_MODEM_MODE_DATA){
        log_e("Wrong modem mode. Should be ESP_MODEM_MODE_COMMAND");
        return -1;
    }

    int rssi, ber;
    esp_err_t err = esp_modem_get_signal_quality(_dce, rssi, ber);
    if (err != ESP_OK) {
        // log_e("esp_modem_get_signal_quality failed with %d %s", err, esp_err_to_name(err));
        return -1;
    }
    return ber;
}

String PPPClass::IMSI() const
{
    if(_dce == NULL){
        return String();
    }

    if(_mode == ESP_MODEM_MODE_DATA){
        log_e("Wrong modem mode. Should be ESP_MODEM_MODE_COMMAND");
        return String();
    }

    char imsi[32];
    esp_err_t err = esp_modem_get_imsi(_dce, (std::string&)imsi);
    if (err != ESP_OK) {
        log_e("esp_modem_get_imsi failed with %d %s", err, esp_err_to_name(err));
        return String();
    }

    return String(imsi);
}

String PPPClass::IMEI() const
{
    if(_dce == NULL){
        return String();
    }

    if(_mode == ESP_MODEM_MODE_DATA){
        log_e("Wrong modem mode. Should be ESP_MODEM_MODE_COMMAND");
        return String();
    }

    char imei[32];
    esp_err_t err = esp_modem_get_imei(_dce, (std::string&)imei);
    if (err != ESP_OK) {
        log_e("esp_modem_get_imei failed with %d %s", err, esp_err_to_name(err));
        return String();
    }

    return String(imei);
}

String PPPClass::moduleName() const
{
    if(_dce == NULL){
        return String();
    }

    if(_mode == ESP_MODEM_MODE_DATA){
        log_e("Wrong modem mode. Should be ESP_MODEM_MODE_COMMAND");
        return String();
    }

    char name[32];
    esp_err_t err = esp_modem_get_module_name(_dce, (std::string&)name);
    if (err != ESP_OK) {
        log_e("esp_modem_get_module_name failed with %d %s", err, esp_err_to_name(err));
        return String();
    }

    return String(name);
}

String PPPClass::operatorName() const
{
    if(_dce == NULL){
        return String();
    }

    if(_mode == ESP_MODEM_MODE_DATA){
        log_e("Wrong modem mode. Should be ESP_MODEM_MODE_COMMAND");
        return String();
    }

    char oper[32];
    int act = 0;
    esp_err_t err = esp_modem_get_operator_name(_dce, (std::string&)oper, act);
    if (err != ESP_OK) {
        log_e("esp_modem_get_operator_name failed with %d %s", err, esp_err_to_name(err));
        return String();
    }

    return String(oper);
}

int PPPClass::networkMode() const
{
    if(_dce == NULL){
        return -1;
    }

    if(_mode == ESP_MODEM_MODE_DATA){
        log_e("Wrong modem mode. Should be ESP_MODEM_MODE_COMMAND");
        return -1;
    }

    int m = 0;
    esp_err_t err = esp_modem_get_network_system_mode(_dce, m);
    if (err != ESP_OK) {
        log_e("esp_modem_get_network_system_mode failed with %d %s", err, esp_err_to_name(err));
        return -1;
    }
    return m;
}

int PPPClass::radioState() const
{
    if(_dce == NULL){
        return -1;
    }

    if(_mode == ESP_MODEM_MODE_DATA){
        log_e("Wrong modem mode. Should be ESP_MODEM_MODE_COMMAND");
        return -1;
    }

    int m = 0;
    esp_err_t err = esp_modem_get_radio_state(_dce, m);
    if (err != ESP_OK) {
        // log_e("esp_modem_get_radio_state failed with %d %s", err, esp_err_to_name(err));
        return -1;
    }
    return m;
}

bool PPPClass::powerDown(){
    if(_dce == NULL){
        return false;
    }

    if(_mode == ESP_MODEM_MODE_DATA){
        log_e("Wrong modem mode. Should be ESP_MODEM_MODE_COMMAND");
        return false;
    }

    esp_err_t err = esp_modem_power_down(_dce);
    if (err != ESP_OK) {
        log_e("esp_modem_power_down failed with %d %s", err, esp_err_to_name(err));
        return false;
    }
    return true;
}

bool PPPClass::reset(){
    if(_dce == NULL){
        return false;
    }

    if(_mode == ESP_MODEM_MODE_DATA){
        log_e("Wrong modem mode. Should be ESP_MODEM_MODE_COMMAND");
        return false;
    }

    esp_err_t err = esp_modem_reset(_dce);
    if (err != ESP_OK) {
        log_e("esp_modem_reset failed with %d %s", err, esp_err_to_name(err));
        return false;
    }
    return true;
}

bool PPPClass::storeProfile(){
    if(_dce == NULL){
        return false;
    }

    if(_mode == ESP_MODEM_MODE_DATA){
        log_e("Wrong modem mode. Should be ESP_MODEM_MODE_COMMAND");
        return false;
    }

    esp_err_t err = esp_modem_store_profile(_dce);
    if (err != ESP_OK) {
        log_e("esp_modem_store_profile failed with %d %s", err, esp_err_to_name(err));
        return false;
    }
    return true;
}


bool PPPClass::sms(const char * num, const char * message) {
    if(_dce == NULL){
        return false;
    }

    if(_mode == ESP_MODEM_MODE_DATA){
        log_e("Wrong modem mode. Should be ESP_MODEM_MODE_COMMAND");
        return false;
    }

    for(int i=0; i<strlen(num); i++){
        if(num[i] != '+' && num[i] != '#' && num[i] != '*' && (num[i] < 0x30 || num[i] > 0x39)){
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

    err = esp_modem_send_sms(_dce, num, message);
    if (err != ESP_OK) {
        log_e("esp_modem_send_sms() failed with %d %s", err, esp_err_to_name(err));
        return false;
    }
    return true;
}

size_t PPPClass::printDriverInfo(Print & out) const {
    size_t bytes = 0;
    if(_dce == NULL || _mode == ESP_MODEM_MODE_DATA){
        return bytes;
    }
    if(attached()){
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
