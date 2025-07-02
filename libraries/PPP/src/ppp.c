#include "sdkconfig.h"
#if CONFIG_LWIP_PPP_SUPPORT && defined __has_include && __has_include("esp_modem_api.h")
#include "esp_modem_api.h"

esp_err_t _esp_modem_at(esp_modem_dce_t *dce_wrap, const char *at, char *p_out, int timeout) {
  return esp_modem_at(dce_wrap, at, p_out, timeout);
}

esp_err_t _esp_modem_send_sms(esp_modem_dce_t *dce_wrap, const char *number, const char *message) {
  return esp_modem_send_sms(dce_wrap, number, message);
}

esp_err_t _esp_modem_set_pin(esp_modem_dce_t *dce_wrap, const char *pin) {
  return esp_modem_set_pin(dce_wrap, pin);
}

esp_err_t _esp_modem_at_raw(esp_modem_dce_t *dce_wrap, const char *cmd, char *p_out, const char *pass, const char *fail, int timeout) {
  return esp_modem_at_raw(dce_wrap, cmd, p_out, pass, fail, timeout);
}

esp_err_t _esp_modem_set_operator(esp_modem_dce_t *dce_wrap, int mode, int format, const char *oper) {
  return esp_modem_set_operator(dce_wrap, mode, format, oper);
}

esp_err_t _esp_modem_set_network_bands(esp_modem_dce_t *dce_wrap, const char *mode, const int *bands, int size) {
  return esp_modem_set_network_bands(dce_wrap, mode, bands, size);
}
#endif  // CONFIG_LWIP_PPP_SUPPORT
