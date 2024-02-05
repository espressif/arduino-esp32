#ifndef __IO_PIN_REMAP_H__
#define __IO_PIN_REMAP_H__

#include "Arduino.h"

#if defined(BOARD_HAS_PIN_REMAP) && !defined(BOARD_USES_HW_GPIO_NUMBERS)

// Pin remapping functions
int8_t digitalPinToGPIONumber(int8_t digitalPin);
int8_t gpioNumberToDigitalPin(int8_t gpioNumber);

// Apply pin remapping to API only when building libraries and user sketch
#ifndef ARDUINO_CORE_BUILD

// Override APIs requiring pin remapping

// cores/esp32/Arduino.h
#define pulseInLong(pin, args...)   pulseInLong(digitalPinToGPIONumber(pin), args)
#define pulseIn(pin, args...)       pulseIn(digitalPinToGPIONumber(pin), args)
#define noTone(_pin)                noTone(digitalPinToGPIONumber(_pin))
#define tone(_pin, args...)         tone(digitalPinToGPIONumber(_pin), args)

// cores/esp32/esp32-hal.h
#define analogGetChannel(pin)               analogGetChannel(digitalPinToGPIONumber(pin))
#define analogWrite(pin, value)             analogWrite(digitalPinToGPIONumber(pin), value)
#define analogWriteFrequency(pin, freq)     analogWriteFrequency(digitalPinToGPIONumber(pin), freq)
#define analogWriteResolution(pin, bits)    analogWriteResolution(digitalPinToGPIONumber(pin), bits)

// cores/esp32/esp32-hal-adc.h
#define analogRead(pin)                             analogRead(digitalPinToGPIONumber(pin))
#define analogReadMilliVolts(pin)                   analogReadMilliVolts(digitalPinToGPIONumber(pin))
#define analogSetPinAttenuation(pin, attenuation)   analogSetPinAttenuation(digitalPinToGPIONumber(pin), attenuation)

// cores/esp32/esp32-hal-dac.h
#define dacDisable(pin)         dacDisable(digitalPinToGPIONumber(pin))
#define dacWrite(pin, value)    dacWrite(digitalPinToGPIONumber(pin), value)

// cores/esp32/esp32-hal-gpio.h
#define analogChannelToDigitalPin(channel)          gpioNumberToDigitalPin(analogChannelToDigitalPin(channel))
#define digitalPinToAnalogChannel(pin)              digitalPinToAnalogChannel(digitalPinToGPIONumber(pin))
#define digitalPinToTouchChannel(pin)               digitalPinToTouchChannel(digitalPinToGPIONumber(pin))
#define digitalRead(pin)                            digitalRead(digitalPinToGPIONumber(pin))
#define attachInterruptArg(pin, fcn, arg, mode)     attachInterruptArg(digitalPinToGPIONumber(pin), fcn, arg, mode)
#define attachInterrupt(pin, fcn, mode)             attachInterrupt(digitalPinToGPIONumber(pin), fcn, mode)
#define detachInterrupt(pin)                        detachInterrupt(digitalPinToGPIONumber(pin))
#define digitalWrite(pin, val)                      digitalWrite(digitalPinToGPIONumber(pin), val)
#define pinMode(pin, mode)                          pinMode(digitalPinToGPIONumber(pin), mode)

// cores/esp32/esp32-hal-i2c.h
#define i2cInit(i2c_num, sda, scl, clk_speed)   i2cInit(i2c_num, digitalPinToGPIONumber(sda), digitalPinToGPIONumber(scl), clk_speed)

// cores/esp32/esp32-hal-i2c-slave.h
#define i2cSlaveInit(num, sda, scl, slaveID, frequency, rx_len, tx_len)     i2cSlaveInit(num, digitalPinToGPIONumber(sda), digitalPinToGPIONumber(scl), slaveID, frequency, rx_len, tx_len)

