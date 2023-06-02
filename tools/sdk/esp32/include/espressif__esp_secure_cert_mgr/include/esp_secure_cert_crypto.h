/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "esp_err.h"
#include "soc/soc_caps.h"

#ifdef SOC_HMAC_SUPPORTED
#include "esp_hmac.h"
/*
 * @info
 * PBKDF2_hmac- password based key derivation function based on HMAC.
 * The API acts as a password based key derivation function by using the hardware HMAC peripheral present on the SoC. The hmac key shall be used from the efuse key block provided as the hmac_key_id. The API makes use of the hardware HMAC peripheral and hardware SHA peripheral present on the SoC. The SHA operation is limited to SHA256.
 * @input
 * hmac_key_id      The efuse_key_id in which the HMAC key has already been burned.
 *                  This key should be read and write protected for its protection. That way it can only be accessed by the hardware HMAC peripheral.
 * salt             The buffer containing the salt value
 * salt_len         The length of the salt in bytes
 * key_length       The expected length of the derived key.
 * iteration_count  The count for which the internal cryptographic operation shall be repeated.
 * output           The pointer to the buffer in which the derived key shall be stored. It must of a writable buffer of size key_length bytes
 *
 */
int esp_pbkdf2_hmac_sha256(hmac_key_id_t hmac_key_id, const unsigned char *salt, size_t salt_len,
                           size_t iteration_count, size_t key_length, unsigned char *output);
#endif
