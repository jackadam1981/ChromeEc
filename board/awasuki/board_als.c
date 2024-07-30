/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery_fuel_gauge.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "system.h"
#include "time.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, "ALS " format, ##args)

#define EEPROM_PAGE_WRITE_MS 5
#define I2C_ADDR_ALS_FLAGS 0x50
#define I2C_PORT_ALS IT83XX_I2C_CH_E

static int als_eeprom_read(void)
{
	uint8_t p[4];
	int rv;

	rv = i2c_read_block(I2C_PORT_ALS, I2C_ADDR_ALS_FLAGS, 0, p, sizeof(p));
	if (rv) {
		CPRINTS("Failed to read for %d", rv);
		return rv;
	}

	return (p[3] << 24 | p[2] << 16 | p[1] << 8 | p[0]);
}

static int als_eeprom_write(int data)
{
	uint8_t p[4];
	int rv;

	p[0] = data & 0xFF;
	p[1] = (data >> 8) & 0xFF;
	p[2] = (data >> 16) & 0xFF;
	p[3] = (data >> 24) & 0xFF;

	rv = i2c_write_block(I2C_PORT_ALS, I2C_ADDR_ALS_FLAGS, 0, p, sizeof(p));
	if (rv) {
		CPRINTS("Failed to write for %d", rv);
		return rv;
	}
	/* Wait for internal write cycle completion */
	crec_msleep(EEPROM_PAGE_WRITE_MS);

	return EC_SUCCESS;
}

static void als_data_handler(void)
{
	int als_data;

	als_data = als_eeprom_read();
	als_data++;
	als_eeprom_write(als_data);

	CPRINTS("data %d", als_data);
}

static bool als_det_enable = 1;
static void als_change_deferred(void)
{
	int out;

	out = gpio_get_level(GPIO_DOOR_OPEN_EC);

	if (out == 0) {
		ccprints("----als pin low! shutdown!");
		als_data_handler();
		chipset_force_shutdown(CHIPSET_SHUTDOWN_BOARD_CUSTOM);
		ccprints("----cut off!");
		cflush();
		board_cut_off_battery();
		als_det_enable = 0;
	}
}
DECLARE_DEFERRED(als_change_deferred);

static void check_als(void)
{
	if (als_det_enable) {
		hook_call_deferred(&als_change_deferred_data, 0);
	}
}
DECLARE_HOOK(HOOK_INIT, check_als, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_SECOND, check_als, HOOK_PRIO_DEFAULT);

void als_interrupt(enum gpio_signal s)
{
	/* Reset als debounce time */
	hook_call_deferred(&als_change_deferred_data, 30 * MSEC);
}

static int command_als(int argc, const char **argv)
{
	int count = 0;
	char *e;

	if (argc > 1) {
		count = strtoi(argv[1], &e, 0);
		if (*e || count < 0)
			return EC_ERROR_PARAM1;
	}

	if (argc == 1) {
		count = als_eeprom_read();
		CPRINTS("als read count %d", count);
		return EC_SUCCESS;
	}

	als_eeprom_write(count);
	count = als_eeprom_read();
	CPRINTS("als set count %d", count);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(alsdata, command_als, "[count]", "als cmd read/write");
