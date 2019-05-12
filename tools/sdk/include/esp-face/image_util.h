/*
  * ESPRESSIF MIT License
  *
  * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
  *
  * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
  * it is free of charge, to any person obtaining a copy of this software and associated
  * documentation files (the "Software"), to deal in the Software without restriction, including
  * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
  * to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all copies or
  * substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  *
  */
#pragma once
#ifdef __cplusplus
extern "C"
{
#endif
#include <stdint.h>
#include <math.h>
#include "mtmn.h"

#define MAX_VALID_COUNT_PER_IMAGE (30)

#define DL_IMAGE_MIN(A, B) ((A) < (B) ? (A) : (B))
#define DL_IMAGE_MAX(A, B) ((A) < (B) ? (B) : (A))

#define IMAGE_WIDTH 320
#define IMAGE_HEIGHT 240

#define RGB565_MASK_RED 0xF800
#define RGB565_MASK_GREEN 0x07E0
#define RGB565_MASK_BLUE 0x001F

    typedef enum
    {
        BINARY,
    } en_threshold_mode;
    typedef struct
    {
        fptp_t landmark_p[10];
    } landmark_t;

    typedef struct
    {
        fptp_t box_p[4];
    } box_t;

    typedef struct tag_box_list
    {
        fptp_t *score;
        box_t *box;
        landmark_t *landmark;
        int len;
    } box_array_t;

    typedef struct tag_image_box
    {
        struct tag_image_box *next;
        fptp_t score;
        box_t box;
        box_t offset;
        landmark_t landmark;
    } image_box_t;

    typedef struct tag_image_list
    {
        image_box_t *head;
        image_box_t *origin_head;
        int len;
    } image_list_t;

    static inline void image_get_width_and_height(box_t *box, float *w, float *h)
    {
        *w = box->box_p[2] - box->box_p[0] + 1;
        *h = box->box_p[3] - box->box_p[1] + 1;
    }

    static inline void image_get_area(box_t *box, float *area)
    {
        float w, h;
        image_get_width_and_height(box, &w, &h);
        *area = w * h;
    }

    static inline void image_calibrate_by_offset(image_list_t *image_list)
    {
        for (image_box_t *head = image_list->head; head; head = head->next)
        {
            float w, h;
            image_get_width_and_height(&(head->box), &w, &h);
            head->box.box_p[0] = DL_IMAGE_MAX(0, head->box.box_p[0] + head->offset.box_p[0] * w);
            head->box.box_p[1] = DL_IMAGE_MAX(0, head->box.box_p[1] + head->offset.box_p[1] * w);
            head->box.box_p[2] += head->offset.box_p[2] * w;
            if (head->box.box_p[2] > IMAGE_WIDTH)
            {
                head->box.box_p[2] = IMAGE_WIDTH - 1;
                head->box.box_p[0] = IMAGE_WIDTH - w;
            }
            head->box.box_p[3] += head->offset.box_p[3] * h;
            if (head->box.box_p[3] > IMAGE_HEIGHT)
            {
                head->box.box_p[3] = IMAGE_HEIGHT - 1;
                head->box.box_p[1] = IMAGE_HEIGHT - h;
            }
        }
    }

    static inline void image_landmark_calibrate(image_list_t *image_list)
    {
        for (image_box_t *head = image_list->head; head; head = head->next)
        {
            float w, h;
            image_get_width_and_height(&(head->box), &w, &h);
            head->landmark.landmark_p[0] = head->box.box_p[0] + head->landmark.landmark_p[0] * w;
            head->landmark.landmark_p[1] = head->box.box_p[1] + head->landmark.landmark_p[1] * h;

            head->landmark.landmark_p[2] = head->box.box_p[0] + head->landmark.landmark_p[2] * w;
            head->landmark.landmark_p[3] = head->box.box_p[1] + head->landmark.landmark_p[3] * h;

            head->landmark.landmark_p[4] = head->box.box_p[0] + head->landmark.landmark_p[4] * w;
            head->landmark.landmark_p[5] = head->box.box_p[1] + head->landmark.landmark_p[5] * h;

            head->landmark.landmark_p[6] = head->box.box_p[0] + head->landmark.landmark_p[6] * w;
            head->landmark.landmark_p[7] = head->box.box_p[1] + head->landmark.landmark_p[7] * h;

            head->landmark.landmark_p[8] = head->box.box_p[0] + head->landmark.landmark_p[8] * w;
            head->landmark.landmark_p[9] = head->box.box_p[1] + head->landmark.landmark_p[9] * h;
        }
    }

    static inline void image_rect2sqr(box_array_t *boxes, int width, int height)
    {
        for (int i = 0; i < boxes->len; i++)
        {
            box_t *box = &(boxes->box[i]);

            int x1 = round(box->box_p[0]);
            int y1 = round(box->box_p[1]);
            int x2 = round(box->box_p[2]);
            int y2 = round(box->box_p[3]);

            int w = x2 - x1 + 1;
            int h = y2 - y1 + 1;
            int l = DL_IMAGE_MAX(w, h);

            box->box_p[0] = round(DL_IMAGE_MAX(0, x1) + 0.5 * (w - l));
            box->box_p[1] = round(DL_IMAGE_MAX(0, y1) + 0.5 * (h - l));

            box->box_p[2] = box->box_p[0] + l - 1;
            if (box->box_p[2] > width)
            {
                box->box_p[2] = width - 1;
                box->box_p[0] = width - l;
            }
            box->box_p[3] = box->box_p[1] + l - 1;
            if (box->box_p[3] > height)
            {
                box->box_p[3] = height - 1;
                box->box_p[1] = height - l;
            }
        }
    }

    static inline void rgb565_to_888(uint16_t in, uint8_t *dst)
    {                                           /*{{{*/
        dst[0] = (in & RGB565_MASK_BLUE) << 3;  // blue
        dst[1] = (in & RGB565_MASK_GREEN) >> 3; // green
        dst[2] = (in & RGB565_MASK_RED) >> 8;   // red
    }                                           /*}}}*/

    static inline void rgb888_to_565(uint16_t *in, uint8_t r, uint8_t g, uint8_t b)
    { /*{{{*/
        uint16_t rgb565 = 0;
        rgb565 = ((r >> 3) << 11);
        rgb565 |= ((g >> 2) << 5);
        rgb565 |= (b >> 3);
        *in = rgb565;
    } /*}}}*/

    /**
     * @brief 
     * 
     * @param score 
     * @param offset 
     * @param width 
     * @param height 
     * @param p_net_size
     * @param score_threshold 
     * @param scale 
     * @return image_list_t* 
     */
    image_list_t *image_get_valid_boxes(fptp_t *score,
                                        fptp_t *offset,
                                        int width,
                                        int height,
                                        int p_net_size,
                                        fptp_t score_threshold,
                                        fptp_t scale);
    /**
     * @brief 
     * 
     * @param image_sorted_list 
     * @param insert_list 
     */
    void image_sort_insert_by_score(image_list_t *image_sorted_list, const image_list_t *insert_list);

    /**
     * @brief 
     * 
     * @param image_list 
     * @param nms_threshold 
     * @param same_area 
     */
    void image_nms_process(image_list_t *image_list, fptp_t nms_threshold, int same_area);

    /**
     * @brief 
     * 
     * @param dimage 
     * @param dw 
     * @param dh 
     * @param dc 
     * @param simage 
     * @param sw 
     * @param sc 
     */
    void image_zoom_in_twice(uint8_t *dimage,
                             int dw,
                             int dh,
                             int dc,
                             uint8_t *simage,
                             int sw,
                             int sc);

    /**
     * @brief 
     * 
     * @param dst_image 
     * @param src_image 
     * @param dst_w 
     * @param dst_h 
     * @param dst_c 
     * @param src_w 
     * @param src_h 
     */
    void image_resize_linear(uint8_t *dst_image, uint8_t *src_image, int dst_w, int dst_h, int dst_c, int src_w, int src_h);

    /**
     * @brief 
     * 
     * @param corp_image 
     * @param src_image 
     * @param rotate_angle 
     * @param ratio 
     * @param center 
     */
    void image_cropper(uint8_t *corp_image, uint8_t *src_image, int dst_w, int dst_h, int dst_c, int src_w, int src_h, float rotate_angle, float ratio, float *center);

    /**
     * @brief 
     * 
     * @param m 
     * @param bmp 
     * @param count 
     */
    void transform_input_image(uint8_t *m, uint16_t *bmp, int count);

    /**
     * @brief 
     * 
     * @param bmp 
     * @param m 
     * @param count 
     */
    void transform_output_image(uint16_t *bmp, uint8_t *m, int count);

    /**
     * @brief 
     * 
     * @param buf 
     * @param boxes 
     * @param width 
     */
    void draw_rectangle_rgb565(uint16_t *buf, box_array_t *boxes, int width);

    /**
     * @brief 
     * 
     * @param buf 
     * @param boxes 
     * @param width 
     */
    void draw_rectangle_rgb888(uint8_t *buf, box_array_t *boxes, int width);
    void image_abs_diff(uint8_t *dst, uint8_t *src1, uint8_t *src2, int count);
    void image_threshold(uint8_t *dst, uint8_t *src, int threshold, int value, int count, en_threshold_mode mode);
    void image_erode(uint8_t *dst, uint8_t *src, int src_w, int src_h, int src_c);
#ifdef __cplusplus
}
#endif
