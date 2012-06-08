/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Mock up temperature sensor */

#include "temp_sensor.h"
#include "console.h"
#include "board.h"
#include "util.h"


static int temp_val[TEMP_SENSOR_COUNT];


int temp_sensor_powered(enum temp_sensor_id id)
{
	/* Always powered. */
	return 1;
}


int temp_sensor_read(enum temp_sensor_id id)
{
	return temp_val[id];
}


const struct temp_sensor_t temp_sensors[TEMP_SENSOR_COUNT] = {
	{"MOCK_CPU", TEMP_SENSOR_POWER_NONE, TEMP_SENSOR_TYPE_CPU,
	 0, 0},
	{"MOCK_BOARD", TEMP_SENSOR_POWER_NONE, TEMP_SENSOR_TYPE_BOARD,
	 0, 1},
	{"MOCK_CASE", TEMP_SENSOR_POWER_NONE, TEMP_SENSOR_TYPE_CASE,
	 0, 2},
};


static int command_set_cpu_temp(int argc, char **argv)
{
	char *e;
	int t;

	t = strtoi(argv[1], &e, 0);
	temp_val[TEMP_SENSOR_MOCK_CPU] = t;
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(setcputemp, command_set_cpu_temp,
			"temperature",
			"Set mock CPU temperature value",
			NULL);


static int command_set_board_temp(int argc, char **argv)
{
	char *e;
	int t;

	t = strtoi(argv[1], &e, 0);
	temp_val[TEMP_SENSOR_MOCK_BOARD] = t;
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(setboardtemp, command_set_board_temp,
			"temperature",
			"Set mock board temperature value",
			NULL);


static int command_set_case_temp(int argc, char **argv)
{
	char *e;
	int t;

	t = strtoi(argv[1], &e, 0);
	temp_val[TEMP_SENSOR_MOCK_CASE] = t;
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(setcasetemp, command_set_case_temp,
			"temperature",
			"Set mock case temperature value",
			NULL);
