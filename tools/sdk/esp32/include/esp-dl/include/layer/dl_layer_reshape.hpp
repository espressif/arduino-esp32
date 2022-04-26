#pragma once

#include "dl_constant.hpp"
#include "dl_variable.hpp"
#include "dl_tool.hpp"
#include "dl_layer_base.hpp"

namespace dl
{
    namespace layer
    {
        /**
         * @brief  Reshape(input)
         * 
         * @tparam feature_t supports int16_t and int8_t,
         *         - int16_t: stands for operation in int16_t quantize
         *         - int8_t: stands for operation in int8_t quantize
         */
        template <typename feature_t>
        class Reshape : public Layer
        {
        private:
            int output_exponent;           /*<! exponent of output >*/
            Tensor<feature_t> *output;     /*<! output ptr of Reshape >*/
            bool inplace;                  /*<! true: the output will store to input0
                                                false: the output will store to a separate memory >*/
            std::vector<int> output_shape; /*<! output shape of Reshape >*/
        public:
            /**
             * @brief Construct a new Reshape object
             * 
             * @param shape      the target shape
             * @param name       name of Reshape layer
             * @param inplace    true: the output will store to input0
             *                   false: the output will store to a separate memory
             */
            Reshape(std::vector<int> shape, const char *name = "Reshape", bool inplace = false) : Layer(name),
                                                                                                  output(NULL),
                                                                                                  inplace(inplace),
                                                                                                  output_shape(shape)
            {
            }

            /**
             * @brief Destroy the Reshape object
             * 
             */
            ~Reshape()
            {
                if ((!this->inplace) && (this->output != NULL))
                {
                    delete this->output;
                }
            }

            /**
            * @brief             Update output shape and exponent
            * 
            * @param input        as an input
            * @param print_shape  whether to print the output shape.
            */
            void build(Tensor<feature_t> &input, bool print_shape = false)
            {
                this->output_exponent = input.exponent;
                if (!this->inplace)
                {
                    if (this->output == NULL)
                    {
                        this->output = new Tensor<feature_t>;
                    }
                    this->output->set_exponent(this->output_exponent);
                    this->output->set_shape(input.shape);
                    this->output->reshape(this->output_shape);
                    this->output->free_element();
                }
                else
                {
                    this->output = &input;
                    this->output->reshape(this->output_shape);
                }
                this->output_shape = this->output->shape;

                if (print_shape)
                {
                    std::cout << this->name << " | ";
                    this->output->print_shape();
                }
            }

            /**
             * @brief Get the output
             * 
             * @return Tensor<feature_t>& Reshape result
             */
            Tensor<feature_t> &get_output()
            {
                return *this->output;
            }

            /**
             * @brief Call Reshape operation.
             * 
             * @param input  as an input
             * @return Tensor<feature_t>& Reshape result
             */
            Tensor<feature_t> &call(Tensor<feature_t> &input)
            {
                DL_LOG_LAYER_LATENCY_INIT();

                if (!this->inplace)
                {
                    DL_LOG_LAYER_LATENCY_START();
                    this->output->set_exponent(input.exponent);
                    this->output->reshape(this->output_shape);
                    this->output->copy_element(input, true);
                    DL_LOG_LAYER_LATENCY_END(this->name, "reshape");
                }
                else
                {
                    DL_LOG_LAYER_LATENCY_START();
                    this->output->reshape(this->output_shape);
                    DL_LOG_LAYER_LATENCY_END(this->name, "reshape");
                }
                return *this->output;
            }
        };
    } // namespace layer
} // namespace dl
