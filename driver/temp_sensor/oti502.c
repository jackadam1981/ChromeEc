/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* OTI502 temperature sensor module for Chrome EC */

#include "common.h"
#include "console.h"
#include "oti502.h"
#include "i2c.h"
#include "hooks.h"
#include "util.h"

static int temp_val_ambient;	/* Ambient is chip temperature*/
static int temp_val_object;		/* Object is IR temperature */

static int fake_temp[2] = {-1, -1};

static int oti502_read_block(const int offset, uint8_t *data, int len)
{
	return i2c_read_block(I2C_PORT_THERMAL, OTI502_I2C_ADDR_FLAGS,
			 offset, data, len);
}

int oti502_get_val(int idx, int *temp_ptr)
{
	switch (idx) {
	case OTI502_IDX_AMBIENT:

		if (fake_temp[idx] != -1)
			*temp_ptr = C_TO_K(fake_temp[idx]);
		else
			*temp_ptr = temp_val_ambient;
		break;
	case OTI502_IDX_OBJECT:
		if (fake_temp[idx] != -1)
			*temp_ptr = C_TO_K(fake_temp[idx]);
		else
			*temp_ptr = temp_val_object;
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}

	return EC_SUCCESS;
}

static void temp_sensor_poll(void)
{
	uint8_t temp_val[6];

	memset(temp_val, 0, sizeof(temp_val));

	oti502_read_block(0x80, temp_val, sizeof(temp_val));

	if (temp_val[2] >= 0x80) {
		/* Treat temperature as 0 degree C if temperature is negative*/
		temp_val_ambient = 0;
		ccprintf("Temperature ambient is negative !\n");
	} else {
		temp_val_ambient = ((temp_val[1] << 8) + temp_val[0]) / 200;
		temp_val_ambient = C_TO_K(temp_val_ambient);
	}

	if (temp_val[5] >= 0x80) {
		/* Treat temperature as 0 degree C if temperature is negative*/
		temp_val_object = 0;
		ccprintf("Temperature object is negative !\n");
	} else {
		temp_val_object = ((temp_val[4] << 8) + temp_val[5]) / 200;
		temp_val_object = C_TO_K(temp_val_object);
	}
}
DECLARE_HOOK(HOOK_SECOND, temp_sensor_poll, HOOK_PRIO_TEMP_SENSOR);

static int therm_set_fake_temp(int index, int degree_c)
{
	if ((index < 0) || (index >= 3))
		return EC_ERROR_INVAL;

	fake_temp[index] = degree_c;
	ccprintf("New degree will be updated 1 sec later\n\n");

	return EC_SUCCESS;
}

static int command_oti502(int argc, char **argv)
{
	char *command;
	char *e;
	int data;
	int offset;
	int rv = 0;

	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;

	command = argv[1];
	offset = strtoi(argv[2], &e, 0);
	if (*e || offset < 0 || offset > 255)
		return EC_ERROR_PARAM2;

	data = strtoi(argv[3], &e, 0);

	if (!strcasecmp(command, "fake")) {
		ccprintf("Hook temperature\n");
		rv = therm_set_fake_temp(offset, data);
	} else
		return EC_ERROR_PARAM1;

	return rv;
}
DECLARE_CONSOLE_COMMAND(oti502, command_oti502,
	"[fake <index> <value>]"
	"Temps in Celsius.",
	"Set fake temperature of thermistor");

