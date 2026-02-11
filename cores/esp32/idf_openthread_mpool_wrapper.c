// Copyright 2024-2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*
 * OpenThread message pool platform wrapper (core).
 *
 * Provides __wrap_otPlatMessagePool* so that any build that links libopenthread
 * (OpenThread library, Matter/esp_matter, etc.) gets correct allocation
 * via -Wl,--wrap=... in platform.txt.
 *
 * Pool size matches IDF OpenThread Kconfig:
 * - CONFIG_OPENTHREAD_NUM_MESSAGE_BUFFERS default 1024 when platform pool from PSRAM
 * - CONFIG_OPENTHREAD_NUM_MESSAGE_BUFFERS default 65 when not (internal RAM)
 * So: BOARD_HAS_PSRAM -> use requested size (1024 buffers); otherwise -> 65 buffers.
 * Buffer size (aBufferSize) is unchanged; only the number of buffers is reduced
 * when PSRAM is disabled.
 */

#include "soc/soc_caps.h"

#if SOC_IEEE802154_SUPPORTED

/*
 * BOARD_HAS_PSRAM is defined by the Arduino build system (boards.txt) when the
 * user selects "PSRAM: Enabled" in the board menu (-DBOARD_HAS_PSRAM).
 */

#include <stddef.h>
#include <stdint.h>
#include <assert.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "openthread/instance.h"
#include "openthread/platform/messagepool.h"

#define OT_MPOOL_TAG "OPENTHREAD"

/* Match IDF Kconfig: 65 buffers when not using PSRAM for message pool */
/* from IDF 5.5.2 openthread component kconfig:
            config OPENTHREAD_NUM_MESSAGE_BUFFERS
                int "The number of openthread message buffers"
                default 65 if !OPENTHREAD_PLATFORM_MSGPOOL_MANAGEMENT
                default 1024 if OPENTHREAD_PLATFORM_MSGPOOL_MANAGEMENT
*/
#define OT_MPOOL_NUM_BUFFERS_NO_PSRAM 65

static int s_buffer_pool_head = -1;
static otMessageBuffer **s_buffer_pool_pointer = NULL;

void __wrap_otPlatMessagePoolInit(otInstance *aInstance, uint16_t aMinNumFreeBuffers, size_t aBufferSize)
{
    otMessageBuffer *buffer_pool;
    uint16_t num_buffers;

#ifdef BOARD_HAS_PSRAM
    const uint32_t mem_caps = MALLOC_CAP_SPIRAM;
    const char *mem_type = "PSRAM";
    num_buffers = aMinNumFreeBuffers;
    printf("OpenThread Message buffer pool: %u buffers, %u bytes (PSRAM)", num_buffers, (unsigned)(num_buffers * aBufferSize));
#else
    const uint32_t mem_caps = MALLOC_CAP_DEFAULT;
    const char *mem_type = "internal RAM";
    num_buffers = (aMinNumFreeBuffers > OT_MPOOL_NUM_BUFFERS_NO_PSRAM)
                      ? OT_MPOOL_NUM_BUFFERS_NO_PSRAM
                      : aMinNumFreeBuffers;
    printf("OpenThread Message buffer pool: %u buffers, %u bytes (internal RAM)", num_buffers, (unsigned)(num_buffers * aBufferSize));
#endif

    buffer_pool = (otMessageBuffer *)heap_caps_calloc(num_buffers, aBufferSize, mem_caps);
    s_buffer_pool_pointer = (otMessageBuffer **)heap_caps_calloc(num_buffers, sizeof(otMessageBuffer *), mem_caps);

    if (buffer_pool == NULL || s_buffer_pool_pointer == NULL) {
        ESP_LOGE(OT_MPOOL_TAG,
                 "Failed to allocate message buffer pool from %s (%u buffers x %u bytes = %u bytes)",
                 mem_type, num_buffers, (unsigned)aBufferSize,
                 (unsigned)(num_buffers * aBufferSize));
        if (buffer_pool) {
            heap_caps_free(buffer_pool);
        }
        if (s_buffer_pool_pointer) {
            heap_caps_free(s_buffer_pool_pointer);
            s_buffer_pool_pointer = NULL;
        }
        assert(false);
        return;
    }

    for (uint16_t i = 0; i < num_buffers; i++) {
        s_buffer_pool_pointer[i] = (otMessageBuffer *)((uint8_t *)buffer_pool + i * aBufferSize);
    }
    s_buffer_pool_head = (int)num_buffers - 1;

    ESP_LOGI(OT_MPOOL_TAG, "Message buffer pool: %u buffers, %u bytes (%s)",
             num_buffers, (unsigned)(num_buffers * aBufferSize), mem_type);
}

otMessageBuffer *__wrap_otPlatMessagePoolNew(otInstance *aInstance)
{
    otMessageBuffer *ret = NULL;
    if (s_buffer_pool_head >= 0) {
        ret = s_buffer_pool_pointer[s_buffer_pool_head];
        s_buffer_pool_head--;
    }
    return ret;
}

void __wrap_otPlatMessagePoolFree(otInstance *aInstance, otMessageBuffer *aBuffer)
{
    s_buffer_pool_head++;
    s_buffer_pool_pointer[s_buffer_pool_head] = aBuffer;
}

uint16_t __wrap_otPlatMessagePoolNumFreeBuffers(otInstance *aInstance)
{
    return (s_buffer_pool_head >= 0) ? (uint16_t)(s_buffer_pool_head + 1) : 0;
}

#endif /* SOC_IEEE802154_SUPPORTED */
