#include "temp_sensor.h"
#include "console.h"
#include "board.h"
#include "util.h"

static int temp_val[TEMP_SENSOR_COUNT];

int temp_sensor_powered(enum temp_sensor_id id)
{
	return 1;
}

int temp_sensor_read(enum temp_sensor_id id)
{
	return temp_val[id];
}

const struct temp_sensor_t temp_sensors[TEMP_SENSOR_COUNT] = {
	{"MOCK0", TEMP_SENSOR_POWER_NONE, TEMP_SENSOR_TYPE_CPU,
	 0, 0},
	{"MOCK1", TEMP_SENSOR_POWER_NONE, TEMP_SENSOR_TYPE_CASE,
	 0, 1},
};

static int command_set_temp(int argc, char **argv)
{
	char *e;
	int id, t;

	id = strtoi(argv[1], &e, 0);
	t = strtoi(argv[2], &e, 0);
	temp_val[id] = t;
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(settemp, command_set_temp,
			"id temperature",
			"Set mock temperature value",
			NULL);

