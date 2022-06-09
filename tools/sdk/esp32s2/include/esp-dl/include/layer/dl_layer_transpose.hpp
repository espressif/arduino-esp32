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
         * @brief 
         * 
         * @tparam feature_t 
         */
        template <typename feature_t>
        class Transpose : public Layer
        {
        private:
            int output_exponent;           /*<! exponent of output >*/
            Tensor<feature_t> *output;     /*<! output ptr of Transpose >*/
            bool inplace;                  /*<! true: the output will store to input0
                                                false: the output will store to a separate memory >*/
            std::vector<int> perm;         /*<! the new arangement of the dims. if perm == {}, the dims arangement will be reversed. >*/
            std::vector<int> output_shape; /*<! output shape of Transpose >*/
        public:
            /**
             * @brief Construct a new Transpose object
             * 
             * @param perm          the new arangement of the dims. if perm == {}, the dims arangement will be reversed.
             * @param name          name of Transpose layer
             * @param inplace       true: the output will store to input
             *                      false: the output will store to a separate memory
             */
            Transpose(std::vector<int> perm = {}, const char *name = "Transpose", bool inplace = false) : Layer(name),
                                                                                                          output(NULL),
                                                                                                          inplace(inplace),
                                                                                                          perm(perm),
                                                                                                          output_shape({})
            {
            }

            /**
             * @brief Destroy the Transpose object
             * 
             */
            ~Transpose()
            {
                if ((!this->inplace) && (this->output != NULL))
                {
                    delete this->output;
                }
            }

            /**
            * @brief Update output shape and exponent
            * 
            * @param input as an input
            * @param print_shape  whether to print the output shape.
            */
            void build(Tensor<feature_t> &input, bool print_shape = false)
            {
                this->output_exponent = input.exponent;
                this->output_shape = input.shape;
                int dims = this->output_shape.size();
                if (this->perm.size() == 0)
                {
                    for (int i = dims - 1; i >= 0; i--)
                    {
                        this->perm.push_back(i);
                    }
                }
                for (int i = 0; i < dims; ++i)
                {
                    if (this->perm[i] < 0)
                        this->perm[i] = dims + this->perm[i];
                    this->output_shape[i] = input.shape[this->perm[i]];
                }

                if (!this->inplace)
                {
                    if (this->output == NULL)
                    {
                        this->output = new Tensor<feature_t>;
                    }
                    this->output->set_exponent(this->output_exponent);
                    this->output->set_shape(this->output_shape);
                    this->output->free_element();
                }
                else
                {
                    this->output = &input;
                    this->output->set_shape(this->output_shape);
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
             * @return Tensor<feature_t>& Transpose result
             */
            Tensor<feature_t> &get_output()
            {
                return *this->output;
            }

            /**
             * @brief Call Transpose operation.
             * 
             * @param input as an input.
             * @return Tensor<feature_t>& Transpose result.
             */
            Tensor<feature_t> &call(Tensor<feature_t> &input)
            {
                DL_LOG_LAYER_LATENCY_INIT();

                if (!this->inplace)
                {
                    DL_LOG_LAYER_LATENCY_START();
                    this->output->set_exponent(input.exponent);
                    this->output->transpose(input, this->perm);
                    DL_LOG_LAYER_LATENCY_END(this->name, "transpose");
                }
                else
                {
                    DL_LOG_LAYER_LATENCY_START();
                    this->output->transpose(this->perm);
                    DL_LOG_LAYER_LATENCY_END(this->name, "transpose");
                }
                return *this->output;
            }
        };
    } // namespace layer
} // namespace dl
