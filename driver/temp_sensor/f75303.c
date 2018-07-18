/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* F75303 temperature sensor module for Chrome EC */

#include "common.h"
#include "f75303.h"
#include "i2c.h"
#include "hooks.h"
#include "util.h"
#include "console.h"

static int temp_val_local;
static int temp_val_remote1;
static int temp_val_remote2;
static int fake_temp[F75303_IDX_COUNT] = {-1, -1, -1};

/**
 * Read 8 bits register from temp sensor.
 */
static int raw_read8(const int offset, int *data_ptr)
{
	return i2c_read8(I2C_PORT_THERMAL, F75303_I2C_ADDR, offset, data_ptr);
}

static int get_temp(const int offset, int *temp_ptr)
{
	int rv;
	int temp_raw = 0;

	rv = raw_read8(offset, &temp_raw);
	if (rv != 0)
		return rv;

	*temp_ptr = temp_raw;
	return EC_SUCCESS;
}

int f75303_get_val(int idx, int *temp_ptr)
{
	switch (idx) {
	case F75303_IDX_LOCAL:
		*temp_ptr = temp_val_local;
		break;
	case F75303_IDX_REMOTE1:
		*temp_ptr = temp_val_remote1;
		break;
	case F75303_IDX_REMOTE2:
		*temp_ptr = temp_val_remote2;
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}

	return EC_SUCCESS;
}

static void f75303_sensor_poll(void)
{
	if (fake_temp[F75303_IDX_LOCAL] != -1) {
		temp_val_local = C_TO_K(fake_temp[F75303_IDX_LOCAL]);
	} else {
		get_temp(F75303_TEMP_LOCAL, &temp_val_local);
		temp_val_local = C_TO_K(temp_val_local);
	}

	if (fake_temp[F75303_IDX_REMOTE1] != -1) {
		temp_val_remote1 = C_TO_K(fake_temp[F75303_IDX_REMOTE1]);
	} else {
		get_temp(F75303_TEMP_REMOTE1, &temp_val_remote1);
		temp_val_remote1 = C_TO_K(temp_val_remote1);
	}

	if (fake_temp[F75303_IDX_REMOTE2] != -1) {
		temp_val_remote2 = C_TO_K(fake_temp[F75303_IDX_REMOTE2]);
	} else {
		get_temp(F75303_TEMP_REMOTE2, &temp_val_remote2);
		temp_val_remote2 = C_TO_K(temp_val_remote2);
	}
}
DECLARE_HOOK(HOOK_SECOND, f75303_sensor_poll, HOOK_PRIO_TEMP_SENSOR);

static int f75303_set_fake_temp(int argc, char **argv)
{
	int index;
	int value;
	char *e;

	if (argc != 3)
		return EC_ERROR_PARAM_COUNT;

	index = strtoi(argv[1], &e, 0);
	if ((*e) || (index < 0) || (index >= F75303_IDX_COUNT))
		return EC_ERROR_PARAM1;

	if (!strcasecmp(argv[2], "off")) {
		fake_temp[index] = -1;
		ccprintf("Turn off fake temp mode for sensor %u.\n", index);
		return EC_SUCCESS;
	}

	value = strtoi(argv[2], &e, 0);

	if ((*e) || (value < 0) || (value > 100))
		return EC_ERROR_PARAM2;

	fake_temp[index] = value;
	ccprintf("Set fake temp %uC for sensor %u.\n", value, index);
	ccprintf("New temp will be updated 1 sec later.\n\n");

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(f75303_fake, f75303_set_fake_temp,
		"[<index>] [<value>|off]",
		"Set fake temperature of sensor f75303.");

