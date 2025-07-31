/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* APL6012 temperature sensor module for Chrome EC */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "i2c.h"
#include "math_util.h"
#include "temp_sensor/apl6012.h"
#include "util.h"

#ifdef CONFIG_ZEPHYR
#include "temp_sensor/temp_sensor.h"
#endif

static int temps[APL6012_IDX_COUNT];
static int8_t fake_temp[APL6012_IDX_COUNT];

#ifndef CONFIG_ZEPHYR
/**
 * Read 8 bits register from temp sensor.
 */
static int raw_read8(const int offset, int *data)
{
	return i2c_read8(I2C_PORT_THERMAL, APL6012_I2C_ADDR_FLAGS, offset, data);
}
#else
/**
 * Read 8 bits register from temp sensor.
 */
static int raw_read8(int sensor, const int offset, int *data)
{
	return i2c_read8(apl6012_sensors[sensor].i2c_port,
			 apl6012_sensors[sensor].i2c_addr_flags, offset, data);
}
#endif /* !CONFIG_ZEPHYR */

#ifndef CONFIG_ZEPHYR
static int get_temp(const int offset, int *temp)
{
	int rv;
	int temp_raw = 0;

	rv = raw_read8(offset, &temp_raw);
	if (rv != 0)
		return rv;

	*temp = C_TO_K(temp_raw);
	return EC_SUCCESS;
}
#else
static int get_temp(int sensor, const int offset, int *temp)
{
	int rv;
	int temp_raw = 0;

	rv = raw_read8(sensor, offset, &temp_raw);
	if (rv != 0)
		return rv;

	// *temp = CELSIUS_TO_MILLI_KELVIN(temp_raw);
    *temp = temp_raw;
	return EC_SUCCESS;
}
#endif /* !CONFIG_ZEPHYR */

int apl6012_get_val(int idx, int *temp)
{
	if (idx < 0 || APL6012_IDX_COUNT <= idx)
		return EC_ERROR_INVAL;

	if (fake_temp[idx] != -1) {
		*temp = C_TO_K(fake_temp[idx]);
		return EC_SUCCESS;
	}

	*temp = temps[idx];
	return EC_SUCCESS;
}

static inline int apl6012_reg_to_mk(int16_t reg)
{
	// int temp_mc;

	// temp_mc = (((reg >> F75303_SHIFT1) * 1000) >> F75303_SHIFT2);

	// return MILLI_CELSIUS_TO_MILLI_KELVIN(temp_mc);

	return reg;
}

int apl6012_get_val_k(int idx, int *temp_k_ptr)
{
	if (idx >= APL6012_IDX_COUNT)
		return EC_ERROR_INVAL;

	// *temp_k_ptr = MILLI_KELVIN_TO_KELVIN(temps[idx]);
    *temp_k_ptr = temps[idx];
	return EC_SUCCESS;
}

int apl6012_get_val_mk(int idx, int *temp_mk_ptr)
{
	if (idx >= APL6012_IDX_COUNT)
		return EC_ERROR_INVAL;

	*temp_mk_ptr = temps[idx];
	return EC_SUCCESS;
}

#ifndef CONFIG_ZEPHYR
static void apl6012_sensor_poll(void)
{
	int index;

	for (index = 0; index < APL6012_IDX_COUNT; index++) {
		get_temp((APL6012_TD1 + index), &temps[(APL6012_IDX_CH1 + index)]);
	}
}
DECLARE_HOOK(HOOK_SECOND, apl6012_sensor_poll, HOOK_PRIO_TEMP_SENSOR);
#else
void apl6012_update_temperature(int idx)
{
	int temp_reg = 0;
	int rv;

	if (idx >= APL6012_IDX_COUNT)
		return;

	rv = get_temp(idx, (APL6012_TD1 + idx), &temp_reg);

	if (rv == EC_SUCCESS) {
		temps[idx] = temp_reg;
	}
}
#endif /* CONFIG_ZEPHYR */

static int apl6012_set_fake_temp(int argc, const char **argv)
{
	int index;
	int value;
	char *e;

	if (argc != 3)
		return EC_ERROR_PARAM_COUNT;

	index = strtoi(argv[1], &e, 0);
	if ((*e) || (index < 0) || (index >= APL6012_IDX_COUNT))
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
	ccprintf("Force sensor %u = %uC.\n", index, value);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(apl6012, apl6012_set_fake_temp, "<index> <value>|off",
			"Set fake temperature of sensor apl6012.");

static void apl6012_init(void)
{
	int index;

	for (index = 0; index < APL6012_IDX_COUNT; index++)
		fake_temp[index] = -1;
}
DECLARE_HOOK(HOOK_INIT, apl6012_init, HOOK_PRIO_TEMP_SENSOR);
