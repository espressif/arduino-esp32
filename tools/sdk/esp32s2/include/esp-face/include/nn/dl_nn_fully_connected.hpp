#pragma once

#include "dl_constant.hpp"
#include "dl_variable.hpp"
#include "dl_nn.hpp"

namespace dl
{
    namespace nn
    {
        /**
         * @brief activation(FullyConnected(input, filter) + bias).
         * 
         * @param output      as an output
         * @param input       as an input
         * @param filter      filter of FullyConnected
         * @param bias        bias of FullyConnected, if you don't specify anything, no bias is added
         * @param activation  activation of FullyConnected, if you don't specify anything, no activation is applied
         * @param flatten     true: input shape is [x1, x2, ..., xn], filter shape is [1, 1, x1 * x2 * ... * xn, output_dim], output shape is [output_dim]
         *                    false: input shape is [x1, x2, ..., xn, input_dim], filter shape is [1, 1, input_dim, output_dim], output shape is [x1, x2, ...., xn, output_dim]
         * @param assign_core not effective yet
         */
        void fully_connected(Tensor<int16_t> &output,
                             Tensor<int16_t> &input,
                             const Filter<int16_t> &filter,
                             const Bias<int16_t> *const bias = NULL,
                             const Activation<int16_t> *const activation = NULL,
                             const bool flatten = true,
                             const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE);

        /**
         * @brief activation(FullyConnected(input, filter) + bias).
         * 
         * @param output      as an output
         * @param input       as an input
         * @param filter      filter of FullyConnected
         * @param bias        bias of FullyConnected, if you don't specify anything, no bias is added
         * @param activation  activation of FullyConnected, if you don't specify anything, no activation is applied
         * @param flatten     true: input shape is [x1, x2, ..., xn], filter shape is [1, 1, x1 * x2 * ... * xn, output_dim], output shape is [output_dim]
         *                    false: input shape is [x1, x2, ..., xn, input_dim], filter shape is [1, 1, input_dim, output_dim], output shape is [x1, x2, ...., xn, output_dim]
         * @param assign_core not effective yet
         */
        void fully_connected(Tensor<int8_t> &output,
                             Tensor<int8_t> &input,
                             const Filter<int8_t> &filter,
                             const Bias<int8_t> *const bias = NULL,
                             const Activation<int8_t> *const activation = NULL,
                             const bool flatten = true,
                             const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE);

        /**
         * @brief activation(FullyConnected(input, filter) + bias).
         * 
         * @param output      as an output
         * @param input       as an input
         * @param filter      filter of FullyConnected
         * @param bias        bias of FullyConnected, if you don't specify anything, no bias is added
         * @param activation  activation of FullyConnected, if you don't specify anything, no activation is applied
         * @param flatten     true: input shape is [x1, x2, ..., xn], filter shape is [1, 1, x1 * x2 * ... * xn, output_dim], output shape is [output_dim]
         *                    false: input shape is [x1, x2, ..., xn, input_dim], filter shape is [1, 1, input_dim, output_dim], output shape is [x1, x2, ...., xn, output_dim]
         * @param assign_core not effective yet
         */
        void fully_connected(Tensor<int8_t> &output,
                             Tensor<int8_t> &input,
                             const Filter<int8_t> &filter,
                             const Bias<int16_t> *const bias = NULL,
                             const Activation<int8_t> *const activation = NULL,
                             const bool flatten = true,
                             const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE);

        /**
         * @brief activation(FullyConnected(input, filter) + bias).
         * 
         * @tparam feature_t supports int16_t and int8_t,
         *         - int16_t: stands for operation in int16_t quantize
         *         - int8_t: stands for operation in int8_t quantize
         * @param output_exponent exponent of output
         * @param input           as an input
         * @param filter          Filter of FullyConnected
         * @param bias            bias of FullyConnected, if you don't specify anything, no bias is added
         * @param activation      activation of FullyConnected, if you don't specify anything, no activation is applied
         * @param flatten         true: input shape is [x1, x2, ..., xn], filter shape is [1, 1, x1 * x2 * ... * xn, output_dim], output shape is [output_dim]
         *                        false: input shape is [x1, x2, ..., xn, input_dim], filter shape is [1, 1, input_dim, output_dim], output shape is [x1, x2, ...., xn, output_dim]
         * @param assign_core     not effective yet
         * @return FullyConnected result
         */
        template <typename feature_t>
        Tensor<feature_t> fully_connected(const int output_exponent,
                                          Tensor<feature_t> &input,
                                          const Filter<feature_t> &filter,
                                          const Bias<feature_t> *bias,
                                          const Activation<feature_t> *activation,
                                          const bool flatten,
                                          const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE)
        {
            DL_LOG_NN_LATENCY_INIT();

            DL_LOG_NN_LATENCY_START();
            assert(filter.shape.size() == 4);
            assert(filter.shape[0] == 1);
            assert(filter.shape[1] == 1);

            std::vector<int> output_shape;
            if (flatten)
            {
                assert(input.get_size() == filter.shape[2]);
                output_shape = {filter.shape.back()};
            }
            else
            {
                assert(input.shape.back() == filter->shape[2]);
                output_shape = input.shape;
                output_shape[output_shape.size() - 1] = filter.shape.back();
            }
            Tensor<feature_t> output;
            output.set_exponent(output_exponent).set_shape(output_shape).malloc_element();
            DL_LOG_NN_LATENCY_END("apply");

            DL_LOG_NN_LATENCY_START();
            fully_connected(output, input, filter, bias, activation, flatten, assign_core);
            DL_LOG_NN_LATENCY_END("fully_connected");

            return output;
        }
    } // namespace nn
} // namespace dl