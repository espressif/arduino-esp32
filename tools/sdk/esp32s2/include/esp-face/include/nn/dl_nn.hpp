#pragma once
#include <vector>
#include "dl_define.hpp"
#include "dl_tool.hpp"

namespace dl
{
    namespace nn
    {
        /**
         * @brief Get the output shape object
         * 
         * @param input_shape  input shape
         * @param filter_shape filter shape with dilation
         * @param stride_y     stride in height
         * @param stride_x     stride in width
         * @param pad_type     one of PADDING_VALID or PADDING_SAME_END or PADDING_SAME_BEGIN
         * @param is_conv2d    one of true or false,
         *                     - true: serve for Conv2D
         *                     - false: serve for other operations
         * @return std::vector<int> 
         */
        std::vector<int> get_output_shape(const std::vector<int> &input_shape, const std::vector<int> &filter_shape, const int stride_y, const int stride_x, const padding_type_t pad_type, const bool is_conv2d = false, std::vector<int> padding = {});

        /**
         * @brief Get the pad size object
         * 
         * @param output_shape output shape
         * @param input_shape  input shape
         * @param filter_shape filter shape with dilation
         * @param stride_y     stride in height
         * @param stride_x     stride in width
         * @param padding_type one of PADDING_VALID or PADDING_SAME_END or PADDING_SAME_BEGIN
         * @return padding size
         */
        std::vector<int> get_pad_size(const std::vector<int> &output_shape, const std::vector<int> &input_shape, const std::vector<int> &filter_shape, const int stride_y, const int stride_x, const padding_type_t padding_type);
    } // namespace nn
} // namespace dl

#if DL_LOG_NN_LATENCY
/**
 * @brief Initialize.
 */
#define DL_LOG_NN_LATENCY_INIT() dl::tool::Latency latency

/**
 * @brief Time starts.
 */
#define DL_LOG_NN_LATENCY_START() latency.start()

/**
 * @brief Time ends and printed.
 */
#define DL_LOG_NN_LATENCY_END(key) \
    latency.end();                 \
    latency.print("nn", key)
#else
#define DL_LOG_NN_LATENCY_INIT()
#define DL_LOG_NN_LATENCY_START()
#define DL_LOG_NN_LATENCY_END(key)
#endif
