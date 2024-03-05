/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_err.h"

#include "esp_secure_cert_tlv_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * TLV config struct
 */
typedef struct tlv_config {
    esp_secure_cert_tlv_type_t type; /* TLV type */
    esp_secure_cert_tlv_subtype_t subtype; /* TLV subtype */
} esp_secure_cert_tlv_config_t;

/*
 * TLV info struct
 */
typedef struct tlv_info {
    esp_secure_cert_tlv_type_t type; /* Type of the TLV */
    esp_secure_cert_tlv_subtype_t subtype; /* Subtype of the TLV */
    char *data; /* Pointer to the buffer containting TLV data */
    uint32_t length; /* TLV data length */
    uint8_t flags;
} esp_secure_cert_tlv_info_t;

/*
 * TLV iterator struct
 */
typedef struct tlv_iterator {
    void *iterator; /* Opaque TLV iterator */
} esp_secure_cert_tlv_iterator_t;

/*
 *  Get the TLV information for given TLV configuration
 *
 *  @note
 *  TLV Algorithms:
 *  If the TLV data is stored with some additional encryption then it first needs to be decrypted and the decrypted data is
 *  stored in a dynamically allocated buffer. This API automatically decrypts any encryption applied to the TLV by supported algorithms.
 *  For this the API may look for TLV entries of other types which store necessary information, these TLV entries must be of the same subtype as of the subtype field in the config struct.
 *  Please see documentation regarding supported TLV storage algorithms in the TLV documentation.
 *  A call to the esp_secure_cert_free_tlv_info() should be made to free any memory allocated while populating the tlv information object.
 *  This API also validates the crc of the respective tlv before returning the offset.
 *
 *  If tlv type in the config struct is set to ESP_SECURE_CERT_TLV_END then the address returned shall be the end address of current tlv formatted data and the length returned shall be the total length of the valid TLV entries.
 * @input
 *     tlv_config           Pointer to a readable struct of type esp_secure_cert_tlv_config_t.
 *                          The contents of the struct must be already filled by the caller,
 *                          This information shall be used to find the appropriate TLV entry.
 *
 *     tlv_info             Pointer to a writable struct of type esp_secure_cert_tlv_info_t,
 *                          If TLV entry defined by tlv_config is found then the TLV information shall be populated in this struct.
 * @return
 *
 *      - ESP_OK    On success
 *      - ESP_FAIL/other relevant esp error code
 *                  On failure
 */
esp_err_t esp_secure_cert_get_tlv_info(esp_secure_cert_tlv_config_t *tlv_config, esp_secure_cert_tlv_info_t *tlv_info);

/*
 * Free the memory allocated while populating the tlv_info object
 * @note
 * Please note this does not free the tlv_info struct itself but only the memory allocated internally while populating this struct.
 */
esp_err_t esp_secure_cert_free_tlv_info(esp_secure_cert_tlv_info_t *tlv_info);

/*
 * Iterate to the next valid TLV entry
 * @note
 *       To obtain the first TLV entry, the tlv_iterator structure must be zero initialized
 * @input
 *      tlv_iterator Pointer to a readable struct of type esp_secure_cert_tlv_iterator_t
 *
 * @return
 *  ESP_OK      On success
 *              The iterator location shall be moved to point to the next TLV entry.
 *  ESP_FAIL/other relevant error codes
 *              On failure
 */
esp_err_t esp_secure_cert_iterate_to_next_tlv(esp_secure_cert_tlv_iterator_t *tlv_iterator);

/*
 * Get the TLV information from a valid iterator location
 *
 * @note
 * A call to the esp_secure_cert_free_tlv_info() should be made to free any memory allocated while populating the tlv information object.
 *
 * @input
 *     tlv_config           Pointer to a readable struct of type esp_secure_cert_tlv_iterator_t.
 *                          The iterator must be set to point to a valid TLV,
 *                          by a previous call to esp_secure_cert_iterate_to_next_tlv();.
 *
 *     tlv_info             Pointer to a writable struct of type esp_secure_cert_tlv_info_t
 *                          If TLV entry pointed by the iterator is valid then the TLV information shall be populated in this struct.
 * @return
 *  ESP_OK  On success
 *          The tlv_info object shall be populated with information of the TLV pointed by the iterator
 *  ESP_FAIL/other relevant error codes
 *          On failure
 */
esp_err_t esp_secure_cert_get_tlv_info_from_iterator(esp_secure_cert_tlv_iterator_t *tlv_iterator, esp_secure_cert_tlv_info_t *tlv_info);

/*
 * List TLV entries
 *
 * This API serially traverses through all of the available
 * TLV entries in the esp_secure_cert partition and logs
 * brief information about each TLV entry.
 */
void esp_secure_cert_list_tlv_entries(void);

#ifdef __cplusplus
}
#endif
