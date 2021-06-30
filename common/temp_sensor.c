/* Copyright 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Temperature sensor module for Chrome EC */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "host_command.h"
#include "math_util.h"
#include "task.h"
#include "temp_sensor.h"
#include "thermal.h"
#include "timer.h"
#include "util.h"

#ifdef CONFIG_ZEPHYR
#include "temp_sensor/temp_sensor.h"
#endif

int temp_sensor_read_k(enum temp_sensor_id id, int *temp_k_ptr)
{
	const struct temp_sensor_t *sensor;

	if (id < 0 || id >= TEMP_SENSOR_COUNT)
		return EC_ERROR_INVAL;
	sensor = temp_sensors + id;

	/* Prefer read_mk if implemented */
	if (sensor->read_mk) {
		int temp_mk, rv;

		rv = sensor->read_mk(sensor->idx, &temp_mk);
		*temp_k_ptr = MILLI_KELVIN_TO_KELVIN(temp_mk);
		return rv;
	}

	return sensor->read_k(sensor->idx, temp_k_ptr);
}

int temp_sensor_read_mk(enum temp_sensor_id id, int *temp_mk_ptr)
{
	const struct temp_sensor_t *sensor;

	if (id < 0 || id >= TEMP_SENSOR_COUNT)
		return EC_ERROR_INVAL;
	sensor = temp_sensors + id;

	if (!sensor->read_mk)
		return EC_ERROR_UNIMPLEMENTED;

	return sensor->read_mk(sensor->idx, temp_mk_ptr);
}

static void update_mapped_memory(void)
{
	int i, t;
	uint8_t *mptr = host_get_memmap(EC_MEMMAP_TEMP_SENSOR);

	for (i = 0; i < TEMP_SENSOR_COUNT; i++, mptr++) {
		/*
		 * Switch to second range if first one is full, or stop if
		 * second range is also full.
		 */
		if (i == EC_TEMP_SENSOR_ENTRIES)
			mptr = host_get_memmap(EC_MEMMAP_TEMP_SENSOR_B);
		else if (i >= EC_TEMP_SENSOR_ENTRIES +
			 EC_TEMP_SENSOR_B_ENTRIES)
			break;

		/* TODO: Return millikelvin resolution */
		switch (temp_sensor_read_k(i, &t)) {
		case EC_ERROR_NOT_POWERED:
			*mptr = EC_TEMP_SENSOR_NOT_POWERED;
			break;
		case EC_ERROR_NOT_CALIBRATED:
			*mptr = EC_TEMP_SENSOR_NOT_CALIBRATED;
			break;
		case EC_SUCCESS:
			*mptr = t - EC_TEMP_SENSOR_OFFSET;
			break;
		default:
			*mptr = EC_TEMP_SENSOR_ERROR;
		}
	}
}
/* Run after other TEMP tasks, so sensors will have updated first. */
DECLARE_HOOK(HOOK_SECOND, update_mapped_memory, HOOK_PRIO_TEMP_SENSOR_DONE);

static void temp_sensor_init(void)
{
	int i;
	uint8_t *base, *base_b;

	/*
	 * Initialize memory-mapped data so that if a temperature value is read
	 * before we actually poll the sensors, we don't return an impossible
	 * or out-of-range value.
	 */
	base = host_get_memmap(EC_MEMMAP_TEMP_SENSOR);
	base_b = host_get_memmap(EC_MEMMAP_TEMP_SENSOR_B);
	for (i = 0; i < TEMP_SENSOR_COUNT; ++i) {
		if (i < EC_TEMP_SENSOR_ENTRIES)
			base[i] = EC_TEMP_SENSOR_DEFAULT;
		else
			base_b[i - EC_TEMP_SENSOR_ENTRIES] =
				EC_TEMP_SENSOR_DEFAULT;
	}

	/* Set the rest of memory region to SENSOR_NOT_PRESENT */
	for (; i < EC_TEMP_SENSOR_ENTRIES + EC_TEMP_SENSOR_B_ENTRIES; ++i) {
		if (i < EC_TEMP_SENSOR_ENTRIES)
			base[i] = EC_TEMP_SENSOR_NOT_PRESENT;
		else
			base_b[i - EC_TEMP_SENSOR_ENTRIES] =
				EC_TEMP_SENSOR_NOT_PRESENT;
	}

	/* Temp sensor data is present, with B range supported. */
	*host_get_memmap(EC_MEMMAP_THERMAL_VERSION) = 2;
}
DECLARE_HOOK(HOOK_INIT, temp_sensor_init, HOOK_PRIO_DEFAULT);

/*****************************************************************************/
/* Console commands */

#ifdef CONFIG_CMD_TEMP_SENSOR
int console_command_temps(int argc, char **argv)
{
	int temp_mk, temp_mc, i;
	int rv = EC_SUCCESS;

	for (i = 0; i < TEMP_SENSOR_COUNT; ++i) {
		ccprintf("  %-20s: ", temp_sensors[i].name);
		/* Prefer millikelvin resolution if implemented */
		rv = temp_sensor_read_mk(i, &temp_mk);
		if (rv) {
			int temp_k;

			rv = temp_sensor_read_k(i, &temp_k);
			temp_mk = KELVIN_TO_MILLI_KELVIN(temp_k);
		}
		temp_mc = MILLI_KELVIN_TO_MILLI_CELSIUS(temp_mk);

		switch (rv) {
		case EC_SUCCESS:
			ccprintf("%3d.%03d K = %3d.%03d C",
				 temp_mk/1000, temp_mk%1000,
				 temp_mc/1000, ABS(temp_mc)%1000);
#ifdef CONFIG_THROTTLE_AP
			if (thermal_params[i].temp_fan_off &&
			    thermal_params[i].temp_fan_max)
				ccprintf("  %d%%",
					 thermal_fan_percent(
						 thermal_params[i].temp_fan_off,
						 thermal_params[i].temp_fan_max,
						 MILLI_KELVIN_TO_KELVIN(
							 temp_mk)));
#endif
			ccprintf("\n");
			break;
		case EC_ERROR_NOT_POWERED:
			ccprintf("Not powered\n");
			break;
		case EC_ERROR_NOT_CALIBRATED:
			ccprintf("Not calibrated\n");
			break;
		default:
			ccprintf("Error %d\n", rv);
		}
	}

	return rv;
}
DECLARE_CONSOLE_COMMAND(temps, console_command_temps,
			NULL,
			"Print temp sensors");
#endif

/*****************************************************************************/
/* Host commands */

enum ec_status temp_sensor_command_get_info(struct host_cmd_handler_args *args)
{
	const struct ec_params_temp_sensor_get_info *p = args->params;
	struct ec_response_temp_sensor_get_info *r = args->response;
	int id = p->id;

	if (id >= TEMP_SENSOR_COUNT)
		return EC_RES_ERROR;

	strzcpy(r->sensor_name, temp_sensors[id].name, sizeof(r->sensor_name));
	r->sensor_type = temp_sensors[id].type;

	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_TEMP_SENSOR_GET_INFO,
		     temp_sensor_command_get_info,
		     EC_VER_MASK(0));
