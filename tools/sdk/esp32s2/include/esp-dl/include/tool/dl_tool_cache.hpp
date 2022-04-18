#pragma once

#include <stdint.h>

#if CONFIG_IDF_TARGET_ESP32S3
#include "esp32s3/rom/cache.h"
#include "soc/extmem_reg.h"
#endif

namespace dl
{
    namespace tool
    {
        namespace cache
        {
            /**
             * @brief Initialize preload.
             * 
             * @param preload One of 1 or 0,
             *                - 1: turn on the preload
             *                - 0: turn off the preload
             * @return 
             *         - 1: Initialize successfully
             *         - 0: Initialize successfully, autoload has been turned off
             *         - -1: Initialize failed, the chip does not support preload
             */
            int8_t preload_init(uint8_t preload = 1);

            /**
             * @brief Preload memory.
             * 
             * @param addr the start address of data to be preloaded
             * @param size the size of the data in byte to be preloaded
             */
            void preload_func(uint32_t addr, uint32_t size);

            /**
             * @brief Initialize autoload. 
             * 
             * @param autoload  One of 1 or 0,
             *                  - 1: turn on the autoload
             *                  - 0: turn off the autoload
             * @param trigger   One of 0 or 1 or 2,
             *                  - 0: miss, TODO:@yuanjiong
             *                  - 1: hit, TODO:@yuanjiong
             *                  - 2: both,TODO:@yuanjiong
             * @param line_size the number of cache lines to be autoloaded
             * @return status,
             *         - 1: Initialize sucessfully
             *         - 0: Initialize suceesfully, preload has been turned off
             *         - -1: Initialize failed, the chip does not support autoload
             */
            int8_t autoload_init(uint8_t autoload = 1, uint8_t trigger = 2, uint8_t line_size = 0);

            /**
             * @brief Autoload memory.           
             * 
             * @param addr1 the start address of data1 to be autoloaded
             * @param size1 the size of the data1 in byte to be preloaded
             * @param addr2 the start address of data2 to be autoloaded
             * @param size2 the size of the data2 in byte to be preloaded
             */
            void autoload_func(uint32_t addr1, uint32_t size1, uint32_t addr2, uint32_t size2);

            /**
             * @brief Autoload memory.
             * 
             * @param addr1 the start address of data1 to be autoloaded
             * @param size1 the size of the data1 in byte to be preloaded
             */
            void autoload_func(uint32_t addr1, uint32_t size1);
        }
    } // namespace tool
} // namespace dl