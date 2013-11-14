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
	GPIO_TEST_OUTPUT,
	GPIO_TEST_INPUT,

	/* Number of GPIOs; not an actual GPIO */
	GPIO_COUNT
};

#endif /* !__ASSEMBLER__ */
#endif /* __BOARD_H */
