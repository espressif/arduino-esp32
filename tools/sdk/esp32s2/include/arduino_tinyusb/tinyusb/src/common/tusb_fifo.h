/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2020 Reinhard Panhuber - rework to unmasked pointers
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef _TUSB_FIFO_H_
#define _TUSB_FIFO_H_

#ifdef __cplusplus
extern "C" {
#endif

// Due to the use of unmasked pointers, this FIFO does not suffer from losing
// one item slice. Furthermore, write and read operations are completely
// decoupled as write and read functions do not modify a common state. Henceforth,
// writing or reading from the FIFO within an ISR is safe as long as no other
// process (thread or ISR) interferes.
// Also, this FIFO is ready to be used in combination with a DMA as the write and
// read pointers can be updated from within a DMA ISR. Overflows are detectable
// within a certain number (see tu_fifo_overflow()).

#include "common/tusb_common.h"
#include "osal/osal.h"

#define tu_fifo_mutex_t  osal_mutex_t

// mutex is only needed for RTOS
// for OS None, we don't get preempted
#define CFG_FIFO_MUTEX      OSAL_MUTEX_REQUIRED

typedef struct
{
  uint8_t* buffer               ; ///< buffer pointer
  uint16_t depth                ; ///< max items
  uint16_t item_size            ; ///< size of each item
  bool overwritable             ;

  uint16_t non_used_index_space ; ///< required for non-power-of-two buffer length
  uint16_t max_pointer_idx      ; ///< maximum absolute pointer index

  volatile uint16_t wr_idx      ; ///< write pointer
  volatile uint16_t rd_idx      ; ///< read pointer

#if OSAL_MUTEX_REQUIRED
  tu_fifo_mutex_t mutex_wr;
  tu_fifo_mutex_t mutex_rd;
#endif

} tu_fifo_t;

typedef struct
{
  uint16_t len_lin  ; ///< linear length in item size
  uint16_t len_wrap ; ///< wrapped length in item size
  void * ptr_lin    ; ///< linear part start pointer
  void * ptr_wrap   ; ///< wrapped part start pointer
} tu_fifo_buffer_info_t;

#define TU_FIFO_INIT(_buffer, _depth, _type, _overwritable) \
{                                                           \
  .buffer               = _buffer,                          \
  .depth                = _depth,                           \
  .item_size            = sizeof(_type),                    \
  .overwritable         = _overwritable,                    \
  .non_used_index_space = UINT16_MAX - (2*(_depth)-1),      \
  .max_pointer_idx      = 2*(_depth)-1,                     \
}

#define TU_FIFO_DEF(_name, _depth, _type, _overwritable)                      \
    uint8_t _name##_buf[_depth*sizeof(_type)];                                \
    tu_fifo_t _name = TU_FIFO_INIT(_name##_buf, _depth, _type, _overwritable)


bool tu_fifo_set_overwritable(tu_fifo_t *f, bool overwritable);
bool tu_fifo_clear(tu_fifo_t *f);
bool tu_fifo_config(tu_fifo_t *f, void* buffer, uint16_t depth, uint16_t item_size, bool overwritable);

#if OSAL_MUTEX_REQUIRED
TU_ATTR_ALWAYS_INLINE static inline
void tu_fifo_config_mutex(tu_fifo_t *f, tu_fifo_mutex_t write_mutex_hdl, tu_fifo_mutex_t read_mutex_hdl)
{
  f->mutex_wr = write_mutex_hdl;
  f->mutex_rd = read_mutex_hdl;
}

#else

#define tu_fifo_config_mutex(_f, _wr_mutex, _rd_mutex)

#endif

bool     tu_fifo_write                  (tu_fifo_t* f, void const * p_data);
uint16_t tu_fifo_write_n                (tu_fifo_t* f, void const * p_data, uint16_t n);
uint16_t tu_fifo_write_n_const_addr_full_words    (tu_fifo_t* f, const void * data, uint16_t n);

bool     tu_fifo_read                   (tu_fifo_t* f, void * p_buffer);
uint16_t tu_fifo_read_n                 (tu_fifo_t* f, void * p_buffer, uint16_t n);
uint16_t tu_fifo_read_n_const_addr_full_words     (tu_fifo_t* f, void * buffer, uint16_t n);

bool     tu_fifo_peek                   (tu_fifo_t* f, void * p_buffer);
uint16_t tu_fifo_peek_n                 (tu_fifo_t* f, void * p_buffer, uint16_t n);

uint16_t tu_fifo_count                  (tu_fifo_t* f);
uint16_t tu_fifo_remaining              (tu_fifo_t* f);
bool     tu_fifo_empty                  (tu_fifo_t* f);
bool     tu_fifo_full                   (tu_fifo_t* f);
bool     tu_fifo_overflowed             (tu_fifo_t* f);
void     tu_fifo_correct_read_pointer   (tu_fifo_t* f);

TU_ATTR_ALWAYS_INLINE static inline
uint16_t tu_fifo_depth(tu_fifo_t* f)
{
  return f->depth;
}

// Pointer modifications intended to be used in combinations with DMAs.
// USE WITH CARE - NO SAFETY CHECKS CONDUCTED HERE! NOT MUTEX PROTECTED!
void tu_fifo_advance_write_pointer(tu_fifo_t *f, uint16_t n);
void tu_fifo_advance_read_pointer (tu_fifo_t *f, uint16_t n);

// If you want to read/write from/to the FIFO by use of a DMA, you may need to conduct two copies
// to handle a possible wrapping part. These functions deliver a pointer to start
// reading/writing from/to and a valid linear length along which no wrap occurs.
void tu_fifo_get_read_info (tu_fifo_t *f, tu_fifo_buffer_info_t *info);
void tu_fifo_get_write_info(tu_fifo_t *f, tu_fifo_buffer_info_t *info);


#ifdef __cplusplus
}
#endif

#endif /* _TUSB_FIFO_H_ */
