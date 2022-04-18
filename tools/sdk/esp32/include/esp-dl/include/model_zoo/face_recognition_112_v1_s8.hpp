#pragma once

#include "dl_variable.hpp"
#include "face_recognition_tool.hpp"
#include "face_recognizer.hpp"
#include <vector>

using namespace dl;

/**
 * @brief face recognition model v1
 * input size: 112 x 112 x 3
 * quantization mode: S8
 * 
 */
class FaceRecognition112V1S8 : public FaceRecognizer<int8_t>
{       
    public:
        /**
         * @brief Construct a new Face_Recognition_112_V1_S8 object
         * 
         */
        FaceRecognition112V1S8();

        /**
         * @brief Destroy the Face Recognition_112_V1_S8 object
         * 
         */
        ~FaceRecognition112V1S8();
};