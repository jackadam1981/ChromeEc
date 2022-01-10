/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hooks.h"
#include "keyboard_8042.h"
#include "ps2_chip.h"

void send_aux_data_to_device(uint8_t data)
{
	ps2_transmit_byte(NPCX_PS2_CH1, data);
}

static void board_init(void)
{
	if (gpio_get_level(GPIO_EC_PCH_SYS_PWROK)) {
		gpio_set_alternate_function(GPIO_PORT_6, BIT(2), GPIO_ALT_FUNC_1);
		gpio_set_alternate_function(GPIO_PORT_6, BIT(3), GPIO_ALT_FUNC_1);
		ps2_enable_channel(NPCX_PS2_CH1, 1, send_aux_data_to_host_interrupt);
	} else {
		gpio_set_flags(GPIO_EC_PS2_SCL_TPAD, GPIO_ODR_LOW);
		gpio_set_flags(GPIO_EC_PS2_SDA_TPAD, GPIO_ODR_LOW);
		gpio_set_alternate_function(GPIO_PORT_6, BIT(2), GPIO_ALT_FUNC_NONE);
		gpio_set_alternate_function(GPIO_PORT_6, BIT(3), GPIO_ALT_FUNC_NONE);
		ps2_enable_channel(NPCX_PS2_CH1, 0, send_aux_data_to_host_interrupt);
	}
}
DECLARE_HOOK(HOOK_SECOND, board_init, HOOK_PRIO_DEFAULT);
