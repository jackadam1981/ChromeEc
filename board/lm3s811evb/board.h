/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* QEMU Stellaris LM3S6965EVB board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* Use external clock */
#define CONFIG_STM32_CLOCK_HSE_HZ 5000000

#undef CONFIG_HIBERNATE
#undef CONFIG_WP_ALWAYS
#undef CONFIG_FLASH_PSTATE
#undef CONFIG_FLASH_PHYSICAL
#undef CONFIG_FLASH
#undef CONFIG_FMAP
#undef CONFIG_DMA
#undef CONFIG_SPI_FLASH
#define CONFIG_BOARD_POST_GPIO_INIT

#undef CONFIG_FW_INCLUDE_RO
#undef CONFIG_RW_MEM_OFF
#define CONFIG_RW_MEM_OFF 0
#undef CONFIG_RO_SIZE
#define CONFIG_RO_SIZE 0
/* Fake full size if we had a RO partition */
#undef CONFIG_RW_SIZE
#define CONFIG_RW_SIZE CONFIG_FLASH_SIZE

#undef CONFIG_RO_MEM_OFF
#define CONFIG_RO_MEM_OFF 0

#define CONFIG_ADC

#define CONFIG_I2C
#define CONFIG_I2C_MASTER
#define I2C_PORT_0 0

/* This is not actually an EC so disable some features. */
#undef CONFIG_WATCHDOG_HELP
#undef CONFIG_LID_SWITCH
#undef CONFIG_WATCHDOG

#ifndef __ASSEMBLER__

enum adc_channel {
	/* EC internal die temperature in degrees K. */
	ADC_CH_EC_TEMP = 0,

	ADC_CH_COUNT
};

#include "gpio_signal.h"

#endif /* !__ASSEMBLER__ */
#endif /* __CROS_EC_BOARD_H */