// cores/esp32/esp32-hal-ledc.h
#define ledcAttach(pin, freq, resolution)           ledcAttach(digitalPinToGPIONumber(pin), freq, resolution)
#define ledcWrite(pin, duty)                        ledcWrite(digitalPinToGPIONumber(pin), duty)
#define ledcWriteTone(pin, freq)                    ledcWriteTone(digitalPinToGPIONumber(pin), freq)
#define ledcWriteNote(pin, note, octave)            ledcWriteNote(digitalPinToGPIONumber(pin), note, octave)
#define ledcRead(pin)                               ledcRead(digitalPinToGPIONumber(pin))
#define ledcReadFreq(pin)                           ledcReadFreq(digitalPinToGPIONumber(pin))
#define ledcDetach(pin)                             ledcDetach(digitalPinToGPIONumber(pin))
#define ledcChangeFrequency(pin, freq, resolution)  ledcChangeFrequency(digitalPinToGPIONumber(pin), freq, resolution)

#define ledcFade(pin, start_duty, target_duty, max_fade_time_ms)                                ledcFade(digitalPinToGPIONumber(pin), start_duty, target_duty, max_fade_time_ms)
#define ledcFadeWithInterrupt(pin, start_duty, target_duty, max_fade_time_ms, userFunc)         ledcFadeWithInterrupt(digitalPinToGPIONumber(pin), start_duty, target_duty, max_fade_time_ms, userFunc)
#define ledcFadeWithInterruptArg(pin, start_duty, target_duty, max_fade_time_ms, userFunc, arg) ledcFadeWithInterruptArg(digitalPinToGPIONumber(pin), start_duty, target_duty, max_fade_time_ms, userFunc, arg)

// cores/esp32/esp32-hal-matrix.h
#define pinMatrixInAttach(pin, signal, inverted)                    pinMatrixInAttach(digitalPinToGPIONumber(pin), signal, inverted)
#define pinMatrixOutAttach(pin, function, invertOut, invertEnable)  pinMatrixOutAttach(digitalPinToGPIONumber(pin), function, invertOut, invertEnable)
#define pinMatrixOutDetach(pin, invertOut, invertEnable)            pinMatrixOutDetach(digitalPinToGPIONumber(pin), invertOut, invertEnable)

// cores/esp32/esp32-hal-rgb-led.h
#define neopixelWrite(pin, red_val, green_val, blue_val)    neopixelWrite(digitalPinToGPIONumber(pin), red_val, green_val, blue_val)

// cores/esp32/esp32-hal-rmt.h
#define rmtInit(pin, channel_direction, memsize, frequency_Hz)                      rmtInit(digitalPinToGPIONumber(pin), channel_direction, memsize, frequency_Hz)
#define rmtWrite(pin, data, num_rmt_symbols, timeout_ms)                            rmtWrite(digitalPinToGPIONumber(pin), data, num_rmt_symbols, timeout_ms)
#define rmtWriteAsync(pin, data, num_rmt_symbols)                                   rmtWriteAsync(digitalPinToGPIONumber(pin), data, num_rmt_symbols)
#define rmtWriteLooping(pin, data, num_rmt_symbols)                                 rmtWriteLooping(digitalPinToGPIONumber(pin), data, num_rmt_symbols)
#define rmtTransmitCompleted(pin)                                                   rmtTransmitCompleted(digitalPinToGPIONumber(pin))
#define rmtRead(pin, data, num_rmt_symbols, timeout_ms)                             rmtRead(digitalPinToGPIONumber(pin), data, num_rmt_symbols, timeout_ms)
#define rmtReadAsync(pin, data, num_rmt_symbols)                                    rmtReadAsync(digitalPinToGPIONumber(pin), data, num_rmt_symbols)
#define rmtReceiveCompleted(pin)                                                    rmtReceiveCompleted(digitalPinToGPIONumber(pin))
#define rmtSetRxMaxThreshold(pin, idle_thres_ticks)                                 rmtSetRxMaxThreshold(digitalPinToGPIONumber(pin), idle_thres_ticks)
#define rmtSetCarrier(pin, carrier_en, carrier_level, frequency_Hz, duty_percent)   rmtSetCarrier(digitalPinToGPIONumber(pin), carrier_en, carrier_level, frequency_Hz, duty_percent)
#define rmtSetRxMinThreshold(pin, filter_pulse_ticks)                               rmtSetRxMinThreshold(digitalPinToGPIONumber(pin), filter_pulse_ticks)
#define rmtDeinit(pin)                                                              rmtDeinit(digitalPinToGPIONumber(pin))

