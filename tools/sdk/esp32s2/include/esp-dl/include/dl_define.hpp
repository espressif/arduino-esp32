#pragma once

#include <climits>
#include "sdkconfig.h"

#define DL_LOG_LATENCY_UNIT 0  /*<! - 1: cycle */
                               /*<! - 0: us */
#define DL_LOG_NN_LATENCY 0    /*<! - 1: print the latency of each parts of nn */
                               /*<! - 0: mute */
#define DL_LOG_LAYER_LATENCY 0 /*<! - 1: print the latency of each parts of layer */
                               /*<! - 0: mute */

#if CONFIG_SPIRAM_SUPPORT || CONFIG_ESP32_SPIRAM_SUPPORT || CONFIG_ESP32S2_SPIRAM_SUPPORT || CONFIG_ESP32S3_SPIRAM_SUPPORT
#define DL_SPIRAM_SUPPORT 1
#else
#define DL_SPIRAM_SUPPORT 0
#endif

#if CONFIG_IDF_TARGET_ESP32
#define CONFIG_DEFAULT_ASSIGN_CORE \
    {                              \
    } // TODO: 多核 task 完成时，改成默认 0,1
#elif CONFIG_IDF_TARGET_ESP32S2
#define CONFIG_DEFAULT_ASSIGN_CORE \
    {                              \
    }
#elif CONFIG_IDF_TARGET_ESP32S3
#define CONFIG_DEFAULT_ASSIGN_CORE \
    {                              \
    } // TODO: 多核 task 完成时，改成默认 0,1
#elif CONFIG_IDF_TARGET_ESP32C3
#define CONFIG_DEFAULT_ASSIGN_CORE \
    {                              \
    }
#else
#define CONFIG_DEFAULT_ASSIGN_CORE \
    {                              \
    }
#endif

#define DL_Q16_MIN (-32768)
#define DL_Q16_MAX (32767)
#define DL_Q8_MIN (-128)
#define DL_Q8_MAX (127)

#ifndef DL_MAX
#define DL_MAX(x, y) (((x) < (y)) ? (y) : (x))
#endif

#ifndef DL_MIN
#define DL_MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#ifndef DL_CLIP
#define DL_CLIP(x, low, high) ((x) < (low)) ? (low) : (((x) > (high)) ? (high) : (x))
#endif

#ifndef DL_ABS
#define DL_ABS(x) ((x) < 0 ? (-(x)) : (x))
#endif

#ifndef DL_RIGHT_SHIFT
#define DL_RIGHT_SHIFT(x, shift) ((shift) > 0) ? ((x) >> (shift)) : ((x) << -(shift))
#endif

#ifndef DL_LEFT_SHIFT
#define DL_LEFT_SHIFT(x, shift) ((shift) > 0) ? ((x) << (shift)) : ((x) >> -(shift))
#endif

namespace dl
{
    typedef enum
    {
        Linear,    /*<! Linear >*/
        ReLU,      /*<! ReLU >*/
        LeakyReLU, /*<! LeakyReLU >*/
        PReLU,     /*<! PReLU >*/
        // TODO: Sigmoid,   /*<! Sigmoid >*/
        // TODO: Softmax,    /*<! Softmax*/
        // TODO: TanH,
        // TODO: ReLU6
    } activation_type_t;

    typedef enum
    {
        PADDING_NOT_SET,
        PADDING_VALID,      /*<! no padding >*/
        PADDING_SAME_BEGIN,  /*<! SAME in MXNET style >*/
        PADDING_SAME_END,   /*<! SAME in TensorFlow style >*/
    } padding_type_t;
    
    typedef enum
    {
        PADDING_EMPTY,
        PADDING_CONSTANT,
        PADDING_EDGE, 
        PADDING_REFLECT,
        PADDING_SYMMETRIC,
    } padding_mode_t;
} // namespace dl
