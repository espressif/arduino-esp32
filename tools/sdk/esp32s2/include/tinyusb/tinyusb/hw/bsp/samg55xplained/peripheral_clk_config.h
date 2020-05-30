/* Auto-generated config file peripheral_clk_config.h */
#ifndef PERIPHERAL_CLK_CONFIG_H
#define PERIPHERAL_CLK_CONFIG_H

// <<< Use Configuration Wizard in Context Menu >>>

/**
 * \def CONF_HCLK_FREQUENCY
 * \brief HCLK's Clock frequency
 */
#ifndef CONF_HCLK_FREQUENCY
#define CONF_HCLK_FREQUENCY 8000000
#endif

/**
 * \def CONF_FCLK_FREQUENCY
 * \brief FCLK's Clock frequency
 */
#ifndef CONF_FCLK_FREQUENCY
#define CONF_FCLK_FREQUENCY 8000000
#endif

/**
 * \def CONF_CPU_FREQUENCY
 * \brief CPU's Clock frequency
 */
#ifndef CONF_CPU_FREQUENCY
#define CONF_CPU_FREQUENCY 8000000
#endif

/**
 * \def CONF_SLCK_FREQUENCY
 * \brief Slow Clock frequency
 */
#define CONF_SLCK_FREQUENCY 32768

/**
 * \def CONF_MCK_FREQUENCY
 * \brief Master Clock frequency
 */
#define CONF_MCK_FREQUENCY 8000000

// <o> USB Clock Source
// <0=> USB Clock Controller (USB_48M)
// <id> usb_clock_source
// <i> Select the clock source for USB.
#ifndef CONF_UDP_SRC
#define CONF_UDP_SRC 0
#endif

/**
 * \def CONF_UDP_FREQUENCY
 * \brief UDP's Clock frequency
 */
#ifndef CONF_UDP_FREQUENCY
#define CONF_UDP_FREQUENCY 48005120
#endif

// <h> FLEXCOM Clock Settings
// <o> FLEXCOM Clock source
// <0=> Master Clock (MCK)
// <1=> MCK / 8
// <2=> Programmable Clock Controller 6 (PMC_PCK6)
// <2=> Programmable Clock Controller 7 (PMC_PCK7)
// <3=> External Clock
// <i> This defines the clock source for the FLEXCOM, PCK6 is used for FLEXCOM0/1/2/3 and PCK7 is used for FLEXCOM4/5/6/7
// <id> flexcom_clock_source
#ifndef CONF_FLEXCOM7_CK_SRC
#define CONF_FLEXCOM7_CK_SRC 0
#endif

// <o> FLEXCOM External Clock Input on SCK <1-4294967295>
// <i> Inputs the external clock frequency on SCK
// <id> flexcom_clock_freq
#ifndef CONF_FLEXCOM7_SCK_FREQ
#define CONF_FLEXCOM7_SCK_FREQ 10000000
#endif

#ifndef CONF_FLEXCOM7_FREQUENCY
#define CONF_FLEXCOM7_FREQUENCY 8000000
#endif

// <<< end of configuration section >>>

#endif // PERIPHERAL_CLK_CONFIG_H
