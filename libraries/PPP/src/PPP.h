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
    PPP_MODEM_BG96    = ESP_MODEM_DCE_BG96,
    PPP_MODEM_SIM800  = ESP_MODEM_DCE_SIM800,
#if CONFIG_ESP_MODEM_ADD_CUSTOM_MODULE
    PPP_MODEM_CUSTOM  = ESP_MODEM_DCE_CUSTOM,
#endif
    PPP_MODEM_MAX
} ppp_modem_model_t;

class PPPClass: public NetworkInterface {
    public:
        PPPClass();
        ~PPPClass();

        bool setApn(const char * apn);
        bool setPin(const char * pin);

        bool begin(ppp_modem_model_t model, int8_t tx, int8_t rx, int8_t rts=-1, int8_t cts=-1, esp_modem_flow_ctrl_t flow_ctrl=ESP_MODEM_FLOW_CONTROL_NONE);
        void end();

        // Modem DCE APIs
        int RSSI() const;
        int BER() const;
        String IMSI() const;
        String IMEI() const;
        String moduleName() const;
        String operatorName() const;
        int networkMode() const;
        int radioState() const;
        bool attached() const;

        esp_modem_dce_mode_t mode() const { return _mode; }
        bool mode(esp_modem_dce_mode_t m);

        bool powerDown();
        bool reset();
        bool storeProfile();

        bool sms(const char * num, const char * message);

        esp_modem_dce_t * handle() const;

    protected:
        size_t printDriverInfo(Print & out) const;

    public:
        void _onPppEvent(int32_t event_id, void* event_data);

    private:
        esp_modem_dce_t *_dce;
        int8_t _pin_tx;
        int8_t _pin_rx;
        int8_t _pin_rts;
        int8_t _pin_cts;
        esp_modem_flow_ctrl_t _flow_ctrl;
        const char * _pin;
        const char * _apn;
        int _rx_buffer_size;
        int _tx_buffer_size;
        esp_modem_dce_mode_t _mode;

        static bool pppDetachBus(void * bus_pointer);
};

extern PPPClass PPP;

#endif /* CONFIG_LWIP_PPP_SUPPORT */
