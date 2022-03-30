#pragma once

#include <vector>
#include <list>
#include "dl_detect_define.hpp"

class HumanFaceDetectMNP01
{
private:
    void *model;

public:
    /**
     * @brief Construct a new Human Face Detect MNP01 object.
     * 
     * @param score_threshold predicted boxes with score lower than the threshold will be filtered out
     * @param nms_threshold   predicted boxes with IoU higher than the threshold will be filtered out
     * @param top_k           first k highest score boxes will be remained
     */
    HumanFaceDetectMNP01(const float score_threshold, const float nms_threshold, const int top_k);

    /**
     * @brief Destroy the Human Face Detect MNP01 object.
     * 
     */
    ~HumanFaceDetectMNP01();

    /**
     * @brief Inference.
     * 
     * @tparam T supports uint16_t and uint8_t,
     *         - uint16_t: input image is RGB565
     *         - uint8_t: input image is RGB888
     * @param input_element pointer of input image
     * @param input_shape   shape of input image
     * @param candidates    candidate boxes on input image
     * @return detection result
     */
    template <typename T>
    std::list<dl::detect::result_t> &infer(T *input_element, std::vector<int> input_shape, std::list<dl::detect::result_t> &candidates);
};
