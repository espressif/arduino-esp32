/*****************************************************************************
 *
 *   Copyright(C) 2011, Embedded Artists AB
 *   All rights reserved.
 *
 ******************************************************************************
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * products. This software is supplied "AS IS" without any warranties.
 * Embedded Artists AB assumes no responsibility or liability for the
 * use of the software, conveys no license or title under any patent,
 * copyright, or mask work right to the product. Embedded Artists AB
 * reserves the right to make changes in the software without
 * notification. Embedded Artists AB also make no representation or
 * warranty that such application will be suitable for the specified
 * use without further testing or modification.
 *****************************************************************************/
#ifndef __PCA9532C_H
#define __PCA9532C_H


#define PCA9532_I2C_ADDR    (0xC0>>1)

#define PCA9532_INPUT0 0x00
#define PCA9532_INPUT1 0x01
#define PCA9532_PSC0   0x02
#define PCA9532_PWM0   0x03
#define PCA9532_PSC1   0x04
#define PCA9532_PWM1   0x05
#define PCA9532_LS0    0x06
#define PCA9532_LS1    0x07
#define PCA9532_LS2    0x08
#define PCA9532_LS3    0x09

#define PCA9532_AUTO_INC 0x10


/*
 * The Keys on the base board are mapped to LED0 -> LED3 on
 * the PCA9532.
 */

#define KEY1 0x0001
#define KEY2 0x0002
#define KEY3 0x0004
#define KEY4 0x0008

#define KEY_MASK 0x000F

/*
 * MMC Card Detect and MMC Write Protect are mapped to LED4 
 * and LED5 on the PCA9532. Please note that WP is active low.
 */

#define MMC_CD 0x0010
#define MMC_WP 0x0020

#define MMC_MASK  0x30

/* NOTE: LED6 and LED7 on PCA9532 are not connected to anything */
#define PCA9532_NOT_USED 0xC0

/*
 * Below are the LED constants to use when enabling/disabling a LED.
 * The LED names are the names printed on the base board and not
 * the names from the PCA9532 device. base_LED1 -> LED8 on PCA9532,
 * base_LED2 -> LED9, and so on.
 */

#define LED1 0x0100
#define LED2 0x0200
#define LED3 0x0400
#define LED4 0x0800
#define LED5 0x1000
#define LED6 0x2000
#define LED7 0x4000
#define LED8 0x8000

#define LED_MASK 0xFF00

void pca9532_init (void);
uint16_t pca9532_getLedState (uint32_t shadow);
void pca9532_setLeds (uint16_t ledOnMask, uint16_t ledOffMask);
void pca9532_setBlink0Period(uint8_t period);
void pca9532_setBlink0Duty(uint8_t duty);
void pca9532_setBlink0Leds(uint16_t ledMask);
void pca9532_setBlink1Period(uint8_t period);
void pca9532_setBlink1Duty(uint8_t duty);
void pca9532_setBlink1Leds(uint16_t ledMask);

#endif /* end __PCA9532C_H */
/****************************************************************************
**                            End Of File
*****************************************************************************/
