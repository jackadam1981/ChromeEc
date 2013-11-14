/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* IT8380 development board configuration */

#ifndef __BOARD_H
#define __BOARD_H

#ifndef __ASSEMBLER__

/* stubbed features */
#undef CONFIG_LID_SWITCH

enum gpio_signal {
	/* Unimplemented GPIOs */
	GPIO_ENTERING_RW,
	GPIO_H_LED0,
	GPIO_H_LED1,
	GPIO_H_LED2,
	GPIO_H_LED3,
	GPIO_H_LED4,
	GPIO_H_LED5,
	GPIO_H_LED6,
	GPIO_H_LED7,
	GPIO_L_LED0,
	GPIO_L_LED1,
	GPIO_L_LED2,
	GPIO_L_LED3,
	GPIO_L_LED4,
	GPIO_L_LED5,
	GPIO_L_LED6,
	GPIO_L_LED7,
	GPIO_SW_1,
	GPIO_SW_2,
	GPIO_SW_3,
	GPIO_SW_4,

	/* Number of GPIOs; not an actual GPIO */
	GPIO_COUNT
};

#endif /* !__ASSEMBLER__ */
#endif /* __BOARD_H */
