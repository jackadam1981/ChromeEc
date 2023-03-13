/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* NCT7715 temperature sensor module for Chrome EC */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "i2c.h"
#include "math_util.h"
#include "nct7715.h"
#include "util.h"

#ifdef CONFIG_ZEPHYR
#include "temp_sensor/temp_sensor.h"
#endif

#define NCT7715_RESOLUTION 12
#define NCT7715_SHIFT1 (16 - NCT7715_RESOLUTION)
#define NCT7715_SHIFT2 (NCT7715_RESOLUTION - 8)

#define CPRINTS(format, args...) cprints(CC_THERMAL, format, ##args)

static int temp_mk_local[NCT7715_COUNT];

static int raw_read16(int sensor, const int offset, int *data_ptr)
{
#ifdef CONFIG_I2C_BUS_MAY_BE_UNPOWERED
	/*
	 * Don't try to read if the port is unpowered
	 */
	if (!board_is_i2c_port_powered(nct7715_sensors[sensor].i2c_port))
		return EC_ERROR_NOT_POWERED;
#endif
	return i2c_read16(nct7715_sensors[sensor].i2c_port,
			  nct7715_sensors[sensor].i2c_addr_flags, offset,
			  data_ptr);
}

static int get_reg_temp(int sensor, int *temp_ptr)
{
	int temp_raw = 0;

	RETURN_ERROR(raw_read16(sensor, nct7715_REG_TEMP, &temp_raw));

	*temp_ptr = (int)(int16_t)temp_raw;
	return EC_SUCCESS;
}

static inline int nct7715_reg_to_mk(int16_t reg)
{
	int temp_mc;

	temp_mc = (((reg >> NCT7715_SHIFT1) * 1000) >> NCT7715_SHIFT2);

	return MILLI_CELSIUS_TO_MILLI_KELVIN(temp_mc);
}

int nct7715_get_val_k(int idx, int *temp_k_ptr)
{
	if (idx >= NCT7715_COUNT)
		return EC_ERROR_INVAL;

	*temp_k_ptr = MILLI_KELVIN_TO_KELVIN(temp_mk_local[idx]);
	return EC_SUCCESS;
}

int nct7715_get_val_mk(int idx, int *temp_mk_ptr)
{
	if (idx >= NCT7715_COUNT)
		return EC_ERROR_INVAL;

	*temp_mk_ptr = temp_mk_local[idx];
	return EC_SUCCESS;
}

#ifndef CONFIG_ZEPHYR
static void nct7715_poll(void)
{
	int s;
	int temp_reg = 0;

	for (s = 0; s < NCT7715_COUNT; s++) {
		if (get_reg_temp(s, &temp_reg) == EC_SUCCESS)
			temp_mk_local[s] = NCT7715_reg_to_mk(temp_reg);
	}
}
DECLARE_HOOK(HOOK_SECOND, nct7715_poll, HOOK_PRIO_TEMP_SENSOR);
#else
void nct7715_update_temperature(int idx)
{
	int temp_reg = 0;

	if (idx >= NCT7715_COUNT)
		return;

	if (get_reg_temp(idx, &temp_reg) == EC_SUCCESS)
		temp_mk_local[idx] = nct7715_reg_to_mk(temp_reg);
}
#endif /* CONFIG_ZEPHYR */

void nct7715_init(void)
{
	/* Incase we need to initialize somthing */
}
DECLARE_HOOK(HOOK_INIT, nct7715_init, HOOK_PRIO_DEFAULT);
