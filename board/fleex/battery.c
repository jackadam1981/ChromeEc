/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack vendor provided charging profile
 */

#include "battery.h"
#include "battery_fuel_gauge.h"
#include "battery_smart.h"
#include "console.h"
#include "common.h"
#include "extpower.h"
#include "hooks.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ## args)
#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ## args)

/*
 * Battery info for all fleex battery types. Note that the fields
 * start_charging_min/max and charging_min/max are not used for the charger.
 * The effective temperature limits are given by discharging_min/max_c.
 *
 * Fuel Gauge (FG) parameters which are used for determining if the battery
 * is connected, the appropriate ship mode (battery cutoff) command, and the
 * charge/discharge FETs status.
 *
 * Ship mode (battery cutoff) requires 2 writes to the appropriate smart battery
 * register. For some batteries, the charge/discharge FET bits are set when
 * charging/discharging is active, in other types, these bits set mean that
 * charging/discharging is disabled. Therefore, in addition to the mask for
 * these bits, a disconnect value must be specified. Note that for TI fuel
 * gauge, the charge/discharge FET status is found in Operation Status (0x54),
 * but a read of Manufacturer Access (0x00) will return the lower 16 bits of
 * Operation status which contains the FET status bits.
 *
 * The assumption for battery types supported is that the charge/discharge FET
 * status can be read with a sb_read() command and therefore, only the register
 * address, mask, and disconnect value need to be provided.
 */
const struct board_batt_params board_battery_info[] = {
	/* BYD Battery Information */
	[BATTERY_BYD] = {
		.fuel_gauge = {
			.manuf_name = "BYD",
			.ship_mode = {
				.reg_addr = 0x44,
				.reg_data = { 0x0010, 0x0010 },
			},
		},
		.batt_info = {
			.voltage_max		= 13200, /* mV */
			.voltage_normal		= 11400, /* mV */
			.voltage_min		= 9000, /* mV */
			.precharge_current	= 256,	/* mA */
			.start_charging_min_c	= 0,
			.start_charging_max_c	= 50,
			.charging_min_c		= 0,
			.charging_max_c		= 60,
			.discharging_min_c	= -20,
			.discharging_max_c	= 70,
		},
	},

	/* LGC Battery Information */
	[BATTERY_LGC] = {
		.fuel_gauge = {
			.manuf_name = "LGC-LGC3.553",
			.ship_mode = {
				.reg_addr = 0x44,
				.reg_data = { 0x0010, 0x0010 },
			},
		},
		.batt_info = {
			.voltage_max		= 13200, /* mV */
			.voltage_normal		= 11400, /* mV */
			.voltage_min		= 9000, /* mV */
			.precharge_current	= 256,	/* mA */
			.start_charging_min_c	= 0,
			.start_charging_max_c	= 50,
			.charging_min_c		= 0,
			.charging_max_c		= 60,
			.discharging_min_c	= -20,
			.discharging_max_c	= 70,
		},
	},

	/* SIMPLO Battery Information */
	[BATTERY_SIMPLO] = {
		.fuel_gauge = {
			.manuf_name = "SMP-SDI3.72",
			.ship_mode = {
				.reg_addr = 0x0,
				.reg_data = { 0x0010, 0x0010 },
			},
		},
		.batt_info = {
			.voltage_max		= 13200, /* mV */
			.voltage_normal		= 11400, /* mV */
			.voltage_min		= 9000, /* mV */
			.precharge_current	= 256,	/* mA */
			.start_charging_min_c	= 0,
			.start_charging_max_c	= 50,
			.charging_min_c		= 0,
			.charging_max_c		= 60,
			.discharging_min_c	= -20,
			.discharging_max_c	= 70,
		},
	},
};
BUILD_ASSERT(ARRAY_SIZE(board_battery_info) == BATTERY_TYPE_COUNT);

const enum battery_type DEFAULT_BATTERY_TYPE = BATTERY_LGC;

enum gauge_type {
	GAUGE_TYPE_UNKNOWN = 0,
	GAUGE_TYPE_TI_BQ40Z50,
	GAUGE_TYPE_RENESAS_RAJ240,
};
static enum gauge_type fuel_gauge;

/* Get type of the battery connected on the board */
static int get_battery_type(void)
{
	char manuf_name[32], device_name[32];
	int i;
	static enum battery_type battery_type = BATTERY_TYPE_COUNT;

	/*
	 * If battery_type is not the default value, then can return here
	 * as there is no need to query the fuel gauge.
	 */
	if (battery_type != BATTERY_TYPE_COUNT)
		return battery_type;

	/* Get the manufacturer name. If can't read then just exit */
	if (battery_manufacturer_name(manuf_name, sizeof(manuf_name)))
		return battery_type;

	/*
	 * Compare the manufacturer name read from the fuel gauge to the
	 * manufacturer names defined in the board_battery_info table. If
	 * a device name has been specified in the board_battery_info table,
	 * then both the manufacturer and device name must match.
	 */
	for (i = 0; i < BATTERY_TYPE_COUNT; i++) {
		const struct fuel_gauge_info * const fuel_gauge =
			&board_battery_info[i].fuel_gauge;

		if (strcasecmp(manuf_name, fuel_gauge->manuf_name))
			continue;

		if (fuel_gauge->device_name != NULL) {

			if (battery_device_name(device_name,
						sizeof(device_name)))
				continue;

			if (strcasecmp(device_name, fuel_gauge->device_name))
				continue;
		}

		CPRINTS("found batt:%s", fuel_gauge->manuf_name);
		battery_type = i;
		break;
	}

	return battery_type;
}

