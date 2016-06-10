/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Barometer sensor module to read from different pressure sensors. */

#include "barometer.h"
#include "console.h"
#include "stddef.h"

#define BARO_COUNT 1

#define CPRINTF(format, args...) cprintf(CC_BARO, format, ## args)
#define CPRINTS(format, args...) cprints(CC_BARO, format, ## args)

int get_pressure(int id, int *comp_pressure)
{
	int uncomp_pressure, ret;

	ret = baro[id].read(&uncomp_pressure);
	if (ret) {
		CPRINTF("\nError = %d\n");
		return ret;
	}

	CPRINTF("\nUncompensated pressure = %d", uncomp_pressure);

	*comp_pressure = baro[id].comp(uncomp_pressure);
	return 0;
}

int baro_set_work_mode(int id, int mode)
{
	return baro[id].set_work_mode(mode);
}

int baro_init(int id)
{
	return baro[id].init(&baro[id]);
}

/*****************************************************************************/
/* Console commands */

#ifdef CONFIG_CMD_BARO
static int command_baro_init(int argc, char **argv)
{
	int i, rv, val;

	for (i = 0; i < BARO_COUNT; i++) {
		ccprintf("%s: ", baro[i].name);

		rv = baro_init(i);
		switch (rv) {
		case EC_SUCCESS:
			ccprintf("%d Baro init successful\n", val);
			break;
		default:
			ccprintf("Baro initialization error %d\n", rv);
		}
	}

	return EC_SUCCESS;
}


static int command_baro_read(int argc, char **argv)
{
	int i, rv, val;

	for (i = 0; i < BARO_COUNT; i++) {
		ccprintf("%s: ", baro[i].name);
		rv = get_pressure(i, &val);
		switch (rv) {
		case EC_SUCCESS:
			ccprintf("\nCompensated pressure = %dPa\n", val);
			break;
		default:
			ccprintf("Error %d\n", rv);
		}
	}

	return EC_SUCCESS;
}

static int command_baro_set_work_mode(int argc, char **argv)
{
	int i, rv, val;

	if (argc < 2) {
		CPRINTF("Err. Need work mode value. Please use baro_swm [0-4]");
		return -1;
	}

	for (i = 0; i < BARO_COUNT; i++) {
		ccprintf("%s: ", baro[i].name);

		val = *argv[1];
		rv = baro_set_work_mode(i, val);
		switch (rv) {
		case EC_SUCCESS:
			ccprintf("\nWork mode set successfully\n");
			break;
		default:
			ccprintf("Error setting work mode%d\n", rv);
		}
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(baro_init, command_baro_init,
			NULL,
			"Init barometer",
			NULL);

DECLARE_CONSOLE_COMMAND(baro_read, command_baro_read,
			NULL,
			"Print pressure value",
			NULL);

DECLARE_CONSOLE_COMMAND(baro_swm, command_baro_set_work_mode,
			NULL,
			"Set barometer work mode",
			NULL);

#endif
