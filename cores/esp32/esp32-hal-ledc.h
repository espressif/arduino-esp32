// Copyright 2015-2023 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _ESP32_HAL_LEDC_H_
#define _ESP32_HAL_LEDC_H_

#include "soc/soc_caps.h"
#if SOC_LEDC_SUPPORTED

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "hal/ledc_types.h"

typedef enum {
  NOTE_C,
  NOTE_Cs,
  NOTE_D,
  NOTE_Eb,
  NOTE_E,
  NOTE_F,
  NOTE_Fs,
  NOTE_G,
  NOTE_Gs,
  NOTE_A,
  NOTE_Bb,
  NOTE_B,
  NOTE_MAX
} note_t;

typedef void (*voidFuncPtr)(void);
typedef void (*voidFuncPtrArg)(void *);

typedef struct {
  uint8_t pin;                 // Pin assigned to channel
  uint8_t channel;             // Channel number
  uint8_t channel_resolution;  // Resolution of channel
  uint8_t timer_num;           // Timer number used by this channel
  uint32_t freq_hz;            // Frequency configured for this channel
  voidFuncPtr fn;
  void *arg;
#ifndef SOC_LEDC_SUPPORT_FADE_STOP
  SemaphoreHandle_t lock;  //xSemaphoreCreateBinary
#endif
} ledc_channel_handle_t;

/**
 * @brief Get the LEDC clock source.
 *
 * @return LEDC clock source.
 */
ledc_clk_cfg_t ledcGetClockSource(void);

/**
 * @brief Set the LEDC clock source.
 *
 * @param source LEDC clock source to set.
 *
 * @return true if LEDC clock source was successfully set, false otherwise.
 */
bool ledcSetClockSource(ledc_clk_cfg_t source);

/**
 * @brief Attach a pin to the LEDC driver, with a given frequency and resolution.
 *        Channel is automatically assigned.
 *
 * @param pin GPIO pin
 * @param freq frequency of PWM signal
 * @param resolution resolution for LEDC pin
 *
 * @return true if configuration is successful and pin was successfully attached, false otherwise.
 */
bool ledcAttach(uint8_t pin, uint32_t freq, uint8_t resolution);

/**
 * @brief Attach a pin to the LEDC driver, with a given frequency, resolution and channel.
 *
 * @param pin GPIO pin
 * @param freq frequency of PWM signal
 * @param resolution resolution for LEDC pin
 * @param channel LEDC channel to attach to
 *
 * @return true if configuration is successful and pin was successfully attached, false otherwise.
 */
bool ledcAttachChannel(uint8_t pin, uint32_t freq, uint8_t resolution, uint8_t channel);

/**
 * @brief Set the duty cycle of a given pin.
 *
 * @param pin GPIO pin
 * @param duty duty cycle to set
 *
 * @return true if duty cycle was successfully set, false otherwise.
 */
bool ledcWrite(uint8_t pin, uint32_t duty);

/**
 * @brief Set the duty cycle of a given channel.
 *
 * @param channel LEDC channel
 * @param duty duty cycle to set
 *
 * @return true if duty cycle was successfully set, false otherwise.
 */
bool ledcWriteChannel(uint8_t channel, uint32_t duty);

/**
 * @brief Sets the duty to 50 % PWM tone on selected frequency.
 *
 * @param pin GPIO pin
 * @param freq select frequency of pwm signal. If frequency is 0, duty will be set to 0.
 *
 * @return frequency if tone was successfully set.
 *         If ``0`` is returned, error occurs and LEDC pin was not configured.
 */
uint32_t ledcWriteTone(uint8_t pin, uint32_t freq);

/**
 * @brief Sets the LEDC pin to specific note.
 *
 * @param pin GPIO pin
 * @param note select note to be set (NOTE_C, NOTE_Cs, NOTE_D, NOTE_Eb, NOTE_E, NOTE_F, NOTE_Fs, NOTE_G, NOTE_Gs, NOTE_A, NOTE_Bb, NOTE_B).
 * @param octave select octave for note.
 *
 * @return frequency if note was successfully set.
 *         If ``0`` is returned, error occurs and LEDC pin was not configured.
 */
uint32_t ledcWriteNote(uint8_t pin, note_t note, uint8_t octave);

/**
 * @brief Read the duty cycle of a given LEDC pin.
 *
 * @param pin GPIO pin
 *
 * @return duty cycle of selected LEDC pin.
 */
uint32_t ledcRead(uint8_t pin);

/**
 * @brief Read the frequency of a given LEDC pin.
 *
 * @param pin GPIO pin
 *
 * @return frequency of selected LEDC pin.
 */
uint32_t ledcReadFreq(uint8_t pin);

/**
 * @brief Detach a pin from the LEDC driver.
 *
 * @param pin GPIO pin
 *
 * @return true if pin was successfully detached, false otherwise.
 */
bool ledcDetach(uint8_t pin);

/**
 * @brief Change the frequency and resolution of a given LEDC pin.
 *
 * @param pin GPIO pin
 * @param freq frequency of PWM signal
 * @param resolution resolution for LEDC pin
 *
 * @return frequency configured for the LEDC channel.
 *         If ``0`` is returned, error occurs and LEDC pin was not configured.
 */
