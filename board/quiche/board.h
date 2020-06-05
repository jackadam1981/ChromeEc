/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Quiche board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* Baseboard features */
#include "baseboard.h"

/* Optional features */
#define CONFIG_SYSTEM_UNLOCKED /* Allow dangerous commands while in dev. */

/* USB Type C and USB PD defines */
#define USB_PD_PORT_HOST   0
#define USB_PD_PORT_DP   1

#define CONFIG_USB_MUX_PS8822

/* USB Type A Features */

/* BC 1.2 */


#ifndef __ASSEMBLER__

#include "registers.h"

#define GPIO_TRIGGER_1 GPIO_USB3_A3_CDP_EN
#define GPIO_TRIGGER_2 GPIO_USB3_A4_CDP_EN


enum  debug_gpio {
	TRIGGER_1 = 0,
	TRIGGER_2,
};

void board_debug_gpio(int trigger, int enable);

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
