/* Copyright 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* IT83xx development board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* Select Baseboard features */
#include "baseboard.h"

#undef CONFIG_KEYBOARD_COL2_INVERTED
/* Use I/O expander  */
#define CONFIG_IO_EXPANDER_IT8801
#define CONFIG_IO_EXPANDER
#define CONFIG_IO_EXPANDER_PORT_COUNT 1

/* Setting master port */
#define IT8801_KEYBOARD_PWM_I2C_PORT 1
/* IT8801 I2C address */
#define IT8801_KEYBOARD_PWM_I2C_ADDR_FLAGS    IT8801_I2C_ADDR1

#define CONFIG_KEYBOARD_NOT_RAW

/* Optional features */
#define CONFIG_DAC

#ifndef __ASSEMBLER__

#include "gpio_signal.h"
#include "registers.h"

enum pwm_channel {
	PWM_CH_FAN,
	PWM_CH_WITH_DSLEEP_FLAG,
	/* Number of PWM channels */
	PWM_CH_COUNT
};

enum adc_channel {
	ADC_VBUSSA,
	ADC_VBUSSB,
	ADC_EVB_CH_13,
	ADC_EVB_CH_14,
	ADC_EVB_CH_15,
	ADC_EVB_CH_16,
	/* Number of ADC channels */
	ADC_CH_COUNT
};

#endif /* !__ASSEMBLER__ */
#endif /* __CROS_EC_BOARD_H */
