/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelerometer.h"
#include "console.h"
#include "hooks.h"
#include "host_command.h"
#include "keyboard_8042.h"
#include "math.h"
#include "timer.h"
#include "task.h"
#include "util.h"

static int accel_disp;

/* GESTURE TODO: Sampling interval for accelerometers. */
static int accel_interval_ms = 250;

/* GESTURE TODO: Define the size of the gesture configuration data. */
#define GESTURE_CONFIG_LENGTH	500

/* Configuration data. */
static uint8_t gesture_config_data[GESTURE_CONFIG_LENGTH];


static void gesture_recognize(int s1, int s2, int s3, int s4, int s5, int s6)
{
	/*
	 * GESTURE TODO: Fill in this function to take in accelerometer data
	 * (two 3-axis accelerometers) and determine if a gesture is
	 * recognized.
	 */

	/*
	 * If a gesture is recognized, here are some examples of host events
	 * that can be triggered for testing.
	 */
#if 0
	/* Signal lid opened. */
	host_set_single_event(EC_HOST_EVENT_LID_OPEN);

	/* Press and release a key. */
	keyboard_state_changed(4, 1, 1);
	keyboard_state_changed(4, 1, 0);
#endif

	return;
}

void motion_sense_task(void)
{
	timestamp_t ts0, ts1;
	int wait_us;
	int acc_lidX, acc_lidY, acc_lidZ, acc_baseX, acc_baseY, acc_baseZ;
	uint8_t *mptr = host_get_memmap(EC_MEMMAP_GESTURE_DATA);
	uint8_t sample_id = 0;

	/*
	 * TODO: remove this workaround once we get proto boards: workaround
	 * enables accelerometers after AP is powered on.
	 */
	task_wait_event(3000000);

	/* Initialize accelerometers. */
	accel_init(ACCEL_ADDR_LID);
	accel_init(ACCEL_ADDR_BASE);

	while (1) {
		ts0 = get_time();

		/* Read all accelerations. */
		read_accel(ACCEL_ADDR_LID, &acc_lidX, &acc_lidY, &acc_lidZ);
		read_accel(ACCEL_ADDR_BASE, &acc_baseX, &acc_baseY, &acc_baseZ);

		/*
		 * Use the EC_MEMMAP_GESTURE_SAMPLE_ID as a status byte. The
		 * top bit is a busy bit, and the bottom seven are a counter.
		 * Set the busy bit before writing the sensor data. Increment
		 * the counter and clear the busy bit after writing the sensors
		 * data. On the host side, the host needs to make sure the busy
		 * bit is not set and that the counter remains the same before
		 * and after reading the data.
		 */
		(*(mptr + EC_MEMMAP_GESTURE_SAMPLE_ID)) |= 0x80;

		/* Update all sensor data in host shared memory. */
		*(mptr + 0x0) = (acc_lidX >> 8) & 0xff;
		*(mptr + 0x1) = acc_lidX & 0xff;
		*(mptr + 0x2) = (acc_lidY >> 8) & 0xff;
		*(mptr + 0x3) = acc_lidY & 0xff;
		*(mptr + 0x4) = (acc_lidZ >> 8) & 0xff;
		*(mptr + 0x5) = acc_lidZ & 0xff;
		*(mptr + 0x6) = (acc_baseX >> 8) & 0xff;
		*(mptr + 0x7) = acc_baseX & 0xff;
		*(mptr + 0x8) = (acc_baseY >> 8) & 0xff;
		*(mptr + 0x9) = acc_baseY & 0xff;
		*(mptr + 0xa) = (acc_baseZ >> 8) & 0xff;
		*(mptr + 0xb) = acc_baseZ & 0xff;

		/*
		 * Increment sample id and clear busy bit to signal we finished
		 * updating data.
		 */
		sample_id = (sample_id + 1) & ~0x80;
		(*(mptr + EC_MEMMAP_GESTURE_SAMPLE_ID)) = sample_id;

		/* See if a gesture is recognized. */
		gesture_recognize(acc_lidX, acc_lidY, acc_lidZ,
				acc_baseX, acc_baseY, acc_baseZ);

		if (accel_disp) {
			ccprintf("%d,\t%d,\t%d,\t", acc_lidX, acc_lidY,
					acc_lidZ);
			ccprintf("%d,\t%d,\t%d,\n", acc_baseX, acc_baseY,
					acc_baseZ);
		}

		ts1 = get_time();
		wait_us = accel_interval_ms*1000 - (ts1.val-ts0.val);
		if (wait_us > 0)
			task_wait_event(wait_us);
	}
}



/*****************************************************************************/
/* Console commands */
static int command_ctrl_accels(int argc, char **argv)
{
	char *e;
	int val;

	if (argc > 3)
		return EC_ERROR_PARAM_COUNT;

	/* First argument is on/off whether to display accel data. */
	if (argc > 1) {
		if (!parse_bool(argv[1], &val))
			return EC_ERROR_PARAM1;

		accel_disp = val;

		if (accel_disp)
			ccprintf("\nLidX\tLidY\tLidZ\tBaseX\tBaseY\tBaseZ\n");
	}

	/* Second arg changes the accel task time interval. */
	if (argc > 2) {
		val = strtoi(argv[2], &e, 0);
		if (*e)
			return EC_ERROR_PARAM2;

		accel_interval_ms = val;
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(accel, command_ctrl_accels,
	"on/off [interval]",
	"Control acceleration task.", NULL);

static int command_get_config(int argc, char **argv)
{
	int i, j;

	for (i = 0; i < GESTURE_CONFIG_LENGTH; i += 16) {
		for (j = 0; i+j < GESTURE_CONFIG_LENGTH && j < 16; j++)
			ccprintf("%02x ", gesture_config_data[i+j]);
		ccprintf("\n");
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(gesturecfg, command_get_config,
	"",
	"Print contents of gesture config data.", NULL);

/*****************************************************************************/
/* Host commands */
static int gesture_config_write(struct host_cmd_handler_args *args)
{
	const struct ec_params_gesture_config_write *p = args->params;

	/* Make sure data will fit into gesture_config_data. */
	if (p->offset + p->size > GESTURE_CONFIG_LENGTH)
		return EC_RES_INVALID_PARAM;

	ccprintf("Writing config data at offset: %d, size %d:\n", p->offset,
			p->size);

	/* Copy host data into RAM. */
	memcpy(&gesture_config_data[p->offset], (const uint8_t *)(p+1),
			p->size);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_GESTURE_CONFIG_WRITE,
		     gesture_config_write,
		     EC_VER_MASK(0) | EC_VER_MASK(EC_VER_GESTURE_CONFIG_WRITE));
