/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* STM32L-discovery board configuration */

#ifndef __BOARD_H
#define __BOARD_H

/* Optional features */
#define CONFIG_USB_POWER_DELIVERY
#define CONFIG_ADC
#undef  CONFIG_WATCHDOG_HELP
#undef CONFIG_LID_SWITCH
#undef CONFIG_I2C

/*
 * Allow dangerous commands all the time, since we don't have a write protect
 * switch.
 */
#define CONFIG_SYSTEM_UNLOCKED

#ifndef __ASSEMBLER__

/* Timer selection */
#define TIM_CLOCK_MSB 3
#define TIM_CLOCK_LSB 9
#define TIM_CLOCK_PD_TX 10
#define TIM_CLOCK_PD_RX 2

/* GPIO signal list */
enum gpio_signal {
	/* Inputs with interrupt handlers are first for efficiency */
	GPIO_USER_BUTTON = 0,
	GPIO_DAC_OUT1,
	GPIO_PD_RX,
	/* Outputs */
	GPIO_PD_TX,
	GPIO_PD_TX_GND,
	GPIO_LED_BLUE,
	GPIO_LED_GREEN,
	GPIO_TEST0,
	GPIO_TEST1,
	GPIO_TEST2,
	/* Unimplemented signals we emulate */
	GPIO_ENTERING_RW,
	GPIO_WP_L,
	/* Number of GPIOs; not an actual GPIO */
	GPIO_COUNT
};

/* alias for common PD code */
#define GPIO_PD_TX_EN GPIO_PD_TX_GND

/* ADC signal */
enum adc_channel {
	ADC_CH_PD_RX = 0,
	/* Number of ADC channels */
	ADC_CH_COUNT
};

#endif /* !__ASSEMBLER__ */

#endif /* __BOARD_H */
