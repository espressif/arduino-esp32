#pragma once

#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

#include "dl_define.hpp"

extern "C"
{
#if CONFIG_TIE728_BOOST
    void dl_tie728_memset_8b(void *ptr, const int value, const int n);
    void dl_tie728_memset_16b(void *ptr, const int value, const int n);
    void dl_tie728_memset_32b(void *ptr, const int value, const int n);
#endif
}

namespace dl
{
    namespace tool
    {
        /**
         * @brief Set memory zero.
         *
         * @param ptr pointer of memory
         * @param n   byte number
         */
        void set_zero(void *ptr, const int n);

        /**
         * @brief Set array value.
         *
         * @tparam T supports all data type, sizeof(T) equals to 1, 2 and 4 will boost by instruction
         * @param ptr   pointer of array
         * @param value value to set
         * @param len   length of array
         */
        template <typename T>
        void set_value(T *ptr, const T value, const int len)
        {
#if CONFIG_TIE728_BOOST
            int *temp = (int *)&value;
            if (sizeof(T) == 1)
                dl_tie728_memset_8b(ptr, *temp, len);
            else if (sizeof(T) == 2)
                dl_tie728_memset_16b(ptr, *temp, len);
            else if (sizeof(T) == 4)
                dl_tie728_memset_32b(ptr, *temp, len);
            else
#endif
                for (size_t i = 0; i < len; i++)
                    ptr[i] = value;
        }

        /**
         * @brief Copy memory.
         *
         * @param dst pointer of destination
         * @param src pointer of source
         * @param n   byte number
         */
        void copy_memory(void *dst, void *src, const int n);