/*
 * Initialize the battery type for the board.
 *
 * The first call to battery_get_info() is when the charger task starts, so
 * initialize the battery type as soon as I2C is initialized.
 */
static void init_battery_type(void)
{
	if (get_battery_type() == BATTERY_TYPE_COUNT)
		CPRINTS("battery not found");
}
DECLARE_HOOK(HOOK_INIT, init_battery_type, HOOK_PRIO_INIT_I2C + 1);

static inline const struct board_batt_params *get_batt_params(void)
{
	int type = get_battery_type();

	return &board_battery_info[type == BATTERY_TYPE_COUNT ?
		DEFAULT_BATTERY_TYPE : type];
}

const struct battery_info *battery_get_info(void)
{
	return &get_batt_params()->batt_info;
}

int board_cut_off_battery(void)
{
	int rv;
	int cmd;
	int data;
	int type = get_battery_type();

	/* If battery type is unknown can't send ship mode command */
	if (type == BATTERY_TYPE_COUNT)
		return EC_RES_ERROR;

	/* Ship mode command requires writing 2 data values */
	cmd = board_battery_info[type].fuel_gauge.ship_mode.reg_addr;
	data = board_battery_info[type].fuel_gauge.ship_mode.reg_data[0];
	rv = sb_write(cmd, data);
	if (rv != EC_SUCCESS)
		return EC_RES_ERROR;

	data = board_battery_info[type].fuel_gauge.ship_mode.reg_data[1];
	rv = sb_write(cmd, data);

	return rv ? EC_RES_ERROR : EC_RES_SUCCESS;
}

/*
 * Read a value from the Manufacturer Access System (MAC).
 */
static int sb_get_mac(uint16_t cmd, uint8_t *data, int len)
{
	int rv;

	rv = sb_write(SB_MANUFACTURER_ACCESS, cmd);
	if (rv)
		return rv;

	return sb_read_string(SB_MANUFACTURER_DATA, data, len);
}

static enum gauge_type get_gauge_ic(void)
{
	uint8_t data[11];

	/* 0x0002 is for 'Firmware Version' (p91 in BQ40Z50-R2 TRM).
	 * We can't use sb_read_mfgacc because the command won't be included
	 * in the returned block. */
	if (sb_get_mac(0x0002, data, sizeof(data)))
		return GAUGE_TYPE_UNKNOWN;

	/* BQ40Z50 returns FW version: 1.06 for BYD/ 1.07 for LGC */
	if (data[2] == 0x01 && (data[3] == 0x06 || data[3] == 0x07))
		return GAUGE_TYPE_TI_BQ40Z50;
	else
		return GAUGE_TYPE_RENESAS_RAJ240;
}

static int battery_check_disconnect_ti_bq40z50(void)
{
	int rv;
	uint8_t data[6];

	/* Check if battery charging + discharging is disabled. */
	rv = sb_read_mfgacc(PARAM_OPERATION_STATUS,
			    SB_ALT_MANUFACTURER_ACCESS, data, sizeof(data));
	if (rv)
		return BATTERY_DISCONNECT_ERROR;

	if ((data[3] & (BATTERY_DISCHARGING_DISABLED |
			BATTERY_CHARGING_DISABLED)) ==
	    (BATTERY_DISCHARGING_DISABLED | BATTERY_CHARGING_DISABLED)) {
		return BATTERY_DISCONNECTED;
	}

	return BATTERY_NOT_DISCONNECTED;
}

static int battery_check_disconnect_renesas_raj240(void)
{
	int data;
	int rv;

	rv = sb_read(0x41, &data);
	if (rv)
		return BATTERY_DISCONNECT_ERROR;

	if (data != 0x1E /* 1E: Power down */)
		return BATTERY_NOT_DISCONNECTED;

	return BATTERY_DISCONNECTED;
}

/*
 * This function checks the charge/discharge FET status bits. Each battery type
 * supported provides the register address, mask, and disconnect value for these
 * 2 FET status bits. If the FET status matches the disconnected value, then
 * BATTERY_DISCONNECTED is returned. This function is required to handle the
 * cases when the fuel gauge is awake and will return a non-zero state of
 * charge, but is not able yet to provide power (i.e. discharge FET is not
 * active). By returning BATTERY_DISCONNECTED the AP will not be powered up
 * until either the external charger is able to provided enough power, or
 * the battery is able to provide power and thus prevent a brownout when the
 * AP is powered on by the EC.
 */
enum battery_disconnect_state battery_get_disconnect_state(void)
{
	if (fuel_gauge == GAUGE_TYPE_UNKNOWN) {
		fuel_gauge = get_gauge_ic();
		CPRINTS("fuel_gauge=%d\n", fuel_gauge);
	}

	switch (fuel_gauge) {
	case GAUGE_TYPE_TI_BQ40Z50:
		return battery_check_disconnect_ti_bq40z50();
	case GAUGE_TYPE_RENESAS_RAJ240:
		return battery_check_disconnect_renesas_raj240();
	default:
		return BATTERY_DISCONNECT_ERROR;
	}
}