// cores/esp32/esp32-hal-sigmadelta.h
#define sigmaDeltaAttach(pin, freq)     sigmaDeltaAttach(digitalPinToGPIONumber(pin), freq)
#define sigmaDeltaWrite(pin, duty)      sigmaDeltaWrite(digitalPinToGPIONumber(pin), duty)
#define sigmaDeltaDetach(pin)           sigmaDeltaDetach(digitalPinToGPIONumber(pin))

// cores/esp32/esp32-hal-spi.h
#define spiAttachSCK(spi, sck)          spiAttachSCK(spi, digitalPinToGPIONumber(sck))
#define spiAttachMISO(spi, miso)        spiAttachMISO(spi, digitalPinToGPIONumber(miso))
#define spiAttachMOSI(spi, mosi)        spiAttachMOSI(spi, digitalPinToGPIONumber(mosi))
#define spiDetachSCK(spi, sck)          spiDetachSCK(spi, digitalPinToGPIONumber(sck))
#define spiDetachMISO(spi, miso)        spiDetachMISO(spi, digitalPinToGPIONumber(miso))
#define spiDetachMOSI(spi, mosi)        spiDetachMOSI(spi, digitalPinToGPIONumber(mosi))
#define spiAttachSS(spi, cs_num, ss)    spiAttachSS(spi, cs_num, digitalPinToGPIONumber(ss))
#define spiDetachSS(spi, ss)            spiDetachSS(spi, digitalPinToGPIONumber(ss))

// cores/esp32/esp32-hal-touch.h
#define touchInterruptGetLastStatus(pin)                        touchInterruptGetLastStatus(digitalPinToGPIONumber(pin))
#define touchRead(pin)                                          touchRead(digitalPinToGPIONumber(pin))
#define touchAttachInterruptArg(pin, userFunc, arg, threshold)  touchAttachInterruptArg(digitalPinToGPIONumber(pin), userFunc, arg, threshold)
#define touchAttachInterrupt(pin, userFunc, threshold)          touchAttachInterrupt(digitalPinToGPIONumber(pin), userFunc, threshold)
#define touchDetachInterrupt(pin)                               touchDetachInterrupt(digitalPinToGPIONumber(pin))
#define touchSleepWakeUpEnable(pin, threshold)                  touchSleepWakeUpEnable(digitalPinToGPIONumber(pin), threshold)

// cores/esp32/esp32-hal-uart.h
#define uartBegin(uart_nr, baudrate, config, rxPin, txPin, rx_buffer_size, tx_buffer_size, inverted, rxfifo_full_thrhd) \
        uartBegin(uart_nr, baudrate, config, digitalPinToGPIONumber(rxPin), digitalPinToGPIONumber(txPin), rx_buffer_size, tx_buffer_size, inverted, rxfifo_full_thrhd)
#define uartSetPins(uart, rxPin, txPin, ctsPin, rtsPin) \
        uartSetPins(uart, digitalPinToGPIONumber(rxPin), digitalPinToGPIONumber(txPin), digitalPinToGPIONumber(ctsPin), digitalPinToGPIONumber(rtsPin))
#define uartDetachPins(uart, rxPin, txPin, ctsPin, rtsPin) \
        uartDetachPins(uart, digitalPinToGPIONumber(rxPin), digitalPinToGPIONumber(txPin), digitalPinToGPIONumber(ctsPin), digitalPinToGPIONumber(rtsPin))

#endif // ARDUINO_CORE_BUILD

#else

// pin remapping disabled: use stubs
#define digitalPinToGPIONumber(digitalPin) (digitalPin)
#define gpioNumberToDigitalPin(gpioNumber) (gpioNumber)

#endif

#endif /* __GPIO_PIN_REMAP_H__ */
