#pragma once
#include "dl_tool.hpp"
#include "dl_tool_cache.hpp"
#include <iostream>

namespace dl
{
    namespace layer
    {
        /**
         * @brief Base class for layer.
         * 
         */
        class Layer
        {
        public:
            char *name; /*<! name of layer >*/

            /**
             * @brief Construct a new Layer object.
             * 
             * @param name name of layer.
             */
            Layer(const char *name = NULL);

            /**
             * @brief Destroy the Layer object. Return resource.
             * 
             */
            ~Layer();
        };
    } // namespace layer
} // namespace dl

#if DL_LOG_LAYER_LATENCY
/**
 * @brief Initialize.
 */
#define DL_LOG_LAYER_LATENCY_INIT() dl::tool::Latency latency

/**
 * @brief Time starts.
 */
#define DL_LOG_LAYER_LATENCY_START() latency.start()

/**
 * @brief Time ends and printed.
 */
#define DL_LOG_LAYER_LATENCY_END(prefix, key) \
    latency.end();                            \
    latency.print(prefix, key)
#else
#define DL_LOG_LAYER_LATENCY_INIT()
#define DL_LOG_LAYER_LATENCY_START()
#define DL_LOG_LAYER_LATENCY_END(prefix, key)
#endif
