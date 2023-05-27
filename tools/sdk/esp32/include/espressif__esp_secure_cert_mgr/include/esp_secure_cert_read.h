/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_err.h"

#include "soc/soc_caps.h"
#ifdef CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL
#include "rsa_sign_alt.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/**
@brief Enumeration of ESP Secure Certificate key types.
*/
typedef enum key_type {
    ESP_SECURE_CERT_INVALID_KEY = -1, /* Invalid key */
    ESP_SECURE_CERT_DEFAULT_FORMAT_KEY, /* Key type for the key in default format */
    ESP_SECURE_CERT_HMAC_ENCRYPTED_KEY, /* Encrypted key type */
    ESP_SECURE_CERT_HMAC_DERIVED_ECDSA_KEY, /* HMAC-derived ECDSA key type. */
    ESP_SECURE_CERT_ECDSA_PERIPHERAL_KEY, /* ECDSA peripheral key type. */
} esp_secure_cert_key_type_t;

/* @info
 * Init the esp_secure_cert nvs partition
 *
 * @return
 *      - ESP_OK    On success
 *      - ESP_FAIL/other relevant esp error code
 *                  On failure
 */
esp_err_t esp_secure_cert_init_nvs_partition(void);

/* @info
 *  Get the device cert from the esp_secure_cert partition
 *
 *  @note
 *       If your esp_secure_cert partition is of type NVS, the API will dynamically allocate
 *       the required memory to store the device cert and return the pointer.
 *       The pointer can be freed in this case (NVS) using respective free API
 *
 *       In case of cust_flash partition, a read only flash pointer shall be returned here. This pointer should not be freed
 *
 * @params
 *      - buffer(out)       This value shall be filled with the device cert address
 *                          on successful completion
 *      - len(out)          This value shall be filled with the length of the device cert
 *                          If your esp_secure_cert partition is of type NVS, the API will dynamically allocate
 *                          the required memory to store the device cert
 *
 *                          In case of cust_flash partition, a read only flash pointer shall be returned here.
 * @return
 *      - ESP_OK    On success
 *      - ESP_FAIL/other relevant esp error code
 *                  On failure
 */
esp_err_t esp_secure_cert_get_device_cert(char **buffer, uint32_t *len);

/*
 * Free any internally allocated resources for the device cert.
 * @note
 *      This API would free the memory if it is dynamically allocated
 *
 * @params
 *      - buffer(in)        The data pointer
 *                          This pointer should be the same one which has been obtained
 *                          through "esp_secure_cert_get_device_cert" API.
 */
esp_err_t esp_secure_cert_free_device_cert(char *buffer);

/* @info
 *  Get the ca cert from the esp_secure_cert partition
 *
  * @note
 *      The API shall dynamically allocate the memory if required.
 *      The dynamic allocation of memory shall be required in following cases:
 *      1) partition type is NVS
 *      2) HMAC encryption is enabled for the API needs to be called
 *
 *      The esp_secure_cert_free_ca_cert API needs to be called in order to free the memory.
 *      The API shall only free the memory if it has been dynamically allocated.
 *
 * @params
 *      - buffer(out)       This value shall be filled with the ca cert address
 *                          on successful completion
 *      - len(out)          This value shall be filled with the length of the ca cert
 *                          If your esp_secure_cert partition is of type NVS, the API will dynamically allocate
 *                          the required memory to store the ca cert
 *
 *                          In case of cust_flash partition, a read only flash pointer shall be returned here.
 * @return
 *      - ESP_OK    On success
 *      - ESP_FAIL/other relevant esp error code
 *                  On failure
 */
esp_err_t esp_secure_cert_get_ca_cert(char **buffer, uint32_t *len);

/*
 * Free any internally allocated resources for the ca cert.
 * @note
 *      This API would free the memory if it is dynamically allocated
 *
 * @params
 *      - buffer(in)        The data pointer
 *                          This pointer should be the same one which
 *                          has been obtained through "esp_secure_cert_get_ca_cert" API.
 */
esp_err_t esp_secure_cert_free_ca_cert(char *buffer);

#ifndef CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL
/* @info
 *  Get the private key from the esp_secure_cert partition
 *
 * @note
 *      The API shall dynamically allocate the memory if required.
 *      The dynamic allocation of memory shall be required in following cases:
 *      1) partition type is NVS
 *      2) HMAC encryption is enabled for the API needs to be called
 *
 *      The esp_secure_cert_free_priv_key API needs to be called in order to free the memory.
 *      The API shall only free the memory if it has been dynamically allocated.
 *
 *      The private key(buffer) shall be returned as NULL when private key type is ESP_SECURE_CERT_ECDSA_PERIPHERAL_KEY.
 *
 * @params
 *      - buffer(out)       This value shall be filled with the private key address
 *                          on successful completion
 *      - len(out)          This value shall be filled with the length of the private key
 *
 *
 * @return
 *      - ESP_OK    On success
 *      - ESP_FAIL/other relevant esp error code
 *                  On failure
 */
esp_err_t esp_secure_cert_get_priv_key(char **buffer, uint32_t *len);

/*
 * Free any internally allocated resources for the priv key.
 * @note
 *      This API would free the memory if it is dynamically allocated
 *
 * @params
 *      - buffer(in)        The data pointer
 *                          This pointer should be the same one which
 *                          has been obtained through "esp_secure_cert_get_priv_key" API.
 */
esp_err_t esp_secure_cert_free_priv_key(char *buffer);

#else /* !CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL */
/* @info
 *       This function returns the flash esp_ds_context which can then be
 *       directly provided to an esp-tls connection through its config structure.
 *       The memory for the context is dynamically allocated.
 *
 * @params
 *      - ds_ctx    The pointer to the DS context
 * @return
 *      - NULL      On failure
 */
esp_ds_data_ctx_t *esp_secure_cert_get_ds_ctx(void);

/*
 *@info
 *      Free the ds context
 */

void esp_secure_cert_free_ds_ctx(esp_ds_data_ctx_t *ds_ctx);
#endif /* CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL */

#ifndef CONFIG_ESP_SECURE_CERT_SUPPORT_LEGACY_FORMATS

/* @info
 *  Get the private key type from the esp_secure_cert partition
 *
 * @note
 *      The API is only supported for the TLV format
 *
 * @params
 *      - priv_key_type(in/out)    Pointer to store the obtained key type
 * @return
 *      - ESP_OK    On success
 *      - ESP_FAIL/other relevant esp error code
 *                  On failure
 */
esp_err_t esp_secure_cert_get_priv_key_type(esp_secure_cert_key_type_t *priv_key_type);

/* @info
 *  Get the efuse key block id in which the private key is stored.
 * @note
 *      The API is only supported for the TLV format.
 *      For now only ECDSA type of private key can be stored in the eFuse key blocks
 *
 * @params
 *      - efuse_key_id(in/out)    Pointer to store the obtained key id
 * @return
 *      - ESP_OK    On success
 *      - ESP_FAIL/other relevant esp error code
 *                  On failure
 */
esp_err_t esp_secure_cert_get_priv_key_efuse_id(uint8_t *efuse_key_id);
#endif

#ifdef __cplusplus
}
#endif
