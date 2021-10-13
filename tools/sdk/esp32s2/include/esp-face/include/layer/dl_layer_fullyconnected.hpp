#pragma once

#include "dl_nn_fully_connected.hpp"
#include "dl_layer_base.hpp"

namespace dl
{
    namespace layer
    {
        /**
         * @brief Activation(FullyConnected(input, filter) + bias).
         * 
         * @tparam feature_t supports int16_t and int8_t,
         *         - int16_t: stands for operation in int16_t quantize
         *         - int8_t: stands for operation in int8_t quantize
         * @tparam bias_t supports int16_t and int8_t, must specify when using int8 per-channel quantization
         *         - int16_t: for int16 quantization and int8 per-channel quantization
         *         - int8_t: for int8 per-tensor quantization
         */
        template <typename feature_t, typename bias_t = feature_t>
        class FullyConnected : public Layer
        {
        private:
            const int output_exponent;               /*<! exponent of output >*/
            const bool flatten;                      /*<! true: input shape is [x1, x2, ..., xn], filter shape is [1, 1, x1 * x2 * ... * xn, output_dim], output shape is [output_dim]
                                                          false: input shape is [x1, x2, ..., xn, input_dim], filter shape is [1, 1, input_dim, output_dim], output shape is [x1, x2, ...., xn, output_dim] >*/
            const Filter<feature_t> *filter;         /*<! filter of FullyConnected >*/
            const Bias<bias_t> *bias;                /*<! bias of FullyConnected, if you don't specify anything, no bias is added >*/
            const Activation<feature_t> *activation; /*<! activation of FullyConnected, if you don't specify anything, no activation is applied >*/
            Tensor<feature_t> *output;               /*<! output ptr of FullyConnected >*/
            std::vector<int> output_shape;           /*<! output shape of FullyConnected >*/

        public:
            /**
             * @brief Construct a new FullyConnected object.
             * 
             * @param output_exponent exponent of output
             * @param filter          filter of FullyConnected
             * @param bias            bias of FullyConnected, if you don't specify anything, no bias is added
             * @param activation      activation of FullyConnected, if you don't specify anything, no activation is applied
             * @param flatten         true: input shape is [x1, x2, ..., xn], filter shape is [1, 1, x1 * x2 * ... * xn, output_dim], output shape is [output_dim]
                                      false: input shape is [x1, x2, ..., xn, input_dim], filter shape is [1, 1, input_dim, output_dim], output shape is [x1, x2, ...., xn, output_dim]
             * @param name            name of layer
             */
            FullyConnected(const int output_exponent,
                           const Filter<feature_t> *filter,
                           const Bias<bias_t> *bias = NULL,
                           const Activation<feature_t> *activation = NULL,
                           const bool flatten = true,
                           const char *name = "FullyConnected") : Layer(name),
                                                                  output_exponent(output_exponent),
                                                                  flatten(flatten),
                                                                  filter(filter),
                                                                  bias(bias),
                                                                  activation(activation),
                                                                  output_shape({})
            {
                this->output = new Tensor<feature_t>;
            }

            /**
             * @brief Destroy the FullyConnected object.
             * 
             */
            ~FullyConnected()
            {
                if (this->output != NULL)
                {
                    delete this->output;
                }
            }

            /**
             * @brief Update output padding and input padding.
             * 
             * @param input as an input
             * @param print_shape  whether to print the output shape.
             */
            void build(Tensor<feature_t> &input, bool print_shape = false)
            {
                assert(this->filter->shape.size() == 4);
                assert(this->filter->shape[0] == 1);
                assert(this->filter->shape[1] == 1);
                if (this->flatten)
                {
                    assert(input.get_size() == this->filter->shape[2]);
                    this->output_shape = {this->filter->shape[3]};
                }
                else
                {
                    assert(input.shape.back() == this->filter->shape[2]);
                    this->output_shape = input.shape;
                    this->output_shape[this->output_shape.size() - 1] = this->filter->shape[3];
                }
                this->output->set_shape(this->output_shape);
                this->output->set_exponent(this->output_exponent);
                this->output->free_element();

                if (print_shape)
                {
                    std::cout << this->name << " | ";
                    this->output->print_shape();
                }
            }

            /**
             * @brief Get the output
             * 
             * @return Tensor<feature_t>& FullyConnected result
             */
            Tensor<feature_t> &get_output()
            {
                return *this->output;
            }

            /**
             * @brief Call FullyConnected operation
             * 
             * @param input           as an input.
             * @param autoload_enable one of true or false, 
             *                        - true: load input and output from PSRAM to CACHE automatically
             *                        - false: do not
             * @param assign_core     not effective yet
             * @return FullyConnected result
             */
            Tensor<feature_t> &call(Tensor<feature_t> &input, bool autoload_enable = false, const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE)
            {
                DL_LOG_LAYER_LATENCY_INIT();

                DL_LOG_LAYER_LATENCY_START();
                if (this->output->shape != this->output_shape)
                {
                    this->output->set_shape(this->output_shape);
                }
                this->output->malloc_element();
                this->output->set_exponent(this->output_exponent);
                DL_LOG_LAYER_LATENCY_END(this->name, "apply");

                if (autoload_enable)
                {
                    dl::tool::cache::autoload_func((uint32_t)(this->output->element), this->output->get_size() * sizeof(feature_t),
                                                   (uint32_t)(input.element), input.get_size() * sizeof(feature_t));
                }

                DL_LOG_LAYER_LATENCY_START();
                nn::fully_connected(*this->output, input, *(this->filter), this->bias, this->activation, this->flatten, assign_core);
                DL_LOG_LAYER_LATENCY_END(this->name, "fully_connected");
                return *this->output;
            }

            /**
             * @brief Preload the filter to Cache.
             * NOTE: Call this layer's preload() before previous layer's call() such that filter could be loaded while previous layer is doing calculation.
             */
            void preload()
            {
                size_t size = sizeof(feature_t);
                int shape_size = this->filter->shape.size();
                for (int i = 0; i < shape_size; ++i)
                {
                    size *= filter->shape[i];
                }
                dl::tool::cache::preload_func((uint32_t)(this->filter->element), size);
            }
        };
    } // namespace layer
} // namespace dl
