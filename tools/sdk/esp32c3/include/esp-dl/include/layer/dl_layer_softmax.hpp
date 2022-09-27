#pragma once

#include <memory>
#include <math.h>

#include "dl_constant.hpp"
#include "dl_variable.hpp"
#include "dl_math.hpp"
#include "dl_layer_base.hpp"

namespace dl
{
    namespace layer
    {
        /**
         * @brief Softmax(input)
         *
         * @tparam I supports int16_t and int8_t,
         *         - int16_t: stands for intput in int16_t quantize
         *         - int8_t: stands for intput in int8_t quantize
         * @tparam I supports int16_t, int8_t and float
         *         - int16_t: stands for output in int16_t quantize
         *         - int8_t: stands for output in int8_t quantize
         *         - float: stands for output in float
         * @tparam type supports QIQO and QIFO
         *         - QIQO: stands for both input and output in quantize
         *         - QIFO: stands for input in quantize and output in floating
         * @tparam inplace supports true and false,
         *         - true: the output will store to input. However, if the type of input and output is different then will not
         *         - false: the output will store to a separate memory
         */
        template <typename I, typename O = float, int type = QIFO, bool inplace = false>
        class Softmax : public Layer
        {
        private:
            const int output_exponent;     /*<! exponent of output >*/
            const float rescale;           /*<! rescale output to quantization >*/
            Tensor<O> *output;             /*<! output ptr of Softmax>*/
            std::vector<int> output_shape; /*<! output shape of Softmax >*/
            int loop;                      /*<! loop times >*/
            int channel;                   /*<! channel of input and output >*/
            float scale;                   /*<! scale input to floating-point >*/

        public:
            /**
             * @brief Construct a new Softmax object
             *
             * @param output_exponent      exponent of output
             * @param name                 name of Softmax
             * @param inplace              true: the output will store to input
             *                             false: the output will store to a separate memory
             */
            Softmax(const int output_exponent, const char *name = "Softmax") : Layer(name),
                                                                               output_exponent(output_exponent),
                                                                               rescale(DL_RESCALE(output_exponent)),
                                                                               output(nullptr) {}

            /**
             * @brief Destroy the Softmax object
             *
             */
            ~Softmax()
            {
                if constexpr (inplace == false || type == QIFO || sizeof(I) != sizeof(O))
                    if (this->output != nullptr)
                        delete this->output;
            }

            /**
             * @brief Update output shape and exponent
             *
             * @param input       as an input
             * @param print_shape  whether to print the output shape.
             */
            void build(Tensor<I> &input, bool print_shape = false)
            {
                this->scale = DL_SCALE(input.exponent);

                this->channel = input.shape[2];
                this->loop = input.get_size() / this->channel;

                this->output_shape = input.shape;

                if constexpr (inplace && type == QIQO && sizeof(I) == sizeof(O))
                {
                    this->output = &input;
                }
                else
                {
                    if (this->output == nullptr)
                        this->output = new Tensor<O>;
                    this->output->set_shape(this->output_shape);
                    this->output->free_element();
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
             * @return Tensor<I>& Softmax result
             */
            Tensor<O> &get_output()
            {
                return *this->output;
            }

            /**
             * @brief Call Softmax operation.
             *
             * @param input       as an input
             * @param assign_core not effective yet
             * @return Softmax result
             */
            Tensor<O> &call(Tensor<I> &input, const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE)
            {
                DL_LOG_LAYER_LATENCY_INIT();

                if constexpr (inplace == false || type == QIFO || sizeof(I) != sizeof(O))
                {
                    DL_LOG_LAYER_LATENCY_START();
                    this->output->malloc_element();
                    DL_LOG_LAYER_LATENCY_END(this->name, "apply");
                }

                DL_LOG_LAYER_LATENCY_START();
                std::unique_ptr<float[]> buf(new float[this->channel]);
                I *input_ptr = input.element;
                O *output_ptr = this->output->element;
                for (size_t i = 0; i < this->loop; i++)
                {
                    I max_input = input_ptr[0];
                    for (size_t j = 1; j < this->channel; j++)
                        max_input = DL_MAX(max_input, input_ptr[j]);

                    float summary = 0.0;
                    for (size_t j = 0; j < this->channel; j++)
                    {
                        buf[j] = dl::math::exp_fast(((float)input_ptr[j] - max_input) * this->scale);
                        // buf[j] = exp(((float)input_ptr[j] - max_input) * this->scale);
                        summary += buf[j];
                    }

                    if constexpr (type == QIQO)
                    {
                        summary = this->rescale / summary;
                        for (size_t j = 0; j < this->channel; j++)
                            dl::tool::truncate(output_ptr[j], buf[j] * summary);
                    }
                    else if constexpr (type == QIFO)
                    {
                        summary = 1.0 / summary;
                        for (size_t j = 0; j < this->channel; j++)
                            output_ptr[j] = buf[j] * summary;
                    }

                    input_ptr += this->channel;
                    output_ptr += this->channel;
                }

                if (this->output->shape != this->output_shape)
                    this->output->set_shape(this->output_shape);
                this->output->set_exponent(this->output_exponent);
                DL_LOG_LAYER_LATENCY_END(this->name, "softmax");

                return *this->output;
            }
        };
    } // namespace layer
} // namespace dl