        /**
         * @brief Apply memory without initialized. Can use free_aligned() to free the memory.
         *
         * @param number number of elements
         * @param size   size of element
         * @param align  number of byte aligned, e.g., 16 means 16-byte aligned
         * @return pointer of allocated memory. NULL for failed
         */
        inline void *malloc_aligned(int number, int size, int align = 4)
        {
            assert((align > 0) && (((align & (align - 1)) == 0)));
            int total_size = number * size;

            void *res = heap_caps_aligned_alloc(align, total_size, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
#if DL_SPIRAM_SUPPORT
            if (NULL == res)
                res = heap_caps_aligned_alloc(align, total_size, MALLOC_CAP_SPIRAM);
#endif
            if (NULL == res)
            {
                printf("Fail to malloc %d bytes from DRAM(%d bytyes) and PSRAM(%d bytes), PSRAM is %s.\n",
                       total_size,
                       heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
                       heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                       DL_SPIRAM_SUPPORT ? "on" : "off");
                return NULL;
            }

            return (void *)res;
        }

        /**
         * @brief Apply memory with zero-initialized. Can use free_aligned() to free the memory.
         *
         * @param number number of elements
         * @param size   size of element
         * @param align  number of byte aligned, e.g., 16 means 16-byte aligned
         * @return pointer of allocated memory. NULL for failed
         */
        inline void *calloc_aligned(int number, int size, int align = 4)
        {

            void *aligned = malloc_aligned(number, size, align);
            set_zero(aligned, number * size);

            return (void *)aligned;
        }

        /**
         * @brief Free the calloc_aligned() and malloc_aligned() memory
         *
         * @param address pointer of memory to free
         */
        inline void free_aligned(void *address)
        {
            if (NULL == address)
                return;

            heap_caps_free(address);
        }

        /**
         * @brief Apply memory without initialized in preference order: internal aligned, internal, external aligned
         *
         * @param number number of elements
         * @param size   size of element
         * @param align  number of byte aligned, e.g., 16 means 16-byte aligned
         * @return pointer of allocated memory. NULL for failed
         */
        inline void *malloc_aligned_prefer(int number, int size, int align = 4)
        {
            assert((align > 0) && (((align & (align - 1)) == 0)));
            int total_size = number * size;
            void *res = heap_caps_aligned_alloc(align, total_size, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
            if (NULL == res)
            {
                res = heap_caps_malloc(total_size, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
            }
#if DL_SPIRAM_SUPPORT
            if (NULL == res)
            {
                res = heap_caps_aligned_alloc(align, total_size, MALLOC_CAP_SPIRAM);
            }
#endif
            if (NULL == res)
            {
                printf("Fail to malloc %d bytes from DRAM(%d bytyes) and PSRAM(%d bytes), PSRAM is %s.\n",
                       total_size,
                       heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
                       heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                       DL_SPIRAM_SUPPORT ? "on" : "off");
                return NULL;
            }

            return res;
        }

        /**
         * @brief Apply memory with zero-initialized in preference order: internal aligned, internal, external aligned
         *
         * @param number number of elements
         * @param size   size of element
         * @param align  number of byte aligned, e.g., 16 means 16-byte aligned
         * @return pointer of allocated memory. NULL for failed
         */
        inline void *calloc_aligned_prefer(int number, int size, int align = 4)
        {
            void *res = malloc_aligned_prefer(number, size, align);
            set_zero(res, number * size);

            return (void *)res;
        }

        /**
         * @brief Free the calloc_aligned_prefer() and malloc_aligned_prefer() memory
         *
         * @param address pointer of memory to free
         */
        inline void free_aligned_prefer(void *address)
        {
            if (NULL == address)
                return;

            heap_caps_free(address);
        }

        /**
         * @brief Truncate the input into int8_t range.
         *
         * @tparam T supports all integer types
         * @param output as an output
         * @param input  as an input
         */
        template <typename T>
        void truncate(int8_t &output, T input)
        {
            output = DL_CLIP(input, INT8_MIN, INT8_MAX);
        }

        /**
         * @brief Truncate the input into int16_t range.
         *
         * @tparam T supports all integer types
         * @param output as an output
         * @param input  as an input
         */
        template <typename T>
        void truncate(int16_t &output, T input)
        {
            output = DL_CLIP(input, INT16_MIN, INT16_MAX);
        }

        template <typename T>
        void truncate(int32_t &output, T input)
        {
            output = DL_CLIP(input, INT32_MIN, INT32_MAX);
        }

        template <typename T>
        void truncate(int64_t &output, T input)
        {
            output = DL_CLIP(input, INT64_MIN, INT64_MAX);
        }

        /**
         * @brief Calculate the exponent of quantizing 1/n into max_value range.
         *
         * @param n          1/n: value to be quantized
         * @param max_value  the max_range
         */
        inline int calculate_exponent(int n, int max_value)
        {
            int exp = 0;
            int tmp = 1 / n;
            while (tmp < max_value)
            {
                exp += 1;
                tmp = (1 << exp) / n;
            }
            exp -= 1;

            return exp;
        }

        /**
         * @brief Print vector in format "[x1, x2, ...]\n".
         *
         * @param array to print
         */
        inline void print_vector(std::vector<int> &array, const char *message = NULL)
        {
            if (message)
                printf("%s: ", message);

            printf("[");
            for (int i = 0; i < array.size(); i++)
            {
                printf(", %d" + (i ? 0 : 2), array[i]);
            }
            printf("]\n");
        }

        /**
         * @brief Get the cycle object
         *
         * @return cycle count
         */
        inline uint32_t get_cycle()
        {
            uint32_t ccount;
            __asm__ __volatile__("rsr %0, ccount"
                                 : "=a"(ccount)
                                 :
                                 : "memory");
            return ccount;
        }

        class Latency
        {
        private:
            const uint32_t size; /*<! size of queue */
            uint32_t *queue;     /*<! queue for storing history period */
            uint32_t period;     /*<! current period */
            uint32_t sum;        /*<! sum of period */
            uint32_t count;      /*<! the number of added period */
            uint32_t next;       /*<! point to next element in queue */
            uint32_t timestamp;  /*<! record the start >*/

        public:
            /**
             * @brief Construct a new Latency object.
             *
             * @param size
             */
            Latency(const uint32_t size = 1) : size(size),
                                               period(0),
                                               sum(0),
                                               count(0),
                                               next(0)
            {
                this->queue = (this->size > 1) ? (uint32_t *)calloc(this->size, sizeof(uint32_t)) : NULL;
            }

            /**
             * @brief Destroy the Latency object.
             *
             */
            ~Latency()
            {
                if (this->queue)
                    free(this->queue);
            }

            /**
             * @brief Record the start timestamp.
             *
             */
            void start()
            {
#if DL_LOG_LATENCY_UNIT
                this->timestamp = get_cycle();
#else
                this->timestamp = esp_timer_get_time();
#endif
            }

            /**
             * @brief Record the period.
             *
             */
            void end()
            {
#if DL_LOG_LATENCY_UNIT
                this->period = get_cycle() - this->timestamp;
#else
                this->period = esp_timer_get_time() - this->timestamp;
#endif
                if (this->queue)
                {
                    this->sum -= this->queue[this->next];
                    this->queue[this->next] = this->period;
                    this->sum += this->queue[this->next];
                    this->next++;
                    this->next = this->next % this->size;
                    if (this->count < this->size)
                    {
                        this->count++;
                    }
                }
            }

            /**
             * @brief Return the period.
             *
             * @return this->timestamp_end - this->timestamp
             */
            uint32_t get_period()
            {
                return this->period;
            }

            /**
             * @brief Get the average period.
             *
             * @return average latency
             */
            uint32_t get_average_period()
            {
                return this->queue ? (this->sum / this->count) : this->period;
            }

            /**
             * @brief Clear the period
             *
             */
            void clear_period()
            {
                this->period = 0;
            }

            /**
             * @brief Print in format "latency: {this->period} {unit}\n".
             */
            void print()
            {
#if DL_LOG_LATENCY_UNIT
                printf("latency: %15u cycle\n", this->get_average_period());
#else
                printf("latency: %15u us\n", this->get_average_period());
#endif
            }

            /**
             * @brief Print in format "{message}: {this->period} {unit}\n".
             *
             * @param message message of print
             */
            void print(const char *message)
            {
#if DL_LOG_LATENCY_UNIT
                printf("%s: %15u cycle\n", message, this->get_average_period());
#else
                printf("%s: %15u us\n", message, this->get_average_period());
#endif
            }

            /**
             * @brief Print in format "{prefix}::{key}: {this->period} {unit}\n".
             *
             * @param prefix prefix of print
             * @param key    key of print
             */
            void print(const char *prefix, const char *key)
            {
#if DL_LOG_LATENCY_UNIT
                printf("%s::%s: %u cycle\n", prefix, key, this->get_average_period());
#else
                printf("%s::%s: %u us\n", prefix, key, this->get_average_period());
#endif
            }
        };
    } // namespace tool
} // namespace dl