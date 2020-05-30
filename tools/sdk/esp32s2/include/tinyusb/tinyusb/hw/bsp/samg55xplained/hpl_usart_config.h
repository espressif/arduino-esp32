/* Auto-generated config file hpl_usart_config.h */
#ifndef HPL_USART_CONFIG_H
#define HPL_USART_CONFIG_H

// <<< Use Configuration Wizard in Context Menu >>>

#include <peripheral_clk_config.h>

#ifndef CONF_USART_7_ENABLE
#define CONF_USART_7_ENABLE 1
#endif

// <h> Basic Configuration

// <o> Frame parity
// <0x0=>Even parity
// <0x1=>Odd parity
// <0x2=>Parity forced to 0
// <0x3=>Parity forced to 1
// <0x4=>No parity
// <i> Parity bit mode for USART frame
// <id> usart_parity
#ifndef CONF_USART_7_PARITY
#define CONF_USART_7_PARITY 0x4
#endif

// <o> Character Size
// <0x0=>5 bits
// <0x1=>6 bits
// <0x2=>7 bits
// <0x3=>8 bits
// <i> Data character size in USART frame
// <id> usart_character_size
#ifndef CONF_USART_7_CHSIZE
#define CONF_USART_7_CHSIZE 0x3
#endif

// <o> Stop Bit
// <0=>1 stop bit
// <1=>1.5 stop bits
// <2=>2 stop bits
// <i> Number of stop bits in USART frame
// <id> usart_stop_bit
#ifndef CONF_USART_7_SBMODE
#define CONF_USART_7_SBMODE 0
#endif

// <o> Clock Output Select
// <0=>The USART does not drive the SCK pin
// <1=>The USART drives the SCK pin if USCLKS does not select the external clock SCK
// <i> Clock Output Select in USART sck, if in usrt master mode, please drive SCK.
// <id> usart_clock_output_select
#ifndef CONF_USART_7_CLKO
#define CONF_USART_7_CLKO 0
#endif

// <o> Baud rate <1-3000000>
// <i> USART baud rate setting
// <id> usart_baud_rate
#ifndef CONF_USART_7_BAUD
#define CONF_USART_7_BAUD 9600
#endif

// </h>

// <e> Advanced configuration
// <id> usart_advanced
#ifndef CONF_USART_7_ADVANCED_CONFIG
#define CONF_USART_7_ADVANCED_CONFIG 0
#endif

// <o> Channel Mode
// <0=>Normal Mode
// <1=>Automatic Echo
// <2=>Local Loopback
// <3=>Remote Loopback
// <i> Channel mode in USART frame
// <id> usart_channel_mode
#ifndef CONF_USART_7_CHMODE
#define CONF_USART_7_CHMODE 0
#endif

// <q> 9 bits character enable
// <i> Enable 9 bits character, this has high priority than 5/6/7/8 bits.
// <id> usart_9bits_enable
#ifndef CONF_USART_7_MODE9
#define CONF_USART_7_MODE9 0
#endif

// <o> Variable Sync
// <0=>User defined configuration
// <1=>sync field is updated when a character is written into US_THR
// <i> Variable Synchronization of Command/Data Sync Start Frarm Delimiter
// <id> variable_sync
#ifndef CONF_USART_7_VAR_SYNC
#define CONF_USART_7_VAR_SYNC 0
#endif

// <o> Oversampling Mode
// <0=>16 Oversampling
// <1=>8 Oversampling
// <i> Oversampling Mode in UART mode
// <id> usart__oversampling_mode
#ifndef CONF_USART_7_OVER
#define CONF_USART_7_OVER 0
#endif

// <o> Inhibit Non Ack
// <0=>The NACK is generated
// <1=>The NACK is not generated
// <i> Inhibit Non Acknowledge
// <id> usart__inack
#ifndef CONF_USART_7_INACK
#define CONF_USART_7_INACK 1
#endif

