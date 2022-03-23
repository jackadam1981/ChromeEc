/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <stddef.h>

#include "console.h"
#include "cbi_ssfc.h"
#include "gpio.h"
#include "hooks.h"
#include "keyboard_8042.h"
#include "ps2.h"
#include "ps2_chip.h"
#include "time.h"
#include "registers.h"

#define CPRINTS(format, args...) cprints(CC_PS2, format, ## args)


void send_aux_data_to_device(uint8_t data)
{
	ps2_transmit_byte(NPCX_PS2_CH1, data);
}

static void board_init(void)
{
	ps2_enable_channel(NPCX_PS2_CH1, 1, send_aux_data_to_host_interrupt);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

static void send_command_to_trackpoint(uint8_t command1, uint8_t command2)
{
	ps2_transmit_byte(NPCX_PS2_CH1, TP_COMMAND);
	msleep(10);
	ps2_transmit_byte(NPCX_PS2_CH1, TP_TOGGLE);
	msleep(10);
	ps2_transmit_byte(NPCX_PS2_CH1, command1);
	msleep(10);
	ps2_transmit_byte(NPCX_PS2_CH1, command2);
	msleep(10);
}

int get_trackpoint_id(void)
{
	if (get_cbi_ssfc_trackpoint() == SSFC_SENSOR_TRACKPOINT_ELAN)
		return TP_VARIANT_ELAN;
	else
		return TP_VARIANT_SYNAPTICS;
}

/* Called on AP S0 -> S3 transition */
static void ps2_suspend(void)
{
	int trackpoint_id;
	/*
	 * When EC send PS2 command to PS2 device,
	 * PS2 device will return ACK(0xFA).
	 * EC will send it to host and cause host wake from suspend.
	 * So disable EC send data to host to avoid it.
	 */
	ps2_enable_channel(NPCX_PS2_CH1, 1, NULL);
	trackpoint_id = get_trackpoint_id();

	/* Send suspend mode to trackpoint */
	if (trackpoint_id == TP_VARIANT_ELAN)
		send_command_to_trackpoint(0x28, 0x08);
	else if (trackpoint_id == TP_VARIANT_SYNAPTICS)
		send_command_to_trackpoint(0x20, 0x10);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, ps2_suspend, HOOK_PRIO_DEFAULT);

/* Called on AP S3 -> S0 transition */
static void ps2_resume(void)
{
	int trackpoint_id;
	trackpoint_id = get_trackpoint_id();

	ps2_enable_channel(NPCX_PS2_CH1, 1, send_aux_data_to_host_interrupt);

	/*
	 * For Synaptics trackpoint, EC need to send command to it again.
	 * For Elan trackpoint, we just need to touch trackpoint and it wake.
	 */
	if (trackpoint_id == TP_VARIANT_SYNAPTICS)
		send_command_to_trackpoint(0x20, 0x10);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, ps2_resume, HOOK_PRIO_DEFAULT);
