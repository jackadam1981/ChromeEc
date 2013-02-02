/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* McCroskey board configuration */

#ifndef __BOARD_H
#define __BOARD_H

/* 48 MHz SYSCLK clock frequency */
#define CPU_CLOCK 48000000

/* Use USART1 as console serial port */
#define CONFIG_CONSOLE_UART 1

/* Debug features */
#define CONFIG_PANIC_HELP
#define CONFIG_ASSERT_HELP
#define CONFIG_CONSOLE_CMDHELP

#undef  CONFIG_TASK_PROFILING
#define CONFIG_WATCHDOG_HELP

/* use STOP mode when we have nothing to do */
#define CONFIG_LOW_POWER_IDLE

#ifndef __ASSEMBLER__

/* By default, enable all console messages except keyboard */
#define CC_DEFAULT	(CC_ALL)

#define USB_CHARGE_PORT_COUNT 0

/* EC drives 13 outputs to keyboard matrix */
#define KB_OUTPUTS 13

#define I2C_PORT_BATTERY I2C_PORT_HOST
#define I2C_PORT_CHARGER I2C_PORT_HOST
#define I2C_PORT_SLAVE 1
#define CONFIG_ARBITRATE_I2C I2C_PORT_HOST

/* Timer selection */
#define TIM_CLOCK_MSB 3
#define TIM_CLOCK_LSB 4

/* GPIO signal list */
enum gpio_signal {
	GPIO_KB_IN00,
	GPIO_KB_IN01,
	GPIO_KB_IN02,
	GPIO_KB_IN03,
	GPIO_KB_IN04,
	GPIO_KB_IN05,
	GPIO_KB_IN06,
	GPIO_KB_IN07,
	GPIO_KBD_PWR_BUTTON,
	GPIO_OMZO_RDY_L,
	GPIO_OZMO_RST_L,
	GPIO_VBUS_UP_DET,
	GPIO_OZMO_REQ_L,
	GPIO_CHARGE_ZERO,
	GPIO_CHARGE_SHUNT,
	GPIO_PMIC_INT_L,
	GPIO_I2C_SCL,
	GPIO_I2C_SDA,
	GPIO_KB_OUT00,
	GPIO_KB_OUT01,
	GPIO_KB_OUT02,
	GPIO_KB_OUT03,
	GPIO_KB_OUT04,
	GPIO_KB_OUT05,
	GPIO_KB_OUT06,
	GPIO_KB_OUT07,
	GPIO_KB_OUT08,
	GPIO_KB_OUT09,
	GPIO_KB_OUT10,
	GPIO_KB_OUT11,
	GPIO_KB_OUT12,
	GPIO_USB_VBUS_CTRL,
	GPIO_HUB_RESET,
	GPIO_WRITE_PROTECTn,
#if 0
	GPIO_BL_PWM,
	GPIO_STM_USBDM,
	GPIO_STM_USBDP,
	GPIO_JTMS_SWDIO,
	GPIO_JTCK_SWCLK,
	GPIO_JTDI,
	GPIO_JTDO,
	GPIO_JNTRST,
	GPIO_OSC32_OUT,
#endif
	GPIO_COUNT
};

void configure_board(void);

void matrix_interrupt(enum gpio_signal signal);

void system_warm_reboot(void);

#endif /* !__ASSEMBLER__ */

#endif /* __BOARD_H */
