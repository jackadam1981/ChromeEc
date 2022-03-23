/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio.h"
#include "hooks.h"
#include "keyboard_8042.h"
#include "ps2_chip.h"
#include "time.h"

void send_aux_data_to_device(uint8_t data)
{
	ps2_transmit_byte(NPCX_PS2_CH1, data);
}

static void board_init(void)
{
	ps2_enable_channel(NPCX_PS2_CH1, 1, send_aux_data_to_host_interrupt);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/* Called on AP S0 -> S3 transition */
static void ps2_suspend(void)
{
	ps2_transmit_byte(NPCX_PS2_CH1, 0xe2);
	msleep(10);
	ps2_transmit_byte(NPCX_PS2_CH1, 0x47);
	msleep(10);
	ps2_transmit_byte(NPCX_PS2_CH1, 0x28);
	msleep(10);
	ps2_transmit_byte(NPCX_PS2_CH1, 0x08);
	msleep(10);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, ps2_suspend, HOOK_PRIO_DEFAULT);
