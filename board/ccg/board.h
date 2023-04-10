/* Copyright 2020 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel ADL-P-RVP-ITE board-specific configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

#undef CONFIG_ADC
#undef CONFIG_LID_SWITCH
#undef CONFIG_HIBERNATE
#undef CONFIG_SPI_FLASH
#undef CONFIG_SWITCH

#define CONFIG_IO_EXPANDER
#define CONFIG_IO_EXPANDER_PCA9535

#define I2C_PORT_CHARGER IT83XX_I2C_CH_B
#define CONFIG_IO_EXPANDER_PORT_COUNT 1

/* I2C ports */
#define CONFIG_I2C
#define CONFIG_I2C_CONTROLLER

/* eSPI */
#define CONFIG_HOST_INTERFACE_ESPI

/* Optional feature - used by ITE */
#define CONFIG_IT83XX_FLASH_CLOCK_48MHZ
#define CONFIG_IT83XX_VCC_1P8V

#ifndef __ASSEMBLER__
#include "gpio_signal.h"

enum adc_channel {
	TEST,
};
#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
