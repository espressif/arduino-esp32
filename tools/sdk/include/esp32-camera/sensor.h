/*
 * This file is part of the OpenMV project.
 * Copyright (c) 2013/2014 Ibrahim Abdelkader <i.abdalkader@gmail.com>
 * This work is licensed under the MIT license, see the file LICENSE for details.
 *
 * Sensor abstraction layer.
 *
 */
#ifndef __SENSOR_H__
#define __SENSOR_H__
#include <stdint.h>

#define OV9650_PID     (0x96)
#define OV2640_PID     (0x26)
#define OV7725_PID     (0x77)
#define OV3660_PID     (0x36)

typedef enum {
    PIXFORMAT_RGB565,    // 2BPP/RGB565
    PIXFORMAT_YUV422,    // 2BPP/YUV422
    PIXFORMAT_GRAYSCALE, // 1BPP/GRAYSCALE
    PIXFORMAT_JPEG,      // JPEG/COMPRESSED
    PIXFORMAT_RGB888,    // 3BPP/RGB888
    PIXFORMAT_RAW,       // RAW
    PIXFORMAT_RGB444,    // 3BP2P/RGB444
    PIXFORMAT_RGB555,    // 3BP2P/RGB555
} pixformat_t;

typedef enum {
    FRAMESIZE_96x96,    // 96x96
    FRAMESIZE_QQVGA,    // 160x120
    FRAMESIZE_QQVGA2,   // 128x160
    FRAMESIZE_QCIF,     // 176x144
    FRAMESIZE_HQVGA,    // 240x176
    FRAMESIZE_240x240,  // 240x240
    FRAMESIZE_QVGA,     // 320x240
    FRAMESIZE_CIF,      // 400x296
    FRAMESIZE_VGA,      // 640x480
    FRAMESIZE_SVGA,     // 800x600
    FRAMESIZE_XGA,      // 1024x768
    FRAMESIZE_SXGA,     // 1280x1024
    FRAMESIZE_UXGA,     // 1600x1200
    FRAMESIZE_QXGA,     // 2048*1536
    FRAMESIZE_INVALID
} framesize_t;

typedef enum {
    GAINCEILING_2X,
    GAINCEILING_4X,
    GAINCEILING_8X,
    GAINCEILING_16X,
    GAINCEILING_32X,
    GAINCEILING_64X,
    GAINCEILING_128X,
} gainceiling_t;

typedef struct {
    uint8_t MIDH;
    uint8_t MIDL;
    uint8_t PID;
    uint8_t VER;
} sensor_id_t;

typedef struct {
    framesize_t framesize;//0 - 10
    uint8_t quality;//0 - 63
    int8_t brightness;//-2 - 2
    int8_t contrast;//-2 - 2
    int8_t saturation;//-2 - 2
    int8_t sharpness;//-2 - 2
    uint8_t denoise;
    uint8_t special_effect;//0 - 6
    uint8_t wb_mode;//0 - 4
    uint8_t awb;
    uint8_t awb_gain;
    uint8_t aec;
    uint8_t aec2;
    int8_t ae_level;//-2 - 2
    uint16_t aec_value;//0 - 1200
    uint8_t agc;
    uint8_t agc_gain;//0 - 30
    uint8_t gainceiling;//0 - 6
    uint8_t bpc;
    uint8_t wpc;
    uint8_t raw_gma;
    uint8_t lenc;
    uint8_t hmirror;
    uint8_t vflip;
    uint8_t dcw;
    uint8_t colorbar;
} camera_status_t;

typedef struct _sensor sensor_t;
typedef struct _sensor {
    sensor_id_t id;             // Sensor ID.
    uint8_t  slv_addr;          // Sensor I2C slave address.
    pixformat_t pixformat;
    camera_status_t status;
    int xclk_freq_hz;

    // Sensor function pointers
    int  (*init_status)         (sensor_t *sensor);
    int  (*reset)               (sensor_t *sensor);
    int  (*set_pixformat)       (sensor_t *sensor, pixformat_t pixformat);
    int  (*set_framesize)       (sensor_t *sensor, framesize_t framesize);
    int  (*set_contrast)        (sensor_t *sensor, int level);
    int  (*set_brightness)      (sensor_t *sensor, int level);
    int  (*set_saturation)      (sensor_t *sensor, int level);
    int  (*set_sharpness)       (sensor_t *sensor, int level);
    int  (*set_denoise)         (sensor_t *sensor, int level);
    int  (*set_gainceiling)     (sensor_t *sensor, gainceiling_t gainceiling);
    int  (*set_quality)         (sensor_t *sensor, int quality);
    int  (*set_colorbar)        (sensor_t *sensor, int enable);
    int  (*set_whitebal)        (sensor_t *sensor, int enable);
    int  (*set_gain_ctrl)       (sensor_t *sensor, int enable);
    int  (*set_exposure_ctrl)   (sensor_t *sensor, int enable);
    int  (*set_hmirror)         (sensor_t *sensor, int enable);
    int  (*set_vflip)           (sensor_t *sensor, int enable);

    int  (*set_aec2)            (sensor_t *sensor, int enable);
    int  (*set_awb_gain)        (sensor_t *sensor, int enable);
    int  (*set_agc_gain)        (sensor_t *sensor, int gain);
    int  (*set_aec_value)       (sensor_t *sensor, int gain);

    int  (*set_special_effect)  (sensor_t *sensor, int effect);
    int  (*set_wb_mode)         (sensor_t *sensor, int mode);
    int  (*set_ae_level)        (sensor_t *sensor, int level);

    int  (*set_dcw)             (sensor_t *sensor, int enable);
    int  (*set_bpc)             (sensor_t *sensor, int enable);
    int  (*set_wpc)             (sensor_t *sensor, int enable);

    int  (*set_raw_gma)         (sensor_t *sensor, int enable);
    int  (*set_lenc)            (sensor_t *sensor, int enable);
} sensor_t;

// Resolution table (in camera.c)
extern const int resolution[][2];

#endif /* __SENSOR_H__ */
