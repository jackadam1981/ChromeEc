/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Fruitpie board configuration */

#ifndef __BOARD_H
#define __BOARD_H

/* 48 MHz SYSCLK clock frequency */
#define CPU_CLOCK 48000000

/* the UART console is on USART2 (PA14/PA15) */
#undef CONFIG_UART_CONSOLE
#define CONFIG_UART_CONSOLE 2

/* Optional features */
#undef CONFIG_WATCHDOG_HELP
#undef CONFIG_LID_SWITCH
#undef CONFIG_TASK_PROFILING
#define CONFIG_ADC

/*
 * Allow dangerous commands all the time, since we don't have a write protect
 * switch.
 */
#define CONFIG_SYSTEM_UNLOCKED

#ifndef __ASSEMBLER__

/* Timer selection */
#define CONFIG_STM_HWTIMER32
#define TIM_CLOCK32  2
#define TIM_ADC      3

/* GPIO signal list */
enum gpio_signal {
	/* Inputs with interrupt handlers are first for efficiency */
	GPIO_USER_BUTTON = 0,
	/* */
	GPIO_AIN1,
	GPIO_AIN2,
	GPIO_AIN3,
	GPIO_AIN4,
	GPIO_AIN5,
	GPIO_AIN6,
	GPIO_AIN7,
	GPIO_AIN8,
	GPIO_AIN9,
	/* Outputs */
	GPIO_LED_BLUE,
	GPIO_LED_GREEN,
	/* Unimplemented signals we emulate */
	GPIO_ENTERING_RW,
	GPIO_WP_L,
	/* Number of GPIOs; not an actual GPIO */
	GPIO_COUNT
};

/* ADC signal */
enum adc_channel {
	ADC_CH_AIN0 = 0,
	ADC_CH_AIN1,
	ADC_CH_AIN2,
	ADC_CH_AIN3,
	ADC_CH_AIN4,
	ADC_CH_AIN5,
	ADC_CH_AIN6,
	ADC_CH_AIN7,
	ADC_CH_AIN8,
	ADC_CH_AIN9,
	/* Number of ADC channels */
	ADC_CH_COUNT
};

#endif /* !__ASSEMBLER__ */

#endif /* __BOARD_H */
