#pragma once

#include "dl_nn_conv2d.hpp"
#include "dl_layer_base.hpp"

namespace dl
{
    namespace layer
    {
        /**
         * @brief Activation(Conv2D(input, filter) + bias).
         * 
         * @tparam feature_t supports int16_t and int8_t,
         *         - int16_t: stands for operation in int16_t quantize
         *         - int8_t: stands for operation in int8_t quantize
         * @tparam bias_t supports int16_t and int8_t, must specify when using int8 per-channel quantization
         *         - int16_t: for int16 quantization and int8 per-channel quantization
         *         - int8_t: for int8 per-tensor quantization
         */
        template <typename feature_t, typename bias_t = feature_t>
        class Conv2D : public Layer
        {
        private:
            const int output_exponent;               /*<! exponent of output >*/
            const Filter<feature_t> *filter;         /*<! filter of Conv2D >*/
            const int stride_y;                      /*<! stride in height >*/
            const int stride_x;                      /*<! stride in width >*/
            const padding_type_t padding_type;       /*<! one of PADDING_VALID or PADDING_SAME_END or PADDING_SAME_BEGIN >*/
            const Bias<bias_t> *bias;                /*<! bias of Conv2D, if you don't specify anything, no bias is added >*/
            const Activation<feature_t> *activation; /*<! activation of Conv2D, if you don't specify anything, no activation is applied >*/
            std::vector<int> padding;                /*<! padding size needed in [top, bottom, left, right] of this operation >*/
            Tensor<feature_t> *output;               /*<! output ptr of Conv2D >*/
            std::vector<int> output_shape;           /*<! output shape of Conv2D >*/

        public:
            /**
             * @brief Construct a new Conv2D object.
             * 
             * @param output_exponent exponent of output
             * @param filter          filter of Conv2D
             * @param bias            bias of Conv2D, if you don't specify anything, no bias is added
             * @param activation      activation of Conv2D, if you don't specify anything, no activation is applied
             * @param padding_type    one of PADDING_VALID or PADDING_SAME_END or PADDING_SAME_BEGIN or PADDING_NOT_SET,
             *                        - PADDING_VALID means no padding
             *                        PADDING_SAME_END and PADDING_SAME_BEGIN results in padding with zeros evenly to the left/right or up/down of the input 
             *                        such that output has the same height/width dimension as the input,
             *                        - PADDING_SAME_END results padding in TensorFlow style
             *                        - PADDING_SAME_BEGIN results padding in MXNET style
             *                        - PADDING_NOT_SET means padding with the specific "padding" value below. 
             * @param padding         if padding_type is PADDING_NOT_SET, this value will be used as padding size. 
             *                        the shape must be 4, the value of each position is: [padding top, padding bottom, padding left, padding right]
             * @param stride_y        stride in height
             * @param stride_x        stride in width
             * @param name            name of layer
             */
            Conv2D(const int output_exponent,
                   const Filter<feature_t> *filter,
                   const Bias<bias_t> *bias = NULL,
                   const Activation<feature_t> *activation = NULL,
                   const padding_type_t padding_type = PADDING_VALID,
                   std::vector<int> padding = {},
                   const int stride_y = 1,
                   const int stride_x = 1,
                   const char *name = "Conv2D") : Layer(name),
                                                  output_exponent(output_exponent),
                                                  filter(filter),
                                                  stride_y(stride_y),
                                                  stride_x(stride_x),
                                                  padding_type(padding_type),
                                                  bias(bias),
                                                  activation(activation),
                                                  padding(padding),
                                                  output_shape({})
            {
                this->output = new Tensor<feature_t>;
                if (this->padding_type == PADDING_NOT_SET)
                {
                    assert(this->padding.size() == 4);
                }
            }

            /**
             * @brief Destroy the Conv2D object.
             * 
             */
            ~Conv2D()
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
                assert(input.shape[0] > 0);
                assert(input.shape[1] > 0);
                assert(input.shape.size() == 3);
                assert(this->filter->shape.size() == 4);
                assert(input.shape[2] == this->filter->shape[2]);

                this->output_shape = nn::get_output_shape(input.shape, this->filter->shape_with_dilation, this->stride_y, this->stride_x, this->padding_type, true, this->padding);
                this->output->set_shape(this->output_shape);
                this->output->set_exponent(this->output_exponent);
                this->output->free_element();
                if (this->padding_type != PADDING_NOT_SET)
                {
                    this->padding = nn::get_pad_size(this->output_shape, input.shape, this->filter->shape_with_dilation, this->stride_y, this->stride_x, this->padding_type);
                }

                if (print_shape)
                {
                    std::cout << this->name << " | ";
                    this->output->print_shape();
                }
            }

            /**
             * @brief Get the output
             * 
             * @return Tensor<feature_t>& Conv2D result
             */
            Tensor<feature_t> &get_output()
            {
                return *this->output;
            }

            /**
             * @brief Call Conv2D operation
             * 
             * @param input           as an input.
             * @param autoload_enable one of true or false, 
             *                        - true: load input and output from PSRAM to CACHE automatically
             *                        - false: do not
             * @param assign_core     not effective yet
             * @return Conv2D result
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
                nn::conv2d(*this->output, input, this->padding, *(this->filter), this->stride_y, this->stride_x, this->bias, this->activation, assign_core);
                DL_LOG_LAYER_LATENCY_END(this->name, "conv2d");
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
