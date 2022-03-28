#pragma once

#include "dl_constant.hpp"
#include "dl_variable.hpp"
#include "dl_nn_add2d.hpp"
#include "dl_layer_base.hpp"

namespace dl
{
    namespace layer
    {
        /**
         * @brief Activation(Add2D(input0, input1)).
         * NOTE: addition is element-wise, i.e., output[i,j,k] = input0[i,j,k] + input1[i,j,k]
         * 
         * @tparam feature_t supports int16_t and int8_t,
         *         - int16_t: stands for operation in int16_t quantize
         *         - int8_t: stands for operation in int8_t quantize
         */
        template <typename feature_t>
        class Add2D : public Layer
        {
        private:
            const Activation<feature_t> *activation; /*<! activation of add2d, if you don't specify anything, no activation is applied >*/
            const int output_exponent;               /*<! exponent of output >*/
            Tensor<feature_t> *output;               /*<! output ptr of add2d >*/
            bool inplace;                            /*<! true: the output will store to input0
                                                          false: the output will store to a separate memory >*/
            std::vector<int> output_shape;           /*<! output shape of add2d >*/

        public:
            /**
             * @brief Construct a new Add2D object.
             * 
             * @param output_exponent exponent of output
             * @param activation      activation of add2d, if you don't specify anything, no activation is applied
             * @param name            name of add2d
             * @param inplace         true: the output will store to input0
             *                        false: the output will store to a separate memory
             */
            Add2D(const int output_exponent, const Activation<feature_t> *activation = NULL, const char *name = "Add2D", bool inplace = false) : Layer(name),
                                                                                                                                                 activation(activation),
                                                                                                                                                 output_exponent(output_exponent),
                                                                                                                                                 output(NULL),
                                                                                                                                                 inplace(inplace),
                                                                                                                                                 output_shape({}) {}

            /**
             * @brief Destroy the Add2D object
             */
            ~Add2D()
            {
                if ((!this->inplace) && (this->output != NULL))
                {
                    delete this->output;
                }
            }

            /**
             * @brief Update output shape.
             * NOTE: input0.shape must equal to input1.shape.
             * 
             * @param input0 as one input
             * @param input1 as another input
             * @param print_shape  whether to print the output shape.
             */
            void build(Tensor<feature_t> &input0, Tensor<feature_t> &input1, bool print_shape = false)
            {
                assert(input0.is_same_shape(input1));
                this->output_shape = input0.shape;

                if (!this->inplace)
                {
                    if (this->output == NULL)
                    {
                        this->output = new Tensor<feature_t>;
                    }
                    this->output->set_exponent(this->output_exponent);
                    this->output->set_shape(input0.shape);
                    this->output->free_element();
                }
                else
                {
                    this->output = &input0;
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
            * @return Tensor<feature_t>& Add2D result
            */
            Tensor<feature_t> &get_output()
            {
                return *this->output;
            }

            /**
             * @brief Call Add2D operation.
             * 
             * @param input0      as one input
             * @param input1      as another input
             * @param assign_core not effective yet
             * @return Tensor<feature_t>& added result
             */
            Tensor<feature_t> &call(Tensor<feature_t> &input0, Tensor<feature_t> &input1, const std::vector<int> &assign_core = CONFIG_DEFAULT_ASSIGN_CORE)
            {
                DL_LOG_LAYER_LATENCY_INIT();

                if (!this->inplace)
                {
                    DL_LOG_LAYER_LATENCY_START();
                    if (this->output->shape != this->output_shape)
                    {
                        this->output->set_shape(this->output_shape);
                    }
                    this->output->malloc_element();
                    this->output->set_exponent(this->output_exponent);
                    DL_LOG_LAYER_LATENCY_END(this->name, "apply");

                    DL_LOG_LAYER_LATENCY_START();
                    nn::add2d(*this->output, input0, input1, this->activation, assign_core);
                    DL_LOG_LAYER_LATENCY_END(this->name, "add2d");
                }
                else
                {
                    DL_LOG_LAYER_LATENCY_START();
                    if (this->output->shape != this->output_shape)
                    {
                        this->output->set_shape(this->output_shape);
                    }
                    nn::add2d(*this->output, input0, input1, this->activation, assign_core, this->output_exponent);
                    DL_LOG_LAYER_LATENCY_END(this->name, "add2d");
                }

                return *this->output;
            }
        };
    } // namespace layer
} // namespace dl
