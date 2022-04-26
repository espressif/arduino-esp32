// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef _FB_GFX_H_
#define _FB_GFX_H_

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum {
        FB_RGB888, FB_BGR888, FB_RGB565, FB_BGR565, FB_GRAY
    } fb_format_t;

    typedef struct {
            int width;
            int height;
            int bytes_per_pixel;
            fb_format_t format;
            uint8_t * data;
    } fb_data_t;

    void     fb_gfx_fillRect     (fb_data_t *fb, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
    void     fb_gfx_drawFastHLine(fb_data_t *fb, int32_t x, int32_t y, int32_t w, uint32_t color);
    void     fb_gfx_drawFastVLine(fb_data_t *fb, int32_t x, int32_t y, int32_t h, uint32_t color);
    uint8_t  fb_gfx_putc         (fb_data_t *fb, int32_t x, int32_t y, uint32_t color, unsigned char c);
    uint32_t fb_gfx_print        (fb_data_t *fb, int32_t x, int32_t y, uint32_t color, const char * str);
    uint32_t fb_gfx_printf       (fb_data_t *fb, int32_t x, int32_t y, uint32_t color, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* _FB_GFX_H_ */