uint32_t ledcChangeFrequency(uint8_t pin, uint32_t freq, uint8_t resolution);

/**
 * @brief Sets inverting of the output signal for a given LEDC pin.
 *
 * @param pin GPIO pin
 * @param out_invert select, if output should be inverted (true = inverting output).
 *
 * @return true if output inverting was successfully set, false otherwise.
 */
bool ledcOutputInvert(uint8_t pin, bool out_invert);

//Fade functions
/**
 * @brief Setup and start a fade on a given LEDC pin.
 *
 * @param pin GPIO pin
 * @param start_duty initial duty cycle of the fade
 * @param target_duty target duty cycle of the fade
 * @param max_fade_time_ms maximum fade time in milliseconds
 *
 * @return true if fade was successfully set and started, false otherwise.
 */
bool ledcFade(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms);

/**
 * @brief Setup and start a fade on a given LEDC pin with a callback function.
 *
 * @param pin GPIO pin
 * @param start_duty initial duty cycle of the fade
 * @param target_duty target duty cycle of the fade
 * @param max_fade_time_ms maximum fade time in milliseconds
 * @param userFunc callback function to be called after fade is finished
 *
 * @return true if fade was successfully set and started, false otherwise.
 */
bool ledcFadeWithInterrupt(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms, void (*userFunc)(void));

/**
 * @brief Setup and start a fade on a given LEDC pin with a callback function and argument.
 *
 * @param pin GPIO pin
 * @param start_duty initial duty cycle of the fade
 * @param target_duty target duty cycle of the fade
 * @param max_fade_time_ms maximum fade time in milliseconds
 * @param userFunc callback function to be called after fade is finished
 * @param arg argument to be passed to the callback function
 *
 * @return true if fade was successfully set and started, false otherwise.
 */
bool ledcFadeWithInterruptArg(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms, void (*userFunc)(void *), void *arg);

//Gamma Curve Fade functions - only available on supported chips
#ifdef SOC_LEDC_GAMMA_CURVE_FADE_SUPPORTED

/**
 * @brief Set a custom gamma correction lookup table for gamma curve fading.
 *        The LUT should contain normalized values (0.0 to 1.0) representing
 *        the gamma-corrected brightness curve.
 *
 * @param gamma_table Pointer to array of float values (0.0 to 1.0)
 * @param size Number of entries in the lookup table
 *
 * @return true if gamma table was successfully set, false otherwise.
 *
 * @note The LUT array must remain valid for as long as gamma fading is used.
 *       Larger tables provide smoother transitions but use more memory.
 */
bool ledcSetGammaTable(const float *gamma_table, uint16_t size);

/**
 * @brief Clear the current gamma correction lookup table.
 *        After calling this, gamma correction will use mathematical
 *        calculation with the default gamma factor (2.8).
 */
void ledcClearGammaTable(void);

/**
 * @brief Set the gamma factor for gamma correction.
 *
 * @param factor Gamma factor to use for gamma correction.
 */
void ledcSetGammaFactor(float factor);

/**
 * @brief Setup and start a gamma curve fade on a given LEDC pin.
 *        Gamma correction makes LED brightness changes appear more gradual to human eyes.
 *
 * @param pin GPIO pin
 * @param start_duty initial duty cycle of the fade
 * @param target_duty target duty cycle of the fade
 * @param max_fade_time_ms maximum fade time in milliseconds
 *
 * @return true if gamma fade was successfully set and started, false otherwise.
 *
 * @note This function is only available on ESP32 variants that support gamma curve fading.
 */
bool ledcFadeGamma(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms);

/**
 * @brief Setup and start a gamma curve fade on a given LEDC pin with a callback function.
 *
 * @param pin GPIO pin
 * @param start_duty initial duty cycle of the fade
 * @param target_duty target duty cycle of the fade
 * @param max_fade_time_ms maximum fade time in milliseconds
 * @param userFunc callback function to be called after fade is finished
 *
 * @return true if gamma fade was successfully set and started, false otherwise.
 *
 * @note This function is only available on ESP32 variants that support gamma curve fading.
 */
bool ledcFadeGammaWithInterrupt(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms, void (*userFunc)(void));

/**
 * @brief Setup and start a gamma curve fade on a given LEDC pin with a callback function and argument.
 *
 * @param pin GPIO pin
 * @param start_duty initial duty cycle of the fade
 * @param target_duty target duty cycle of the fade
 * @param max_fade_time_ms maximum fade time in milliseconds
 * @param userFunc callback function to be called after fade is finished
 * @param arg argument to be passed to the callback function
 *
 * @return true if gamma fade was successfully set and started, false otherwise.
 *
 * @note This function is only available on ESP32 variants that support gamma curve fading.
 */
bool ledcFadeGammaWithInterruptArg(uint8_t pin, uint32_t start_duty, uint32_t target_duty, int max_fade_time_ms, void (*userFunc)(void *), void *arg);
#endif  // SOC_LEDC_GAMMA_CURVE_FADE_SUPPORTED

#ifdef __cplusplus
}
#endif

#endif /* SOC_LEDC_SUPPORTED */
#endif /* _ESP32_HAL_LEDC_H_ */