// <o> Disable Successive NACK
// <0=>NACK is sent on the ISO line as soon as a parity error occurs
// <1=>Many parity errors generate a NACK on the ISO line
// <i> Disable Successive NACK
// <id> usart_dsnack
#ifndef CONF_USART_7_DSNACK
#define CONF_USART_7_DSNACK 0
#endif

// <o> Inverted Data
// <0=>Data isn't inverted, nomal mode
// <1=>Data is inverted
// <i> Inverted Data
// <id> usart_invdata
#ifndef CONF_USART_7_INVDATA
#define CONF_USART_7_INVDATA 0
#endif

// <o> Maximum Number of Automatic Iteration <0-7>
// <i> Defines the maximum number of iterations in mode ISO7816, protocol T = 0.
// <id> usart_max_iteration
#ifndef CONF_USART_7_MAX_ITERATION
#define CONF_USART_7_MAX_ITERATION 0
#endif

// <q> Receive Line Filter enable
// <i> whether the USART filters the receive line using a three-sample filter
// <id> usart_receive_filter_enable
#ifndef CONF_USART_7_FILTER
#define CONF_USART_7_FILTER 0
#endif

// <q> Manchester Encoder/Decoder Enable
// <i> whether the USART Manchester Encoder/Decoder
// <id> usart_manchester_filter_enable
#ifndef CONF_USART_7_MAN
#define CONF_USART_7_MAN 0
#endif

// <o> Manchester Synchronization Mode
// <0=>The Manchester start bit is a 0 to 1 transition
// <1=>The Manchester start bit is a 1 to 0 transition
// <i> Manchester Synchronization Mode
// <id> usart_manchester_synchronization_mode
#ifndef CONF_USART_7_MODSYNC
#define CONF_USART_7_MODSYNC 0
#endif

// <o> Start Frame Delimiter Selector
// <0=>Start frame delimiter is COMMAND or DATA SYNC
// <1=>Start frame delimiter is one bit
// <i> Start Frame Delimiter Selector
// <id> usart_start_frame_delimiter
#ifndef CONF_USART_7_ONEBIT
#define CONF_USART_7_ONEBIT 0
#endif

// <o> Fractional Part <0-7>
// <i> Fractional part of the baud rate if baud rate generator is in fractional mode
// <id> usart_arch_fractional
#ifndef CONF_USART_7_FRACTIONAL
#define CONF_USART_7_FRACTIONAL 0x0
#endif

// <o> Data Order
// <0=>LSB is transmitted first
// <1=>MSB is transmitted first
// <i> Data order of the data bits in the frame
// <id> usart_arch_msbf
#ifndef CONF_USART_7_MSBF
#define CONF_USART_7_MSBF 0
#endif

// </e>

#define CONF_USART_7_MODE 0x0

// Calculate BAUD register value in UART mode
#if CONF_FLEXCOM7_CK_SRC < 3
#ifndef CONF_USART_7_BAUD_CD
#define CONF_USART_7_BAUD_CD ((CONF_FLEXCOM7_FREQUENCY) / CONF_USART_7_BAUD / 8 / (2 - CONF_USART_7_OVER))
#endif
#ifndef CONF_USART_7_BAUD_FP
#define CONF_USART_7_BAUD_FP                                                                                           \
	((CONF_FLEXCOM7_FREQUENCY) / CONF_USART_7_BAUD / (2 - CONF_USART_7_OVER) - 8 * CONF_USART_7_BAUD_CD)
#endif
#elif CONF_FLEXCOM7_CK_SRC == 3
// No division is active. The value written in US_BRGR has no effect.
#ifndef CONF_USART_7_BAUD_CD
#define CONF_USART_7_BAUD_CD 1
#endif
#ifndef CONF_USART_7_BAUD_FP
#define CONF_USART_7_BAUD_FP 1
#endif
#endif

// <<< end of configuration section >>>

#endif // HPL_USART_CONFIG_H
