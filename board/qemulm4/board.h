/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* LM4 QEMU emulator configuration */

#ifndef __BOARD_H
#define __BOARD_H

/* Optional features */
#define CONFIG_CONSOLE_CMDHELP
#define CONFIG_SYSTEM_UNLOCKED  /* Allow dangerous commands */

enum adc_channel {
	ADC_CH_EC_TEMP = 0,  /* EC internal die temperature in degrees K. */
	ADC_CH_COUNT
};

/* I2C ports */
#define I2C_PORTS_USED 0 /* No I2C port defined */

/* GPIO signal list */
enum gpio_signal {
	GPIO_RECOVERYn = 0,       /* Recovery signal from DOWN button */
	/* Signals which aren't implemented on BDS but we'll emulate anyway, to
	 * make it more convenient to debug other code. */
	GPIO_WRITE_PROTECT,       /* Write protect input */

	/* Number of GPIOs; not an actual GPIO */
	GPIO_COUNT
};

/* Target value for BOOTCFG.  This currently toggles the polarity bit without
 * enabling the boot loader, simply to prove we can program it. */
#define BOOTCFG_VALUE 0xfffffdfe

enum temp_sensor_id {
	TEMP_SENSOR_MOCK_CPU = 0,
	TEMP_SENSOR_MOCK_BOARD,
	TEMP_SENSOR_MOCK_CASE,

	TEMP_SENSOR_COUNT
};

void configure_board(void);

#endif /* __BOARD_H */
