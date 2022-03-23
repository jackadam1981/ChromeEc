/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <stddef.h>

#include "gpio.h"
#include "hooks.h"
#include "keyboard_8042.h"
#include "ps2.h"
#include "ps2_chip.h"
#include "time.h"
#include "registers.h"


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
	uint8_t data_read;
	/*
	 * When EC send PS2 command to PS2 device,
	 * PS2 device will return ACK(0xFA).
	 * EC will send it to host and cause host wake from suspend.
	 * So disable EC send data to host to avoid it.
	 */
	ps2_enable_channel(NPCX_PS2_CH1, 1, NULL);

	/* Read PS2 device ID */
	ps2_transmit_byte(NPCX_PS2_CH1, 0xe1);
	msleep(2);
	data_read = 6;
	cprints(CC_PWM, "[SC] data_read=%x", data_read);
	cprints(CC_PWM, "[SC] TP_VARIANT_SYNAPTICS=%x", TP_VARIANT_SYNAPTICS);
	/* Send suspend mode to trackpoint */
	if (data_read == TP_VARIANT_ELAN) {
		ps2_transmit_byte(NPCX_PS2_CH1, 0xe2);
		msleep(10);
		ps2_transmit_byte(NPCX_PS2_CH1, 0x47);
		msleep(10);
		ps2_transmit_byte(NPCX_PS2_CH1, 0x28);
		msleep(10);
		ps2_transmit_byte(NPCX_PS2_CH1, 0x08);
		msleep(10);
	} else if(data_read == TP_VARIANT_SYNAPTICS) {
		ps2_transmit_byte(NPCX_PS2_CH1, 0xe2);
		msleep(10);
		ps2_transmit_byte(NPCX_PS2_CH1, 0x47);
		msleep(10);
		ps2_transmit_byte(NPCX_PS2_CH1, 0x20);
		msleep(10);
		ps2_transmit_byte(NPCX_PS2_CH1, 0x10);
		msleep(10);
	}
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, ps2_suspend, HOOK_PRIO_DEFAULT);

/* Called on AP S3 -> S0 transition */
static void ps2_resume(void)
{
	ps2_enable_channel(NPCX_PS2_CH1, 1, send_aux_data_to_host_interrupt);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, ps2_resume, HOOK_PRIO_DEFAULT);
