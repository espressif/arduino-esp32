////////////////////////////////////////////////////////////////////////////
//                        **** AUDIO-STRETCH ****                         //
//                      Time Domain Harmonic Scaler                       //
//                    Copyright (c) 2019 David Bryant                     //
//                          All Rights Reserved.                          //
//      Distributed under the BSD Software License (see license.txt)      //
////////////////////////////////////////////////////////////////////////////

// stretch.h

// Time Domain Harmonic Compression and Expansion
//
// This library performs time domain harmonic scaling with pitch detection
// to stretch the timing of a 16-bit PCM signal (either mono or stereo) from
// 1/2 to 2 times its original length. This is done without altering any of
// its tonal characteristics.

#ifndef STRETCH_H
#define STRETCH_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void *StretchHandle;

/* extern function */
StretchHandle stretch_init (int shortest_period, int longest_period, int num_chans, int fast_mode);
int stretch_samples (StretchHandle handle, short *samples, int num_samples, short *output, float ratio);
int stretch_flush (StretchHandle handle, short *output);
void stretch_deinit (StretchHandle handle);

/* internel function */
StretchHandle stretcher_init_internal(int shortest_period, int longest_period, int buff_len);
void stretcher_deinit (StretchHandle handle);
int stretcher_is_empty(StretchHandle handle);
int stretcher_is_full(StretchHandle handle, int num_samples);
int stretcher_push_data(StretchHandle handle, short *samples, int num_samples);
int stretcher_stretch_samples(StretchHandle handle, short *output, float ratio);
int stretcher_stretch_samples_flash(StretchHandle handle, short *output, float ratio, const short *period_data, 
                                    int *start_idx, int end_idx);

#ifdef __cplusplus
}
#endif

#endif

