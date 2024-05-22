#ifndef _dsp_types_H_
#define _dsp_types_H_
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

// union to simplify access to the 16 bit data
typedef union sc16_u {
    struct {
        int16_t re;
        int16_t im;
    };
    uint32_t data;
} sc16_t;

typedef union fc32_u {
    struct {
        float re;
        float im;
    };
    uint64_t data;
} fc32_t;

typedef struct image2d_s {
    void *data; // could be int8_t, unt8_t, int16_t, unt16_t, float
    int step_x; // step of elements by X
    int step_y; // step of elements by Y, usually is 1
    int stride_x; // stride width: size of the elements in X axis * by step_x + padding
    int stride_y; // stride height: size of the elements in Y axis * by step_y + padding
    // Point[x,y] = data[width*y*step_y + x*step_x];
    // Full data size = width*height

} image2d_t;

#endif // _dsp_types_H_
