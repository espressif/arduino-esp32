#pragma once

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
         * @brief TanH(input)
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
        class TanH : public Layer
        {
        private:
            const int output_exponent;     /*<! exponent of output >*/
            const float rescale;           /*<! rescale output to quantization >*/
            Tensor<O> *output;             /*<! output ptr of TanH>*/
            std::vector<int> output_shape; /*<! output shape of TanH >*/
            int size;                      /*<! size of input >*/
            float scale;                   /*<! scale input to floating-point >*/

        public:
            /**
             * @brief Construct a new TanH object
             *
             * @param output_exponent      exponent of output
             * @param name                 name of TanH
             */
            TanH(const int output_exponent, const char *name = "TanH") : Layer(name),
                                                                         output_exponent(output_exponent),
                                                                         rescale(DL_RESCALE(output_exponent)),
                                                                         output(nullptr) {}

            /**
             * @brief Destroy the TanH object
             *
             */
            ~TanH()
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
                this->scale = DL_SCALE(input.exponent + 1);

                this->size = input.get_size();

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
             * @return Tensor<I>& TanH result
             */
            Tensor<O> &get_output()
            {
                return *this->output;
            }

            /**
             * @brief Call TanH operation.
             *
             * @param input       as an input
             * @param assign_core not effective yet
             * @return TanH result
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
                I *input_ptr = input.element;
                O *output_ptr = this->output->element;
                for (size_t i = 0; i < this->size; i++)
                {
                    // float temp = dl::math::exp_fast((float)input_ptr[i] * this->scale);
                    float temp = exp((float)input_ptr[i] * this->scale);
                    temp = (temp - 1.0f) / (temp + 1.0f);

                    if constexpr (type == QIQO)
                        dl::tool::truncate(output_ptr[i], temp * this->rescale);
                    else if constexpr (type == QIFO)
                        output_ptr[i] = temp;
                }

                if (this->output->shape != this->output_shape)
                    this->output->set_shape(this->output_shape);
                this->output->set_exponent(this->output_exponent);
                DL_LOG_LAYER_LATENCY_END(this->name, "tanh");

                return *this->output;
            }
        };
    } // namespace layer
} // namespace dl
