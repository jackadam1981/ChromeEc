
/*
 *
 * Sensor driver implemnetation for BMP280 for Chrome EC 
 *
 */

#include "barometer.h"
#include "console.h"
#include "stddef.h"

#define BARO_COUNT 1

#define CPRINTF(format, args...) cprintf(CC_BARO, format, ## args)
#define CPRINTS(format, args...) cprints(CC_BARO, format, ## args)
int get_pressure(int id, int *pressure)
{
	return baro[id].read(pressure);
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
			ccprintf("%d pressure\n", val);
			break;
		default:
			ccprintf("Error %d\n", rv);
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
			ccprintf("%d pressure\n", val);
			break;
		default:
			ccprintf("Error %d\n", rv);
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

#endif
